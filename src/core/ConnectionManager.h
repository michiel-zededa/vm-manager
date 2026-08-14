#pragma once

#include "Backend.h"

#include <QObject>
#include <QHash>
#include <QThreadPool>
#include <QMutex>
#include <QStringList>
#include <memory>
#include <functional>

namespace vmm {

// Owns every hypervisor connection and guarantees that no backend call ever
// runs on the GUI thread. Callers submit a job via runAsync(); it executes on a
// worker and the success/error callbacks are marshalled back to the GUI thread.
class ConnectionManager : public QObject {
    Q_OBJECT
public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager() override;

    // Create + register a backend for a URI. Chooses the real libvirt backend
    // when available, otherwise the mock. Does not open it yet.
    QString addConnection(const QString &uri, const QString &displayName);

    // Open (connect) asynchronously; emits connectionStateChanged on completion.
    void openConnection(const QString &connId);
    void removeConnection(const QString &connId);

    QStringList connectionIds() const;
    HostInfo hostInfo(const QString &connId) const;
    bool hasBackend(const QString &connId) const;

    using Job = std::function<void(IHypervisorBackend &)>;

    // Run `job` on a worker thread against the backend for `connId`.
    // onSuccess/onError run on the GUI thread. Safe if the manager outlives the
    // job (it always does — backends are owned here).
    void runAsync(const QString &connId,
                  Job job,
                  std::function<void()> onSuccess = {},
                  std::function<void(QString)> onError = {});

    // True when at least one real (libvirt) backend was created.
    bool usingRealBackend() const { return m_usingReal; }

signals:
    void connectionStateChanged(const QString &connId, bool connected, const QString &error);

private:
    IHypervisorBackend *backendFor(const QString &connId) const;
    static bool preferMock();

    mutable QMutex m_mutex;
    QHash<QString, std::shared_ptr<IHypervisorBackend>> m_backends;
    QThreadPool m_pool;
    bool m_usingReal = false;
};

} // namespace vmm
