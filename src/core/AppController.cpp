#include "AppController.h"

#include "Backend.h"
#include "ConnectionManager.h"
#include "VmListModel.h"
#include "ConnectionModel.h"
#include "ImageImporter.h"
#include "SnapshotScheduler.h"

#include <QProcessEnvironment>
#include <QCoreApplication>
#include <memory>

namespace vmm {

namespace {
enum Level { Info = 0, Success = 1, Warning = 2, Error = 3 };

VmCreateRequest toRequest(const QVariantMap &m) {
    VmCreateRequest r;
    r.name = m.value("name").toString();
    r.osVariant = m.value("osVariant").toString();
    r.vcpus = m.value("vcpus", 2).toInt();
    r.memoryMiB = m.value("memoryMiB", 2048).toULongLong();
    r.diskGiB = m.value("diskGiB", 20).toULongLong();
    r.installMediaPath = m.value("installMediaPath").toString();
    r.networkName = m.value("networkName", "default").toString();
    r.firmware = m.value("firmware", "bios").toString();
    r.diskFormat = m.value("diskFormat", "qcow2").toString();
    return r;
}

QVariantMap snapshotToMap(const SnapshotInfo &s) {
    return {
        {"name", s.name}, {"parent", s.parent}, {"description", s.description},
        {"created", s.created}, {"hasMemory", s.hasMemory}, {"isCurrent", s.isCurrent},
    };
}
} // namespace

AppController::AppController(QObject *parent) : QObject(parent) {
    m_cm = new ConnectionManager(this);
    m_vms = new VmListModel(this);
    m_connections = new ConnectionModel(this);
    m_importer = new ImageImporter(this);
    m_scheduler = new SnapshotScheduler(this);

    connect(m_cm, &ConnectionManager::connectionStateChanged,
            this, &AppController::onConnectionStateChanged);

    // The scheduler asks us to take a snapshot when a policy fires.
    connect(m_scheduler, &SnapshotScheduler::snapshotDue, this,
            [this](const QString &connId, const QString &uuid, const QString &name) {
                takeSnapshot(connId, uuid, name, QStringLiteral("Scheduled snapshot"), false);
            });

    m_statsTimer.setInterval(2000);
    connect(&m_statsTimer, &QTimer::timeout, this, &AppController::statsPoll);

    m_fullRefreshTimer.setInterval(15000);
    connect(&m_fullRefreshTimer, &QTimer::timeout, this, &AppController::refresh);
}

AppController::~AppController() = default;

QString AppController::appVersion() const {
    return QCoreApplication::applicationVersion();
}

bool AppController::usingMockBackend() const {
    return !m_cm->usingRealBackend();
}

void AppController::setCurrentConnectionId(const QString &id) {
    if (m_currentConnectionId == id)
        return;
    m_currentConnectionId = id;
    emit currentConnectionChanged();
}

void AppController::bootstrap() {
    const auto env = QProcessEnvironment::systemEnvironment();

    // Default local connection. On Linux/macOS a session URI works without root;
    // sysadmins typically want system. We add session and let the user add more.
    const QString localUri = env.value("VMM_CONNECT",
#if defined(Q_OS_WIN)
        QStringLiteral("qemu+ssh://")  // Windows has no local libvirt path yet
#else
        QStringLiteral("qemu:///session")
#endif
    );

    if (!localUri.trimmed().isEmpty() && localUri != QStringLiteral("qemu+ssh://")) {
        addConnection(localUri, usingMockBackend() ? QStringLiteral("Demo host (mock)")
                                                   : QStringLiteral("Local"));
    } else {
        // No sensible local default (e.g. Windows): still show a mock demo host
        // so the app is never empty on first run.
        addConnection(QStringLiteral("mock:///demo"), QStringLiteral("Demo host (mock)"));
    }

    if (m_currentConnectionId.isEmpty() && !m_cm->connectionIds().isEmpty())
        setCurrentConnectionId(m_cm->connectionIds().first());

    m_statsTimer.start();
    m_fullRefreshTimer.start();
    emit backendKindChanged();
}

void AppController::addConnection(const QString &uri, const QString &displayName) {
    const QString id = m_cm->addConnection(uri, displayName);
    if (m_currentConnectionId.isEmpty())
        setCurrentConnectionId(id);
    m_cm->openConnection(id);  // async; onConnectionStateChanged follows
}

void AppController::removeConnection(const QString &connId) {
    m_cm->removeConnection(connId);
    m_vms->removeConnection(connId);
    m_connections->remove(connId);
    if (m_currentConnectionId == connId) {
        const auto ids = m_cm->connectionIds();
        setCurrentConnectionId(ids.isEmpty() ? QString() : ids.first());
    }
}

void AppController::onConnectionStateChanged(const QString &connId, bool connected, const QString &error) {
    HostInfo h = m_cm->hostInfo(connId);
    h.connected = connected;
    h.lastError = error;
    m_connections->upsert(h);
    if (connected) {
        emit notify(Success, tr("Connected"), h.displayName);
        fullRefresh(connId);
    } else {
        emit notify(Error, tr("Connection failed"), error.isEmpty() ? connId : error);
    }
}

void AppController::refresh() {
    for (const QString &id : m_cm->connectionIds())
        fullRefresh(id);
}

void AppController::fullRefresh(const QString &connId) {
    auto result = std::make_shared<QList<VmInfo>>();
    auto host = std::make_shared<HostInfo>();
    m_cm->runAsync(connId,
        [result, host](IHypervisorBackend &b) {
            if (!b.isOpen()) b.open();
            *result = b.listVms();
            *host = b.hostInfo();
        },
        [this, connId, result, host] {
            m_vms->mergeConnection(connId, *result);
            m_connections->upsert(*host);
        },
        [this, connId](const QString &err) {
            emit notify(Warning, tr("Refresh failed"), err);
        });
}

void AppController::statsPoll() {
    for (const QString &connId : m_cm->connectionIds()) {
        auto stats = std::make_shared<QHash<QString, StatSample>>();
        m_cm->runAsync(connId,
            [stats](IHypervisorBackend &b) {
                if (!b.isOpen()) return;
                for (const VmInfo &v : b.listVms())
                    stats->insert(v.uuid, v.stats);
            },
            [this, connId, stats] { m_vms->applyStats(connId, *stats); });
    }
}

void AppController::lifecycle(const QString &connId, const QString &uuid,
                             const QString &action,
                             std::function<void(IHypervisorBackend&)> job) {
    m_cm->runAsync(connId, std::move(job),
        [this, connId, uuid, action] {
            emit vmActionCompleted(uuid, action);
            fullRefresh(connId);
        },
        [this, action](const QString &err) {
            emit notify(Error, tr("%1 failed").arg(action), err);
        });
}

void AppController::startVm(const QString &c, const QString &u)   { lifecycle(c, u, "start",    [u](IHypervisorBackend &b){ b.start(u); }); }
void AppController::shutdownVm(const QString &c, const QString &u){ lifecycle(c, u, "shutdown", [u](IHypervisorBackend &b){ b.shutdown(u); }); }
void AppController::forceOffVm(const QString &c, const QString &u){ lifecycle(c, u, "force off",[u](IHypervisorBackend &b){ b.forceOff(u); }); }
void AppController::rebootVm(const QString &c, const QString &u)  { lifecycle(c, u, "reboot",   [u](IHypervisorBackend &b){ b.reboot(u); }); }
void AppController::pauseVm(const QString &c, const QString &u)   { lifecycle(c, u, "pause",    [u](IHypervisorBackend &b){ b.pause(u); }); }
void AppController::resumeVm(const QString &c, const QString &u)  { lifecycle(c, u, "resume",   [u](IHypervisorBackend &b){ b.resume(u); }); }

void AppController::setAutostart(const QString &c, const QString &u, bool on) {
    lifecycle(c, u, on ? "enable autostart" : "disable autostart",
              [u, on](IHypervisorBackend &b){ b.setAutostart(u, on); });
}

void AppController::deleteVm(const QString &c, const QString &u, bool removeStorage) {
    lifecycle(c, u, "delete", [u, removeStorage](IHypervisorBackend &b){ b.undefine(u, removeStorage); });
}

void AppController::createVm(const QString &connId, const QVariantMap &request) {
    const VmCreateRequest req = toRequest(request);
    m_cm->runAsync(connId,
        [req](IHypervisorBackend &b){ b.define(req); },
        [this, connId, req] {
            emit notify(Success, tr("VM created"), req.name);
            fullRefresh(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Create failed"), err); });
}

void AppController::cloneVm(const QString &connId, const QString &uuid,
                            const QString &newName, bool linked) {
    m_cm->runAsync(connId,
        [uuid, newName, linked](IHypervisorBackend &b){ b.clone(uuid, newName, linked); },
        [this, connId, newName] {
            emit notify(Success, tr("VM cloned"), newName);
            fullRefresh(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Clone failed"), err); });
}

void AppController::markTemplate(const QString &connId, const QString &uuid, bool on) {
    lifecycle(connId, uuid, on ? "mark as template" : "unmark template",
              [uuid, on](IHypervisorBackend &b){ b.markTemplate(uuid, on); });
}

void AppController::takeSnapshot(const QString &connId, const QString &uuid,
                                 const QString &name, const QString &description,
                                 bool includeMemory) {
    m_cm->runAsync(connId,
        [uuid, name, description, includeMemory](IHypervisorBackend &b){
            b.createSnapshot(uuid, name, description, includeMemory);
        },
        [this, connId, uuid, name] {
            emit notify(Success, tr("Snapshot taken"), name);
            loadSnapshots(connId, uuid);
        },
        [this](const QString &err){ emit notify(Error, tr("Snapshot failed"), err); });
}

void AppController::restoreSnapshot(const QString &connId, const QString &uuid, const QString &name) {
    m_cm->runAsync(connId,
        [uuid, name](IHypervisorBackend &b){ b.restoreSnapshot(uuid, name); },
        [this, connId, uuid, name] {
            emit notify(Success, tr("Snapshot restored"), name);
            fullRefresh(connId);
            loadSnapshots(connId, uuid);
        },
        [this](const QString &err){ emit notify(Error, tr("Restore failed"), err); });
}

void AppController::deleteSnapshot(const QString &connId, const QString &uuid, const QString &name) {
    m_cm->runAsync(connId,
        [uuid, name](IHypervisorBackend &b){ b.deleteSnapshot(uuid, name); },
        [this, connId, uuid] { loadSnapshots(connId, uuid); },
        [this](const QString &err){ emit notify(Error, tr("Delete snapshot failed"), err); });
}

void AppController::loadSnapshots(const QString &connId, const QString &uuid) {
    auto result = std::make_shared<QList<SnapshotInfo>>();
    m_cm->runAsync(connId,
        [uuid, result](IHypervisorBackend &b){ *result = b.listSnapshots(uuid); },
        [this, uuid, result] {
            QVariantList out;
            for (const auto &s : *result) out.push_back(snapshotToMap(s));
            emit snapshotsLoaded(uuid, out);
        });
}

void AppController::importImage(const QString &connId, const QString &sourcePath,
                                const QVariantMap &request) {
    const VmCreateRequest req = toRequest(request);
    // ImageImporter converts (off-thread) and emits converted(diskPath) or failed.
    m_importer->convert(sourcePath, req.diskFormat, [this, connId, req](const QString &preparedPath,
                                                                         const QString &error) {
        if (!error.isEmpty()) {
            emit notify(Error, tr("Import failed"), error);
            return;
        }
        m_cm->runAsync(connId,
            [preparedPath, req](IHypervisorBackend &b){ b.importPreparedDisk(preparedPath, req); },
            [this, connId, req] {
                emit notify(Success, tr("Image imported"), req.name);
                fullRefresh(connId);
            },
            [this](const QString &err){ emit notify(Error, tr("Import failed"), err); });
    });
}

void AppController::loadStorage(const QString &connId) {
    auto pools = std::make_shared<QList<StoragePoolInfo>>();
    m_cm->runAsync(connId,
        [pools](IHypervisorBackend &b){ *pools = b.listStoragePools(); },
        [this, connId, pools] {
            QVariantList out;
            for (const auto &p : *pools)
                out.push_back(QVariantMap{
                    {"name", p.name}, {"type", p.type}, {"active", p.active},
                    {"capacityBytes", QVariant::fromValue(p.capacityBytes)},
                    {"allocationBytes", QVariant::fromValue(p.allocationBytes)}});
            emit storageLoaded(connId, out);
        });
}

void AppController::loadNetworks(const QString &connId) {
    auto nets = std::make_shared<QList<NetworkInfo>>();
    m_cm->runAsync(connId,
        [nets](IHypervisorBackend &b){ *nets = b.listNetworks(); },
        [this, connId, nets] {
            QVariantList out;
            for (const auto &n : *nets)
                out.push_back(QVariantMap{
                    {"name", n.name}, {"mode", n.mode}, {"bridge", n.bridge},
                    {"active", n.active}, {"forwardDev", n.forwardDev}});
            emit networksLoaded(connId, out);
        });
}

} // namespace vmm
