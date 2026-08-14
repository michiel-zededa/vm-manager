#include "AppController.h"

#include "Backend.h"
#include "ConnectionManager.h"
#include "VmListModel.h"
#include "ConnectionModel.h"
#include "ImageImporter.h"
#include "SnapshotScheduler.h"

#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QUrlQuery>
#include <QProcess>
#include <QDesktopServices>
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

    // The scheduler asks us to take a snapshot when a policy fires; afterwards
    // we prune older auto-* snapshots beyond the retention count.
    connect(m_scheduler, &SnapshotScheduler::snapshotDue, this,
            [this](const QString &connId, const QString &uuid, const QString &name, int retain) {
                m_cm->runAsync(connId,
                    [uuid, name, retain](IHypervisorBackend &b) {
                        b.createSnapshot(uuid, name, QStringLiteral("Scheduled snapshot"), false);
                        // Keep the newest `retain` auto-* snapshots (names sort by time).
                        QList<SnapshotInfo> autos;
                        for (const auto &s : b.listSnapshots(uuid))
                            if (s.name.startsWith(QLatin1String("auto-")))
                                autos.push_back(s);
                        std::sort(autos.begin(), autos.end(),
                                  [](const SnapshotInfo &a, const SnapshotInfo &c){ return a.name > c.name; });
                        for (int i = qMax(1, retain); i < autos.size(); ++i)
                            b.deleteSnapshot(uuid, autos[i].name);
                    },
                    [this, connId, uuid, name] {
                        emit notify(Success, tr("Scheduled snapshot"), name);
                        loadSnapshots(connId, uuid);
                    },
                    [this](const QString &err) { emit notify(Warning, tr("Scheduled snapshot failed"), err); });
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

bool AppController::demoMode() const {
    return m_cm->isDemoMode();
}

void AppController::setCurrentConnectionId(const QString &id) {
    if (m_currentConnectionId == id)
        return;
    m_currentConnectionId = id;
    emit currentConnectionChanged();
}

void AppController::bootstrap() {
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString override = env.value("VMM_CONNECT").trimmed();

    if (m_cm->isDemoMode()) {
        // Explicit Demo mode: a single seeded mock host.
        addConnection(QStringLiteral("mock:///demo"), QStringLiteral("Demo host (mock)"));
    } else if (!override.isEmpty()) {
        addConnection(override, override);
    } else {
#if !defined(Q_OS_WIN)
        // Local libvirt session works without root on Linux/macOS. On a build
        // without libvirt this is an empty mock — the UI explains why. Windows
        // has no local hypervisor, so we start with no connection and let the
        // user add a remote one.
        addConnection(QStringLiteral("qemu:///session"), QStringLiteral("Local"));
#endif
    }

    if (m_currentConnectionId.isEmpty() && !m_cm->connectionIds().isEmpty())
        setCurrentConnectionId(m_cm->connectionIds().first());

    m_statsTimer.start();
    m_fullRefreshTimer.start();
    emit backendKindChanged();
}

void AppController::addConnection(const QString &uri, const QString &displayName,
                                  const QString &username, const QString &password) {
    const QString id = m_cm->addConnection(uri, displayName, username, password);
    // Show the host immediately (disconnected) so it can be managed even if the
    // first connect fails.
    HostInfo h = m_cm->hostInfo(id);
    h.displayName = displayName;
    h.connected = false;
    m_connections->upsert(h);
    if (m_currentConnectionId.isEmpty())
        setCurrentConnectionId(id);
    m_cm->openConnection(id);  // async; onConnectionStateChanged follows
}

void AppController::connectConnection(const QString &connId) {
    m_cm->openConnection(connId);
}

void AppController::disconnectConnection(const QString &connId) {
    m_cm->closeConnection(connId);
    m_vms->removeConnection(connId);
    HostInfo h = m_cm->hostInfo(connId);
    h.connected = false;
    h.activeVms = 0;
    m_connections->upsert(h);
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
    } else if (!error.isEmpty()) {
        emit notify(Error, tr("Connection failed"), error);
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

void AppController::loadConsole(const QString &connId, const QString &uuid) {
    auto info = std::make_shared<ConsoleInfo>();
    m_cm->runAsync(connId,
        [uuid, info](IHypervisorBackend &b){ *info = b.consoleInfo(uuid); },
        [this, uuid, info] {
            emit consoleLoaded(uuid, QVariantMap{
                {"graphicsType", info->graphicsType}, {"host", info->host},
                {"port", info->port}, {"hasSerial", info->hasSerial},
                {"running", info->running}});
        },
        [this](const QString &err){ emit notify(Warning, tr("Console info failed"), err); });
}

void AppController::openConsoleExternally(const QString &connId, const QString &uuid) {
    auto info = std::make_shared<ConsoleInfo>();
    m_cm->runAsync(connId,
        [uuid, info](IHypervisorBackend &b){ *info = b.consoleInfo(uuid); },
        [this, connId, info] {
            if (!info->running) { emit notify(Warning, tr("Console"), tr("The VM is not running.")); return; }
            if (info->port <= 0 || info->graphicsType.isEmpty()) {
                emit notify(Warning, tr("Console"), tr("No graphical console is configured for this VM."));
                return;
            }
            const QString scheme = info->graphicsType == QLatin1String("spice") ? QStringLiteral("spice")
                                                                                : QStringLiteral("vnc");
            const QUrl curi(connId);

            // Local hypervisor: the endpoint is reachable directly.
            if (!curi.scheme().startsWith(QLatin1String("qemu+"))) {
                const QUrl url(QStringLiteral("%1://%2:%3").arg(scheme, info->host).arg(info->port));
                if (!QDesktopServices::openUrl(url))
                    emit notify(Warning, tr("Console"), tr("Open this address in a viewer: %1").arg(url.toString()));
                else
                    emit notify(Info, tr("Opening console"), url.toString());
                return;
            }

            // Remote host: the graphics port listens on the remote's loopback,
            // so it must be reached through an SSH tunnel.
            const QString transport = curi.scheme().mid(5); // strip "qemu+"
            const QString host = curi.host();
            const QString user = curi.userName();
            const int sshPort = curi.port() > 0 ? curi.port() : 22;
            const QUrlQuery q(curi.query());
            const QString keyfile = q.queryItemValue(QStringLiteral("keyfile"));
            const int localPort = 15900 + (info->port % 1000);
            const QString userHost = user.isEmpty() ? host : (user + "@" + host);
            const QUrl viewUrl(QStringLiteral("%1://127.0.0.1:%2").arg(scheme).arg(localPort));

            // Password (libssh2) connections can't drive OpenSSH non-interactively;
            // hand over the exact tunnel command instead of failing silently.
            if (transport == QLatin1String("libssh2")) {
                emit notify(Warning, tr("Console needs an SSH tunnel"),
                    tr("Run:  ssh -N -L %1:127.0.0.1:%2 %3 -p %4   then open %5")
                        .arg(localPort).arg(info->port).arg(userHost).arg(sshPort).arg(viewUrl.toString()));
                return;
            }

            // Key/agent (ssh transport): open the tunnel for the user.
            QStringList args{ "-f", "-N",
                "-o", "ExitOnForwardFailure=yes",
                "-o", "StrictHostKeyChecking=accept-new",
                "-L", QStringLiteral("%1:127.0.0.1:%2").arg(localPort).arg(info->port) };
            if (sshPort != 22) args << "-p" << QString::number(sshPort);
            if (!keyfile.isEmpty()) args << "-i" << keyfile;
            args << userHost;
            if (!QProcess::startDetached(QStringLiteral("ssh"), args)) {
                emit notify(Error, tr("Console"), tr("Could not start the SSH tunnel (ssh not found?)."));
                return;
            }
            // Give the forward a moment to establish, then launch the viewer.
            QTimer::singleShot(1000, this, [this, viewUrl] {
                if (QDesktopServices::openUrl(viewUrl))
                    emit notify(Info, tr("Opening console"), viewUrl.toString());
                else
                    emit notify(Warning, tr("Console"), tr("Tunnel up — open %1 in a viewer.").arg(viewUrl.toString()));
            });
        },
        [this](const QString &err){ emit notify(Error, tr("Console failed"), err); });
}

void AppController::loadDisks(const QString &connId, const QString &uuid) {
    auto disks = std::make_shared<QList<DiskInfo>>();
    m_cm->runAsync(connId,
        [uuid, disks](IHypervisorBackend &b){ *disks = b.listDisks(uuid); },
        [this, uuid, disks] {
            QVariantList out;
            for (const auto &d : *disks)
                out.push_back(QVariantMap{
                    {"target", d.target}, {"path", d.path}, {"bus", d.bus},
                    {"format", d.format}, {"device", d.device},
                    {"capacityBytes", QVariant::fromValue(d.capacityBytes)}});
            emit disksLoaded(uuid, out);
        },
        [this](const QString &err){ emit notify(Warning, tr("Load disks failed"), err); });
}

void AppController::attachDisk(const QString &connId, const QString &uuid,
                               const QString &volumePath, const QString &bus, const QString &format) {
    m_cm->runAsync(connId,
        [uuid, volumePath, bus, format](IHypervisorBackend &b){ b.attachDisk(uuid, volumePath, bus, format); },
        [this, connId, uuid] { emit notify(Success, tr("Disk attached"), {}); loadDisks(connId, uuid); },
        [this](const QString &err){ emit notify(Error, tr("Attach disk failed"), err); });
}

void AppController::detachDisk(const QString &connId, const QString &uuid, const QString &target) {
    m_cm->runAsync(connId,
        [uuid, target](IHypervisorBackend &b){ b.detachDisk(uuid, target); },
        [this, connId, uuid, target] { emit notify(Success, tr("Disk detached"), target); loadDisks(connId, uuid); },
        [this](const QString &err){ emit notify(Error, tr("Detach disk failed"), err); });
}

void AppController::resizeVolume(const QString &connId, const QString &poolName,
                                 const QString &volumeName, double capacityGiB) {
    const quint64 bytes = quint64(capacityGiB * 1024.0 * 1024.0 * 1024.0);
    m_cm->runAsync(connId,
        [poolName, volumeName, bytes](IHypervisorBackend &b){ b.resizeVolume(poolName, volumeName, bytes); },
        [this, connId, volumeName] { emit notify(Success, tr("Volume resized"), volumeName); loadStorage(connId); },
        [this](const QString &err){ emit notify(Error, tr("Resize failed"), err); });
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
                    {"targetPath", p.targetPath}, {"autostart", p.autostart},
                    {"capacityBytes", QVariant::fromValue(p.capacityBytes)},
                    {"allocationBytes", QVariant::fromValue(p.allocationBytes)}});
            emit storageLoaded(connId, out);
        });
}

void AppController::loadVolumes(const QString &connId, const QString &poolName) {
    auto vols = std::make_shared<QList<VolumeInfo>>();
    m_cm->runAsync(connId,
        [poolName, vols](IHypervisorBackend &b){ *vols = b.listVolumes(poolName); },
        [this, connId, poolName, vols] {
            QVariantList out;
            for (const auto &v : *vols)
                out.push_back(QVariantMap{
                    {"name", v.name}, {"path", v.path}, {"format", v.format},
                    {"capacityBytes", QVariant::fromValue(v.capacityBytes)},
                    {"allocationBytes", QVariant::fromValue(v.allocationBytes)}});
            emit volumesLoaded(connId, poolName, out);
        },
        [this](const QString &err){ emit notify(Warning, tr("Load volumes failed"), err); });
}

void AppController::createStoragePool(const QString &connId, const QString &name,
                                      const QString &type, const QString &path) {
    m_cm->runAsync(connId,
        [name, type, path](IHypervisorBackend &b){ b.createStoragePool(name, type, path); },
        [this, connId, name] {
            emit notify(Success, tr("Pool created"), name);
            loadStorage(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Create pool failed"), err); });
}

void AppController::deleteStoragePool(const QString &connId, const QString &name, bool deleteContents) {
    m_cm->runAsync(connId,
        [name, deleteContents](IHypervisorBackend &b){ b.deleteStoragePool(name, deleteContents); },
        [this, connId, name] {
            emit notify(Success, tr("Pool deleted"), name);
            loadStorage(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Delete pool failed"), err); });
}

void AppController::setStoragePoolActive(const QString &connId, const QString &name, bool active) {
    m_cm->runAsync(connId,
        [name, active](IHypervisorBackend &b){ b.setStoragePoolActive(name, active); },
        [this, connId] { loadStorage(connId); },
        [this](const QString &err){ emit notify(Error, tr("Pool state change failed"), err); });
}

void AppController::createVolume(const QString &connId, const QString &poolName,
                                 const QString &name, const QString &format, double capacityGiB) {
    const quint64 bytes = quint64(capacityGiB * 1024.0 * 1024.0 * 1024.0);
    m_cm->runAsync(connId,
        [poolName, name, format, bytes](IHypervisorBackend &b){ b.createVolume(poolName, name, format, bytes); },
        [this, connId, poolName, name] {
            emit notify(Success, tr("Volume created"), name);
            loadVolumes(connId, poolName);
            loadStorage(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Create volume failed"), err); });
}

void AppController::deleteVolume(const QString &connId, const QString &poolName, const QString &volumeName) {
    m_cm->runAsync(connId,
        [poolName, volumeName](IHypervisorBackend &b){ b.deleteVolume(poolName, volumeName); },
        [this, connId, poolName, volumeName] {
            emit notify(Success, tr("Volume deleted"), volumeName);
            loadVolumes(connId, poolName);
            loadStorage(connId);
        },
        [this](const QString &err){ emit notify(Error, tr("Delete volume failed"), err); });
}

QString AppController::buildConnectionUri(const QString &transport, const QString &host,
                                          const QString &user, int port, const QString &path) const {
    const QString p = path.isEmpty() ? QStringLiteral("system") : path;
    if (transport == QStringLiteral("qemu:///session") || transport == QStringLiteral("qemu:///system"))
        return transport;
    // e.g. qemu+ssh://user@host:port/system
    QString uri = QStringLiteral("qemu+%1://").arg(transport);
    if (!user.isEmpty())
        uri += QUrl::toPercentEncoding(user) + QLatin1Char('@');
    uri += host;
    if (port > 0)
        uri += QStringLiteral(":%1").arg(port);
    uri += QLatin1Char('/') + p;
    return uri;
}

QVariantMap AppController::dependencyStatus() const {
    // GUI apps launched from Finder/Explorer don't inherit the shell PATH, so a
    // Homebrew-installed qemu/libvirt would look "missing". Search the usual
    // install locations in addition to PATH.
    QStringList paths = QProcessEnvironment::systemEnvironment()
                            .value(QStringLiteral("PATH"))
                            .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    const QStringList extra = {
        QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/opt/homebrew/sbin"),
        QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/local/sbin"),
        QStringLiteral("/opt/homebrew/opt/libvirt/sbin"),
        QStringLiteral("/usr/local/opt/libvirt/sbin"),
        QStringLiteral("/usr/bin"), QStringLiteral("/usr/sbin"),
        QStringLiteral("/bin"), QStringLiteral("/sbin"),
    };
    for (const QString &p : extra)
        if (!paths.contains(p)) paths << p;

    const auto has = [&](const QString &exe) {
        return !QStandardPaths::findExecutable(exe, paths).isEmpty();
    };
    QVariantMap m;
    m["qemuImg"]  = has(QStringLiteral("qemu-img"));
    m["qemu"]     = has(QStringLiteral("qemu-system-x86_64")) || has(QStringLiteral("qemu-system-aarch64"));
    m["libvirt"]  = has(QStringLiteral("virsh")) || has(QStringLiteral("libvirtd"));
    m["usingMock"] = usingMockBackend();
    return m;
}

QString AppController::installHint(const QString &what) const {
#if defined(Q_OS_MACOS)
    if (what == QStringLiteral("qemu"))    return QStringLiteral("brew install qemu");
    if (what == QStringLiteral("libvirt")) return QStringLiteral("brew install libvirt && brew services start libvirt");
    return QStringLiteral("brew install qemu libvirt");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Local virtualization on Windows is not supported yet; "
                          "connect to a remote host over qemu+ssh:// instead.");
#else
    if (what == QStringLiteral("qemu"))    return QStringLiteral("sudo apt install qemu-utils qemu-system-x86 || sudo dnf install qemu-img qemu-kvm");
    if (what == QStringLiteral("libvirt")) return QStringLiteral("sudo apt install libvirt-daemon-system && sudo systemctl enable --now libvirtd");
    return QStringLiteral("sudo apt install qemu-system-x86 qemu-utils libvirt-daemon-system");
#endif
}

void AppController::createNetwork(const QString &connId, const QString &name,
                                  const QString &mode, const QString &forwardDev) {
    m_cm->runAsync(connId,
        [name, mode, forwardDev](IHypervisorBackend &b){ b.createNetwork(name, mode, forwardDev); },
        [this, connId, name] { emit notify(Success, tr("Network created"), name); loadNetworks(connId); },
        [this](const QString &err){ emit notify(Error, tr("Create network failed"), err); });
}

void AppController::deleteNetwork(const QString &connId, const QString &name) {
    m_cm->runAsync(connId,
        [name](IHypervisorBackend &b){ b.deleteNetwork(name); },
        [this, connId, name] { emit notify(Success, tr("Network deleted"), name); loadNetworks(connId); },
        [this](const QString &err){ emit notify(Error, tr("Delete network failed"), err); });
}

void AppController::setNetworkActive(const QString &connId, const QString &name, bool active) {
    m_cm->runAsync(connId,
        [name, active](IHypervisorBackend &b){ b.setNetworkActive(name, active); },
        [this, connId] { loadNetworks(connId); },
        [this](const QString &err){ emit notify(Error, tr("Network state change failed"), err); });
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
