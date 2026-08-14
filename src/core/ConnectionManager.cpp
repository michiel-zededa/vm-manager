#include "ConnectionManager.h"
#include "MockBackend.h"

#ifdef HAVE_LIBVIRT
#include "LibvirtBackend.h"
#endif

#include <QMutexLocker>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QThread>
#include <QRunnable>

namespace vmm {

ConnectionManager::ConnectionManager(QObject *parent) : QObject(parent) {
    // Keep a few workers so several hosts can be queried concurrently, but
    // bound it so a burst of actions cannot spawn unbounded threads.
    m_pool.setMaxThreadCount(qMax(4, QThread::idealThreadCount()));
}

ConnectionManager::~ConnectionManager() {
    m_pool.waitForDone();
    QMutexLocker lock(&m_mutex);
    for (auto &b : m_backends) {
        if (b && b->isOpen())
            b->close();
    }
}

bool ConnectionManager::preferMock() {
    const auto env = QProcessEnvironment::systemEnvironment();
    return env.value("VMM_BACKEND").compare("mock", Qt::CaseInsensitive) == 0;
}

QString ConnectionManager::addConnection(const QString &uri, const QString &displayName) {
    QMutexLocker lock(&m_mutex);
    const QString id = uri;
    if (m_backends.contains(id))
        return id;

    std::shared_ptr<IHypervisorBackend> backend;
#ifdef HAVE_LIBVIRT
    if (!preferMock()) {
        backend = std::make_shared<LibvirtBackend>(id, displayName);
        m_usingReal = true;
    }
#endif
    if (!backend)
        backend = std::make_shared<MockBackend>(id, displayName);

    m_backends.insert(id, backend);
    return id;
}

void ConnectionManager::removeConnection(const QString &connId) {
    std::shared_ptr<IHypervisorBackend> backend;
    {
        QMutexLocker lock(&m_mutex);
        backend = m_backends.take(connId);
    }
    if (backend && backend->isOpen())
        backend->close();
}

QStringList ConnectionManager::connectionIds() const {
    QMutexLocker lock(&m_mutex);
    return m_backends.keys();
}

IHypervisorBackend *ConnectionManager::backendFor(const QString &connId) const {
    QMutexLocker lock(&m_mutex);
    auto it = m_backends.find(connId);
    return it == m_backends.end() ? nullptr : it.value().get();
}

bool ConnectionManager::hasBackend(const QString &connId) const {
    return backendFor(connId) != nullptr;
}

HostInfo ConnectionManager::hostInfo(const QString &connId) const {
    if (auto *b = backendFor(connId))
        return b->hostInfo();
    return {};
}

void ConnectionManager::openConnection(const QString &connId) {
    runAsync(connId,
        [](IHypervisorBackend &b) { b.open(); },
        [this, connId] { emit connectionStateChanged(connId, true, {}); },
        [this, connId](const QString &err) { emit connectionStateChanged(connId, false, err); });
}

void ConnectionManager::runAsync(const QString &connId, Job job,
                                 std::function<void()> onSuccess,
                                 std::function<void(QString)> onError) {
    // Hold a shared_ptr so the backend cannot be freed mid-job.
    std::shared_ptr<IHypervisorBackend> backend;
    {
        QMutexLocker lock(&m_mutex);
        backend = m_backends.value(connId);
    }
    if (!backend) {
        if (onError)
            QMetaObject::invokeMethod(this, [onError, connId] {
                onError(QStringLiteral("Unknown connection: %1").arg(connId));
            }, Qt::QueuedConnection);
        return;
    }

    m_pool.start(QRunnable::create([this, backend, job = std::move(job),
                  onSuccess = std::move(onSuccess), onError = std::move(onError)]() {
        QString error;
        try {
            job(*backend);
        } catch (const BackendError &e) {
            error = e.message();
        } catch (const std::exception &e) {
            error = QString::fromUtf8(e.what());
        } catch (...) {
            error = QStringLiteral("Unknown backend error");
        }

        // Marshal completion back to the GUI thread.
        QMetaObject::invokeMethod(this, [error, onSuccess, onError] {
            if (error.isEmpty()) {
                if (onSuccess) onSuccess();
            } else {
                if (onError) onError(error);
            }
        }, Qt::QueuedConnection);
    }));
}

} // namespace vmm
