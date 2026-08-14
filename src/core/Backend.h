#pragma once

#include "Types.h"

#include <QObject>
#include <QString>
#include <QList>
#include <functional>

namespace vmm {

// The single seam between the UI and any hypervisor implementation.
//
// Two implementations exist: LibvirtBackend (real, compiled when libvirt is
// present) and MockBackend (in-memory, always available). The UI never talks
// to libvirt directly — only to this interface, via ConnectionManager which
// runs every call off the GUI thread.
//
// All methods are synchronous and MUST be called from a worker thread. Results
// are marshalled back to the GUI thread by ConnectionManager. Implementations
// throw vmm::BackendError on failure.
class IHypervisorBackend {
public:
    virtual ~IHypervisorBackend() = default;

    // Identity of this backend instance / connection.
    virtual HostInfo hostInfo() = 0;

    // Connection lifecycle.
    virtual void open() = 0;          // connect; throws on failure
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Enumeration.
    virtual QList<VmInfo> listVms() = 0;
    virtual VmInfo vm(const QString &uuid) = 0;
    virtual StatSample sampleStats(const QString &uuid) = 0;

    // Lifecycle actions.
    virtual void start(const QString &uuid) = 0;
    virtual void shutdown(const QString &uuid) = 0;   // graceful (ACPI)
    virtual void forceOff(const QString &uuid) = 0;   // destroy
    virtual void reboot(const QString &uuid) = 0;
    virtual void pause(const QString &uuid) = 0;
    virtual void resume(const QString &uuid) = 0;
    virtual void setAutostart(const QString &uuid, bool on) = 0;

    // Definition / removal.
    virtual VmInfo define(const VmCreateRequest &req) = 0;
    virtual void undefine(const QString &uuid, bool removeStorage) = 0;

    // Templates & cloning.
    virtual void markTemplate(const QString &uuid, bool on) = 0;
    virtual VmInfo clone(const QString &uuid, const QString &newName, bool linked) = 0;

    // Snapshots.
    virtual QList<SnapshotInfo> listSnapshots(const QString &uuid) = 0;
    virtual SnapshotInfo createSnapshot(const QString &uuid, const QString &name,
                                        const QString &description, bool includeMemory) = 0;
    virtual void restoreSnapshot(const QString &uuid, const QString &name) = 0;
    virtual void deleteSnapshot(const QString &uuid, const QString &name) = 0;

    // Storage & networking.
    virtual QList<StoragePoolInfo> listStoragePools() = 0;
    virtual QList<VolumeInfo> listVolumes(const QString &poolName) = 0;
    virtual QList<NetworkInfo> listNetworks() = 0;

    // Storage management. `type` is a libvirt pool type ("dir", "logical", …);
    // for "dir" pools `path` is the host directory to adopt/create.
    virtual StoragePoolInfo createStoragePool(const QString &name, const QString &type,
                                              const QString &path) = 0;
    virtual void deleteStoragePool(const QString &name, bool deleteContents) = 0;
    virtual void setStoragePoolActive(const QString &name, bool active) = 0;
    // Create a new empty volume of `capacityBytes` in `poolName` (format qcow2/raw).
    virtual VolumeInfo createVolume(const QString &poolName, const QString &name,
                                    const QString &format, quint64 capacityBytes) = 0;
    virtual void deleteVolume(const QString &poolName, const QString &volumeName) = 0;

    // Console reachability (graphical endpoint + serial availability).
    virtual ConsoleInfo consoleInfo(const QString &uuid) = 0;

    // Import a prepared (already-converted) qcow2/raw volume + metadata as a VM.
    // Format conversion itself is handled by ImageImporter before this call.
    virtual VmInfo importPreparedDisk(const QString &diskPath, const VmCreateRequest &req) = 0;
};

// Thrown by backend implementations on any failure.
class BackendError {
public:
    explicit BackendError(QString message) : m_message(std::move(message)) {}
    const QString &message() const { return m_message; }
private:
    QString m_message;
};

} // namespace vmm
