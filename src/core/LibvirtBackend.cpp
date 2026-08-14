#include "LibvirtBackend.h"

#include <libvirt/virterror.h>

#include <QUuid>

namespace vmm {

namespace {
VmState mapState(int s) {
    switch (s) {
    case VIR_DOMAIN_RUNNING:     return VmState::Running;
    case VIR_DOMAIN_BLOCKED:     return VmState::Running;
    case VIR_DOMAIN_PAUSED:      return VmState::Paused;
    case VIR_DOMAIN_SHUTDOWN:    return VmState::ShuttingDown;
    case VIR_DOMAIN_SHUTOFF:     return VmState::ShutOff;
    case VIR_DOMAIN_CRASHED:     return VmState::Crashed;
    case VIR_DOMAIN_PMSUSPENDED: return VmState::Suspended;
    default:                     return VmState::NoState;
    }
}

// RAII helper for freeing libvirt-allocated C strings.
struct FreeStr {
    char *p = nullptr;
    ~FreeStr() { if (p) free(p); }
};
} // namespace

LibvirtBackend::LibvirtBackend(QString uri, QString displayName)
    : m_uri(std::move(uri)), m_displayName(std::move(displayName)) {}

LibvirtBackend::~LibvirtBackend() { close(); }

void LibvirtBackend::throwLast(const QString &context) {
    virErrorPtr err = virGetLastError();
    const QString msg = (err && err->message) ? QString::fromUtf8(err->message)
                                              : QStringLiteral("unknown libvirt error");
    throw BackendError(QStringLiteral("%1: %2").arg(context, msg));
}

void LibvirtBackend::open() {
    if (m_conn)
        return;
    m_conn = virConnectOpen(m_uri.toUtf8().constData());
    if (!m_conn)
        throwLast(QStringLiteral("Failed to connect to %1").arg(m_uri));
}

void LibvirtBackend::close() {
    if (m_conn) {
        virConnectClose(m_conn);
        m_conn = nullptr;
    }
}

bool LibvirtBackend::isOpen() const { return m_conn != nullptr; }

virDomainPtr LibvirtBackend::lookup(const QString &uuid) {
    virDomainPtr dom = virDomainLookupByUUIDString(m_conn, uuid.toUtf8().constData());
    if (!dom)
        throwLast(QStringLiteral("No such domain %1").arg(uuid));
    return dom;
}

VmInfo LibvirtBackend::toVmInfo(virDomainPtr dom) {
    VmInfo v;
    v.connectionId = m_uri;

    char uuidBuf[VIR_UUID_STRING_BUFLEN] = {0};
    if (virDomainGetUUIDString(dom, uuidBuf) == 0)
        v.uuid = QString::fromLatin1(uuidBuf);

    if (const char *name = virDomainGetName(dom))
        v.name = QString::fromUtf8(name);

    virDomainInfo info;
    if (virDomainGetInfo(dom, &info) == 0) {
        v.state = mapState(info.state);
        v.vcpus = int(info.nrVirtCpu);
        v.memoryMaxKiB = info.maxMem;
        v.memoryCurrentKiB = info.memory;
        v.stats.memMaxKiB = info.maxMem;
        v.stats.memUsedKiB = info.memory;
        v.stats.timestamp = QDateTime::currentDateTime();
    }

    int autostart = 0;
    if (virDomainGetAutostart(dom, &autostart) == 0)
        v.autostart = autostart != 0;

    // Human title (if set) and OS label via metadata; best-effort.
    if (char *title = virDomainGetMetadata(dom, VIR_DOMAIN_METADATA_TITLE, nullptr, 0)) {
        v.title = QString::fromUtf8(title);
        free(title);
    }
    // TODO(phase-1): parse <os> from the domain XML for a friendly osLabel and
    // compute cpuPercent from virDomainGetCPUStats deltas across polls.
    return v;
}

HostInfo LibvirtBackend::hostInfo() {
    HostInfo h;
    h.id = m_uri;
    h.uri = m_uri;
    h.displayName = m_displayName;
    h.connected = m_conn != nullptr;
    h.isLocal = m_uri.contains(QLatin1String("///"));
    if (!m_conn)
        return h;

    if (const char *type = virConnectGetType(m_conn))
        h.hypervisor = QString::fromUtf8(type);

    virNodeInfo node;
    if (virNodeGetInfo(m_conn, &node) == 0) {
        h.hostArch = QString::fromLatin1(node.model);
        h.hostCpus = int(node.cpus);
        h.hostMemoryKiB = node.memory;
    }

    virDomainPtr *domains = nullptr;
    const int n = virConnectListAllDomains(m_conn, &domains, 0);
    if (n >= 0) {
        h.totalVms = n;
        for (int i = 0; i < n; ++i) {
            int state = 0, reason = 0;
            if (virDomainGetState(domains[i], &state, &reason, 0) == 0 && state == VIR_DOMAIN_RUNNING)
                ++h.activeVms;
            virDomainFree(domains[i]);
        }
        free(domains);
    }
    return h;
}

QList<VmInfo> LibvirtBackend::listVms() {
    if (!m_conn)
        open();
    virDomainPtr *domains = nullptr;
    const int n = virConnectListAllDomains(m_conn, &domains, 0);
    if (n < 0)
        throwLast(QStringLiteral("listAllDomains failed"));

    QList<VmInfo> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        out.push_back(toVmInfo(domains[i]));
        virDomainFree(domains[i]);
    }
    free(domains);
    return out;
}

VmInfo LibvirtBackend::vm(const QString &uuid) {
    virDomainPtr dom = lookup(uuid);
    VmInfo v = toVmInfo(dom);
    virDomainFree(dom);
    return v;
}

StatSample LibvirtBackend::sampleStats(const QString &uuid) {
    // TODO(phase-1): virDomainGetCPUStats + virDomainBlockStats + virDomainInterfaceStats
    // with deltas across two samples for real rates. For now return memory.
    return vm(uuid).stats;
}

void LibvirtBackend::start(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainCreate(d);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("start"));
}

void LibvirtBackend::shutdown(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainShutdown(d);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("shutdown"));
}

void LibvirtBackend::forceOff(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainDestroy(d);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("force off"));
}

void LibvirtBackend::reboot(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainReboot(d, 0);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("reboot"));
}

void LibvirtBackend::pause(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainSuspend(d);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("pause"));
}

void LibvirtBackend::resume(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainResume(d);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("resume"));
}

void LibvirtBackend::setAutostart(const QString &uuid, bool on) {
    virDomainPtr d = lookup(uuid);
    const int rc = virDomainSetAutostart(d, on ? 1 : 0);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("set autostart"));
}

QString LibvirtBackend::buildDomainXml(const VmCreateRequest &req, const QString &diskPath) {
    // Minimal but valid QEMU/KVM domain. Phase 1 will expand (UEFI/OVMF loader,
    // virtio-net model choices, cloud-init seed, TPM, etc.).
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const bool uefi = req.firmware.compare("uefi", Qt::CaseInsensitive) == 0;
    QString media;
    if (!req.installMediaPath.isEmpty()) {
        media = QStringLiteral(
            "  <disk type='file' device='cdrom'>\n"
            "    <driver name='qemu' type='raw'/>\n"
            "    <source file='%1'/>\n"
            "    <target dev='sda' bus='sata'/>\n"
            "    <readonly/>\n"
            "  </disk>\n").arg(req.installMediaPath.toHtmlEscaped());
    }
    return QStringLiteral(
        "<domain type='kvm'>\n"
        "  <name>%1</name>\n"
        "  <uuid>%2</uuid>\n"
        "  <memory unit='MiB'>%3</memory>\n"
        "  <currentMemory unit='MiB'>%3</currentMemory>\n"
        "  <vcpu>%4</vcpu>\n"
        "  <os%5>\n"
        "    <type arch='x86_64' machine='q35'>hvm</type>\n"
        "    <boot dev='hd'/>\n"
        "    <boot dev='cdrom'/>\n"
        "  </os>\n"
        "  <features><acpi/><apic/></features>\n"
        "  <cpu mode='host-passthrough'/>\n"
        "  <devices>\n"
        "    <disk type='file' device='disk'>\n"
        "      <driver name='qemu' type='%6'/>\n"
        "      <source file='%7'/>\n"
        "      <target dev='vda' bus='virtio'/>\n"
        "    </disk>\n"
        "%8"
        "    <interface type='network'>\n"
        "      <source network='%9'/>\n"
        "      <model type='virtio'/>\n"
        "    </interface>\n"
        "    <graphics type='vnc' port='-1' autoport='yes' listen='127.0.0.1'/>\n"
        "    <console type='pty'/>\n"
        "    <channel type='unix'><target type='virtio' name='org.qemu.guest_agent.0'/></channel>\n"
        "  </devices>\n"
        "</domain>\n")
        .arg(req.name.toHtmlEscaped(), uuid)
        .arg(req.memoryMiB)
        .arg(req.vcpus)
        .arg(uefi ? QStringLiteral(" firmware='efi'") : QString(),
             req.diskFormat, diskPath.toHtmlEscaped(), media, req.networkName.toHtmlEscaped());
}

VmInfo LibvirtBackend::define(const VmCreateRequest &req) {
    // NOTE(phase-1): this defines the domain but does not yet allocate the
    // backing volume. Until then, pass a prepared disk via importPreparedDisk,
    // or point diskPath at an existing volume.
    const QString diskPath = QStringLiteral("/var/lib/libvirt/images/%1.%2")
                                 .arg(req.name, req.diskFormat);
    const QString xml = buildDomainXml(req, diskPath);
    virDomainPtr dom = virDomainDefineXML(m_conn, xml.toUtf8().constData());
    if (!dom)
        throwLast(QStringLiteral("define domain"));
    VmInfo v = toVmInfo(dom);
    virDomainFree(dom);
    return v;
}

void LibvirtBackend::undefine(const QString &uuid, bool removeStorage) {
    virDomainPtr d = lookup(uuid);
    unsigned int flags = VIR_DOMAIN_UNDEFINE_MANAGED_SAVE | VIR_DOMAIN_UNDEFINE_NVRAM
                       | VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA;
    // TODO(phase-1): when removeStorage, enumerate <disk> volumes and delete
    // them via virStorageVolDelete before/with undefine.
    Q_UNUSED(removeStorage);
    const int rc = virDomainUndefineFlags(d, flags);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("undefine"));
}

void LibvirtBackend::markTemplate(const QString &uuid, bool on) {
    virDomainPtr d = lookup(uuid);
    // Represent "template" as a title marker until we add first-class metadata.
    const QByteArray title = on ? QByteArray("[template]") : QByteArray();
    const int rc = virDomainSetMetadata(d, VIR_DOMAIN_METADATA_TITLE,
                                        title.constData(), nullptr, nullptr,
                                        VIR_DOMAIN_AFFECT_CONFIG);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("mark template"));
}

VmInfo LibvirtBackend::clone(const QString &, const QString &, bool) {
    // Full/linked clone needs storage-volume copy (virStorageVolCreateXMLFrom)
    // plus XML rewrite of disk paths + MAC/UUID. Scheduled for phase 3.
    throw BackendError(QStringLiteral("Cloning is not implemented yet (phase 3). "
                                      "Use the mock backend to preview the flow."));
}

QList<SnapshotInfo> LibvirtBackend::listSnapshots(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    virDomainSnapshotPtr *snaps = nullptr;
    const int n = virDomainListAllSnapshots(d, &snaps, 0);
    QList<SnapshotInfo> out;
    if (n >= 0) {
        virDomainSnapshotPtr current = virDomainSnapshotCurrent(d, 0);
        for (int i = 0; i < n; ++i) {
            SnapshotInfo s;
            if (const char *nm = virDomainSnapshotGetName(snaps[i]))
                s.name = QString::fromUtf8(nm);
            if (current && virDomainSnapshotGetName(current)
                && s.name == QString::fromUtf8(virDomainSnapshotGetName(current)))
                s.isCurrent = true;
            out.push_back(s);
            virDomainSnapshotFree(snaps[i]);
        }
        if (current) virDomainSnapshotFree(current);
        free(snaps);
    }
    virDomainFree(d);
    return out;
}

SnapshotInfo LibvirtBackend::createSnapshot(const QString &uuid, const QString &name,
                                            const QString &description, bool includeMemory) {
    virDomainPtr d = lookup(uuid);
    const QString xml = QStringLiteral(
        "<domainsnapshot><name>%1</name><description>%2</description></domainsnapshot>")
        .arg(name.toHtmlEscaped(), description.toHtmlEscaped());
    unsigned int flags = includeMemory ? 0u : VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY;
    virDomainSnapshotPtr snap = virDomainSnapshotCreateXML(d, xml.toUtf8().constData(), flags);
    virDomainFree(d);
    if (!snap)
        throwLast(QStringLiteral("create snapshot"));
    virDomainSnapshotFree(snap);
    SnapshotInfo s;
    s.name = name;
    s.description = description;
    s.hasMemory = includeMemory;
    s.created = QDateTime::currentDateTime();
    s.isCurrent = true;
    return s;
}

void LibvirtBackend::restoreSnapshot(const QString &uuid, const QString &name) {
    virDomainPtr d = lookup(uuid);
    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(d, name.toUtf8().constData(), 0);
    if (!snap) { virDomainFree(d); throwLast(QStringLiteral("find snapshot")); }
    const int rc = virDomainRevertToSnapshot(snap, 0);
    virDomainSnapshotFree(snap);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("restore snapshot"));
}

void LibvirtBackend::deleteSnapshot(const QString &uuid, const QString &name) {
    virDomainPtr d = lookup(uuid);
    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(d, name.toUtf8().constData(), 0);
    if (!snap) { virDomainFree(d); throwLast(QStringLiteral("find snapshot")); }
    const int rc = virDomainSnapshotDelete(snap, 0);
    virDomainSnapshotFree(snap);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("delete snapshot"));
}

QList<StoragePoolInfo> LibvirtBackend::listStoragePools() {
    QList<StoragePoolInfo> out;
    virStoragePoolPtr *pools = nullptr;
    const int n = virConnectListAllStoragePools(m_conn, &pools, 0);
    if (n < 0) throwLast(QStringLiteral("list storage pools"));
    for (int i = 0; i < n; ++i) {
        StoragePoolInfo p;
        if (const char *nm = virStoragePoolGetName(pools[i]))
            p.name = QString::fromUtf8(nm);
        virStoragePoolInfo info;
        if (virStoragePoolGetInfo(pools[i], &info) == 0) {
            p.active = info.state == VIR_STORAGE_POOL_RUNNING;
            p.capacityBytes = info.capacity;
            p.allocationBytes = info.allocation;
        }
        out.push_back(p);
        virStoragePoolFree(pools[i]);
    }
    free(pools);
    return out;
}

QList<VolumeInfo> LibvirtBackend::listVolumes(const QString &poolName) {
    QList<VolumeInfo> out;
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, poolName.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(poolName));
    virStorageVolPtr *vols = nullptr;
    const int n = virStoragePoolListAllVolumes(pool, &vols, 0);
    for (int i = 0; i < qMax(0, n); ++i) {
        VolumeInfo v;
        if (const char *nm = virStorageVolGetName(vols[i]))
            v.name = QString::fromUtf8(nm);
        if (char *path = virStorageVolGetPath(vols[i])) {
            v.path = QString::fromUtf8(path);
            free(path);
        }
        virStorageVolInfo info;
        if (virStorageVolGetInfo(vols[i], &info) == 0) {
            v.capacityBytes = info.capacity;
            v.allocationBytes = info.allocation;
        }
        out.push_back(v);
        virStorageVolFree(vols[i]);
    }
    if (vols) free(vols);
    virStoragePoolFree(pool);
    return out;
}

StoragePoolInfo LibvirtBackend::createStoragePool(const QString &name, const QString &type,
                                                  const QString &path) {
    const QString t = type.isEmpty() ? QStringLiteral("dir") : type;
    const QString xml = QStringLiteral(
        "<pool type='%1'>\n"
        "  <name>%2</name>\n"
        "  <target><path>%3</path></target>\n"
        "</pool>\n").arg(t, name.toHtmlEscaped(), path.toHtmlEscaped());
    virStoragePoolPtr pool = virStoragePoolDefineXML(m_conn, xml.toUtf8().constData(), 0);
    if (!pool) throwLast(QStringLiteral("define pool %1").arg(name));
    // Build the target dir (harmless if it exists), autostart, and start it.
    virStoragePoolBuild(pool, 0);
    virStoragePoolSetAutostart(pool, 1);
    virStoragePoolCreate(pool, 0);
    StoragePoolInfo p;
    p.name = name;
    p.type = t;
    virStoragePoolInfo info;
    if (virStoragePoolGetInfo(pool, &info) == 0) {
        p.active = info.state == VIR_STORAGE_POOL_RUNNING;
        p.capacityBytes = info.capacity;
        p.allocationBytes = info.allocation;
    }
    virStoragePoolFree(pool);
    return p;
}

void LibvirtBackend::deleteStoragePool(const QString &name, bool deleteContents) {
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, name.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(name));
    if (deleteContents)
        virStoragePoolDelete(pool, VIR_STORAGE_POOL_DELETE_NORMAL);
    virStoragePoolDestroy(pool);          // stop if running (ignore failure)
    const int rc = virStoragePoolUndefine(pool);
    virStoragePoolFree(pool);
    if (rc < 0) throwLast(QStringLiteral("undefine pool %1").arg(name));
}

void LibvirtBackend::setStoragePoolActive(const QString &name, bool active) {
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, name.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(name));
    const int rc = active ? virStoragePoolCreate(pool, 0) : virStoragePoolDestroy(pool);
    virStoragePoolFree(pool);
    if (rc < 0) throwLast(active ? QStringLiteral("start pool") : QStringLiteral("stop pool"));
}

VolumeInfo LibvirtBackend::createVolume(const QString &poolName, const QString &name,
                                        const QString &format, quint64 capacityBytes) {
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, poolName.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(poolName));
    const QString fmt = format.isEmpty() ? QStringLiteral("qcow2") : format;
    const QString fname = name.contains('.') ? name : (name + "." + fmt);
    const QString xml = QStringLiteral(
        "<volume>\n"
        "  <name>%1</name>\n"
        "  <capacity unit='bytes'>%2</capacity>\n"
        "  <target><format type='%3'/></target>\n"
        "</volume>\n").arg(fname.toHtmlEscaped()).arg(capacityBytes).arg(fmt);
    virStorageVolPtr vol = virStorageVolCreateXML(pool, xml.toUtf8().constData(), 0);
    virStoragePoolFree(pool);
    if (!vol) throwLast(QStringLiteral("create volume %1").arg(fname));
    VolumeInfo v;
    v.name = fname;
    v.format = fmt;
    v.capacityBytes = capacityBytes;
    if (char *path = virStorageVolGetPath(vol)) { v.path = QString::fromUtf8(path); free(path); }
    virStorageVolFree(vol);
    return v;
}

void LibvirtBackend::deleteVolume(const QString &poolName, const QString &volumeName) {
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, poolName.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(poolName));
    virStorageVolPtr vol = virStorageVolLookupByName(pool, volumeName.toUtf8().constData());
    virStoragePoolFree(pool);
    if (!vol) throwLast(QStringLiteral("find volume %1").arg(volumeName));
    const int rc = virStorageVolDelete(vol, 0);
    virStorageVolFree(vol);
    if (rc < 0) throwLast(QStringLiteral("delete volume %1").arg(volumeName));
}

QList<NetworkInfo> LibvirtBackend::listNetworks() {
    QList<NetworkInfo> out;
    virNetworkPtr *nets = nullptr;
    const int n = virConnectListAllNetworks(m_conn, &nets, 0);
    if (n < 0) throwLast(QStringLiteral("list networks"));
    for (int i = 0; i < n; ++i) {
        NetworkInfo net;
        if (const char *nm = virNetworkGetName(nets[i]))
            net.name = QString::fromUtf8(nm);
        net.active = virNetworkIsActive(nets[i]) == 1;
        if (char *bridge = virNetworkGetBridgeName(nets[i])) {
            net.bridge = QString::fromUtf8(bridge);
            free(bridge);
        }
        out.push_back(net);
        virNetworkFree(nets[i]);
    }
    free(nets);
    return out;
}

VmInfo LibvirtBackend::importPreparedDisk(const QString &diskPath, const VmCreateRequest &req) {
    const QString xml = buildDomainXml(req, diskPath);
    virDomainPtr dom = virDomainDefineXML(m_conn, xml.toUtf8().constData());
    if (!dom)
        throwLast(QStringLiteral("define imported domain"));
    VmInfo v = toVmInfo(dom);
    virDomainFree(dom);
    return v;
}

} // namespace vmm
