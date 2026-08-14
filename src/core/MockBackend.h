#pragma once

#include "Backend.h"

#include <QMutex>
#include <QHash>
#include <random>

namespace vmm {

// In-memory backend with believable data and slightly drifting live stats.
// Always available (no libvirt required) so the entire UI can be developed,
// demoed and UI-tested on any machine — including a fresh Mac.
class MockBackend final : public IHypervisorBackend {
public:
    // `demo` seeds believable sample VMs/pools/networks (Demo mode). When false
    // the backend starts empty — used only as a graceful fallback when libvirt
    // is unavailable, so no fake data is ever mistaken for real machines.
    explicit MockBackend(QString connectionId, QString displayName, bool demo = false);

    HostInfo hostInfo() override;
    void open() override;
    void close() override;
    bool isOpen() const override;

    QList<VmInfo> listVms() override;
    VmInfo vm(const QString &uuid) override;
    StatSample sampleStats(const QString &uuid) override;

    void start(const QString &uuid) override;
    void shutdown(const QString &uuid) override;
    void forceOff(const QString &uuid) override;
    void reboot(const QString &uuid) override;
    void pause(const QString &uuid) override;
    void resume(const QString &uuid) override;
    void setAutostart(const QString &uuid, bool on) override;

    VmInfo define(const VmCreateRequest &req) override;
    void undefine(const QString &uuid, bool removeStorage) override;

    void markTemplate(const QString &uuid, bool on) override;
    VmInfo clone(const QString &uuid, const QString &newName, bool linked) override;

    QList<SnapshotInfo> listSnapshots(const QString &uuid) override;
    SnapshotInfo createSnapshot(const QString &uuid, const QString &name,
                                const QString &description, bool includeMemory) override;
    void restoreSnapshot(const QString &uuid, const QString &name) override;
    void deleteSnapshot(const QString &uuid, const QString &name) override;

    QList<StoragePoolInfo> listStoragePools() override;
    QList<VolumeInfo> listVolumes(const QString &poolName) override;
    QList<NetworkInfo> listNetworks() override;

    StoragePoolInfo createStoragePool(const QString &name, const QString &type,
                                      const QString &path) override;
    void deleteStoragePool(const QString &name, bool deleteContents) override;
    void setStoragePoolActive(const QString &name, bool active) override;
    VolumeInfo createVolume(const QString &poolName, const QString &name,
                            const QString &format, quint64 capacityBytes) override;
    void deleteVolume(const QString &poolName, const QString &volumeName) override;

    NetworkInfo createNetwork(const QString &name, const QString &mode,
                              const QString &forwardDev) override;
    void deleteNetwork(const QString &name) override;
    void setNetworkActive(const QString &name, bool active) override;

    ConsoleInfo consoleInfo(const QString &uuid) override;

    VmInfo importPreparedDisk(const QString &diskPath, const VmCreateRequest &req) override;

private:
    VmInfo &require(const QString &uuid);
    StatSample rollStats(const VmInfo &v);

    mutable QMutex m_mutex;
    QString m_connectionId;
    QString m_displayName;
    bool m_demo = false;
    bool m_open = false;

    QHash<QString, VmInfo> m_vms;                        // uuid -> vm
    QHash<QString, QList<SnapshotInfo>> m_snapshots;     // uuid -> snapshots
    QList<StoragePoolInfo> m_pools;
    QHash<QString, QList<VolumeInfo>> m_volumes;         // pool -> volumes
    QList<NetworkInfo> m_networks;

    std::mt19937 m_rng{std::random_device{}()};
};

} // namespace vmm
