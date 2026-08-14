#pragma once

#include "Types.h"
#include "VmListModel.h"
#include "ConnectionModel.h"
#include "ImageImporter.h"
#include "SnapshotScheduler.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

namespace vmm {

class ConnectionManager;

// Root object exposed to QML as `App`. Everything the UI does goes through here.
// It owns the models and the ConnectionManager and keeps the models fresh via
// timers. No QML file ever touches a backend or libvirt directly.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(VmListModel *vms READ vms CONSTANT)
    Q_PROPERTY(ConnectionModel *connections READ connections CONSTANT)
    Q_PROPERTY(ImageImporter *importer READ importer CONSTANT)
    Q_PROPERTY(SnapshotScheduler *scheduler READ scheduler CONSTANT)
    Q_PROPERTY(QString currentConnectionId READ currentConnectionId WRITE setCurrentConnectionId NOTIFY currentConnectionChanged)
    Q_PROPERTY(bool usingMockBackend READ usingMockBackend NOTIFY backendKindChanged)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY backendKindChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    VmListModel *vms() const { return m_vms; }
    ConnectionModel *connections() const { return m_connections; }
    ImageImporter *importer() const { return m_importer; }
    SnapshotScheduler *scheduler() const { return m_scheduler; }

    QString currentConnectionId() const { return m_currentConnectionId; }
    void setCurrentConnectionId(const QString &id);
    bool usingMockBackend() const;
    bool demoMode() const;
    QString appVersion() const;

    // Called once at startup: add the local connection (or a mock host) and
    // any URI from $VMM_CONNECT, then begin polling.
    void bootstrap();

    // ---- Connection actions --------------------------------------------
    Q_INVOKABLE void addConnection(const QString &uri, const QString &displayName,
                                   const QString &username = {}, const QString &password = {});
    Q_INVOKABLE void removeConnection(const QString &connId);
    Q_INVOKABLE void connectConnection(const QString &connId);
    Q_INVOKABLE void disconnectConnection(const QString &connId);
    Q_INVOKABLE void refresh();

    // ---- VM lifecycle ---------------------------------------------------
    Q_INVOKABLE void startVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void shutdownVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void forceOffVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void rebootVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void pauseVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void resumeVm(const QString &connId, const QString &uuid);
    Q_INVOKABLE void setAutostart(const QString &connId, const QString &uuid, bool on);
    Q_INVOKABLE void deleteVm(const QString &connId, const QString &uuid, bool removeStorage);

    // ---- Create / clone / template -------------------------------------
    // request keys: name, osVariant, vcpus, memoryMiB, diskGiB, installMediaPath,
    //               networkName, firmware, diskFormat
    Q_INVOKABLE void createVm(const QString &connId, const QVariantMap &request);
    Q_INVOKABLE void cloneVm(const QString &connId, const QString &uuid,
                             const QString &newName, bool linked);
    Q_INVOKABLE void markTemplate(const QString &connId, const QString &uuid, bool on);

    // ---- Snapshots ------------------------------------------------------
    Q_INVOKABLE void takeSnapshot(const QString &connId, const QString &uuid,
                                  const QString &name, const QString &description,
                                  bool includeMemory);
    Q_INVOKABLE void restoreSnapshot(const QString &connId, const QString &uuid, const QString &name);
    Q_INVOKABLE void deleteSnapshot(const QString &connId, const QString &uuid, const QString &name);
    // Async fetch; result arrives via snapshotsLoaded(uuid, list-of-maps).
    Q_INVOKABLE void loadSnapshots(const QString &connId, const QString &uuid);

    // ---- Console --------------------------------------------------------
    Q_INVOKABLE void loadConsole(const QString &connId, const QString &uuid); // -> consoleLoaded
    // Open the graphical console in the OS's viewer (Screen Sharing / vinagre / …).
    Q_INVOKABLE void openConsoleExternally(const QString &connId, const QString &uuid);

    // ---- Import ---------------------------------------------------------
    // Converts (if needed) then defines a VM. request as in createVm.
    Q_INVOKABLE void importImage(const QString &connId, const QString &sourcePath,
                                 const QVariantMap &request);

    // ---- Enumerations for storage/network views -------------------------
    Q_INVOKABLE void loadStorage(const QString &connId);   // -> storageLoaded
    Q_INVOKABLE void loadNetworks(const QString &connId);  // -> networksLoaded
    Q_INVOKABLE void loadVolumes(const QString &connId, const QString &poolName); // -> volumesLoaded

    // ---- Storage management ---------------------------------------------
    Q_INVOKABLE void createStoragePool(const QString &connId, const QString &name,
                                       const QString &type, const QString &path);
    Q_INVOKABLE void deleteStoragePool(const QString &connId, const QString &name, bool deleteContents);
    Q_INVOKABLE void setStoragePoolActive(const QString &connId, const QString &name, bool active);
    Q_INVOKABLE void createVolume(const QString &connId, const QString &poolName,
                                  const QString &name, const QString &format, double capacityGiB);
    Q_INVOKABLE void deleteVolume(const QString &connId, const QString &poolName, const QString &volumeName);

    // ---- Connections helper (build a URI from parts) --------------------
    Q_INVOKABLE QString buildConnectionUri(const QString &transport, const QString &host,
                                           const QString &user, int port, const QString &path) const;

    // ---- Dependency / environment checks --------------------------------
    Q_INVOKABLE QVariantMap dependencyStatus() const;
    Q_INVOKABLE QString installHint(const QString &what) const;

signals:
    void currentConnectionChanged();
    void backendKindChanged();
    void notify(int level, const QString &title, const QString &message); // level: 0 info,1 success,2 warning,3 error
    void vmActionCompleted(const QString &uuid, const QString &action);
    void snapshotsLoaded(const QString &uuid, const QVariantList &snapshots);
    void consoleLoaded(const QString &uuid, const QVariantMap &console);
    void storageLoaded(const QString &connId, const QVariantList &pools);
    void volumesLoaded(const QString &connId, const QString &poolName, const QVariantList &volumes);
    void networksLoaded(const QString &connId, const QVariantList &networks);

private:
    void fullRefresh(const QString &connId);
    void statsPoll();
    void onConnectionStateChanged(const QString &connId, bool connected, const QString &error);
    // Run a simple lifecycle job then refresh; report via notify + vmActionCompleted.
    void lifecycle(const QString &connId, const QString &uuid,
                   const QString &action, std::function<void(class IHypervisorBackend&)> job);

    ConnectionManager *m_cm = nullptr;
    VmListModel *m_vms = nullptr;
    ConnectionModel *m_connections = nullptr;
    ImageImporter *m_importer = nullptr;
    SnapshotScheduler *m_scheduler = nullptr;

    QString m_currentConnectionId;
    QTimer m_statsTimer;
    QTimer m_fullRefreshTimer;
};

} // namespace vmm
