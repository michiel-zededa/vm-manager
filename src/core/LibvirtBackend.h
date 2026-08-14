#pragma once

#include "Backend.h"

#include <libvirt/libvirt.h>

#include <QString>

namespace vmm {

// Real libvirt client. Compiled only when libvirt is found (HAVE_LIBVIRT).
// Wraps the libvirt C API; all methods run on a worker thread (see
// ConnectionManager) and throw BackendError on failure.
//
// Status: core lifecycle + enumeration + snapshots implemented; define()/import
// build a minimal domain XML; clone() is phase 3 (needs volume copy). See
// ROADMAP.md.
class LibvirtBackend final : public IHypervisorBackend {
public:
    LibvirtBackend(QString uri, QString displayName,
                   QString username = {}, QString password = {});
    ~LibvirtBackend() override;

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

    ConsoleInfo consoleInfo(const QString &uuid) override;

    VmInfo importPreparedDisk(const QString &diskPath, const VmCreateRequest &req) override;

private:
    [[noreturn]] static void throwLast(const QString &context);
    virDomainPtr lookup(const QString &uuid);        // caller frees
    VmInfo toVmInfo(virDomainPtr dom);
    QString buildDomainXml(const VmCreateRequest &req, const QString &diskPath);

    QString m_uri;
    QString m_displayName;
    QString m_username;
    QString m_password;
    virConnectPtr m_conn = nullptr;
};

} // namespace vmm
