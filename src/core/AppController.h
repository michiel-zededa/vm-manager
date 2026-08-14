#pragma once

#include "Types.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

namespace vmm {

class ConnectionManager;
class VmListModel;
class ConnectionModel;
class ImageImporter;
class SnapshotScheduler;

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
    QString appVersion() const;

    // Called once at startup: add the local connection (or a mock host) and
    // any URI from $VMM_CONNECT, then begin polling.
    void bootstrap();

    // ---- Connection actions --------------------------------------------
    Q_INVOKABLE void addConnection(const QString &uri, const QString &displayName);
    Q_INVOKABLE void removeConnection(const QString &connId);
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

    // ---- Import ---------------------------------------------------------
    // Converts (if needed) then defines a VM. request as in createVm.
    Q_INVOKABLE void importImage(const QString &connId, const QString &sourcePath,
                                 const QVariantMap &request);

    // ---- Enumerations for storage/network views -------------------------
    Q_INVOKABLE void loadStorage(const QString &connId);   // -> storageLoaded
    Q_INVOKABLE void loadNetworks(const QString &connId);  // -> networksLoaded

signals:
    void currentConnectionChanged();
    void backendKindChanged();
    void notify(int level, const QString &title, const QString &message); // level: 0 info,1 success,2 warning,3 error
    void vmActionCompleted(const QString &uuid, const QString &action);
    void snapshotsLoaded(const QString &uuid, const QVariantList &snapshots);
    void storageLoaded(const QString &connId, const QVariantList &pools);
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
