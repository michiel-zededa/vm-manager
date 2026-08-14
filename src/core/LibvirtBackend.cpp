#include "LibvirtBackend.h"

#include <libvirt/virterror.h>

#include <QUuid>
#include <QDateTime>
#include <QByteArray>
#include <QRegularExpression>

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

LibvirtBackend::LibvirtBackend(QString uri, QString displayName,
                               QString username, QString password)
    : m_uri(std::move(uri)), m_displayName(std::move(displayName)),
      m_username(std::move(username)), m_password(std::move(password)) {}

LibvirtBackend::~LibvirtBackend() { close(); }

namespace {
// Credential callback for virConnectOpenAuth: supplies the stored username /
// password so libssh2 password auth can proceed without a TTY prompt.
struct AuthData { QByteArray user; QByteArray pass; };

int authCallback(virConnectCredentialPtr cred, unsigned int ncred, void *cbdata) {
    auto *data = static_cast<AuthData *>(cbdata);
    for (unsigned int i = 0; i < ncred; ++i) {
        QByteArray value;
        switch (cred[i].type) {
        case VIR_CRED_AUTHNAME:
        case VIR_CRED_USERNAME:
        case VIR_CRED_ECHOPROMPT:
            value = data->user;
            break;
        case VIR_CRED_PASSPHRASE:
        case VIR_CRED_NOECHOPROMPT:
            value = data->pass;
            break;
        default:
            break;
        }
        if (value.isEmpty() && cred[i].defresult)
            value = QByteArray(cred[i].defresult);
        cred[i].result = strdup(value.constData());
        if (!cred[i].result)
            return -1;
        cred[i].resultlen = uint(value.length());
    }
    return 0;
}
} // namespace

void LibvirtBackend::throwLast(const QString &context) {
    virErrorPtr err = virGetLastError();
    const QString msg = (err && err->message) ? QString::fromUtf8(err->message)
                                              : QStringLiteral("unknown libvirt error");
    throw BackendError(QStringLiteral("%1: %2").arg(context, msg));
}

void LibvirtBackend::open() {
    if (m_conn)
        return;
    if (!m_password.isEmpty()) {
        // Password auth (libssh2 transport) needs an auth callback.
        AuthData data{ m_username.toUtf8(), m_password.toUtf8() };
        int credTypes[] = {
            VIR_CRED_AUTHNAME, VIR_CRED_USERNAME, VIR_CRED_ECHOPROMPT,
            VIR_CRED_PASSPHRASE, VIR_CRED_NOECHOPROMPT, VIR_CRED_REALM,
        };
        virConnectAuth auth;
        auth.credtype = credTypes;
        auth.ncredtype = int(sizeof(credTypes) / sizeof(credTypes[0]));
        auth.cb = authCallback;
        auth.cbdata = &data;
        m_conn = virConnectOpenAuth(m_uri.toUtf8().constData(), &auth, 0);
    } else {
        m_conn = virConnectOpen(m_uri.toUtf8().constData());
    }
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
    sampleRates(dom, v);
    return v;
}

void LibvirtBackend::sampleRates(virDomainPtr dom, VmInfo &v) {
    if (v.state != VmState::Running) { m_domSamples.remove(v.uuid); return; }

    virDomainPtr doms[2] = { dom, nullptr };
    virDomainStatsRecordPtr *records = nullptr;
    const unsigned int flags = 0;
    const unsigned int stats = VIR_DOMAIN_STATS_CPU_TOTAL
                             | VIR_DOMAIN_STATS_BLOCK
                             | VIR_DOMAIN_STATS_INTERFACE;
    const int n = virDomainListGetStats(doms, stats, &records, flags);
    if (n <= 0 || !records) { if (records) virDomainStatsRecordListFree(records); return; }

    unsigned long long cpuNs = 0, rd = 0, wr = 0, rx = 0, tx = 0;
    virDomainStatsRecordPtr rec = records[0];
    for (int i = 0; i < rec->nparams; ++i) {
        const virTypedParameter &p = rec->params[i];
        unsigned long long val = 0;
        switch (p.type) {
        case VIR_TYPED_PARAM_ULLONG: val = p.value.ul; break;
        case VIR_TYPED_PARAM_UINT:   val = p.value.ui; break;
        case VIR_TYPED_PARAM_LLONG:  val = static_cast<unsigned long long>(p.value.l); break;
        case VIR_TYPED_PARAM_INT:    val = static_cast<unsigned long long>(p.value.i); break;
        default: break;
        }
        const QByteArray f(p.field);
        if (f == "cpu.time") cpuNs = val;
        else if (f.endsWith(".rd.bytes")) rd += val;
        else if (f.endsWith(".wr.bytes")) wr += val;
        else if (f.endsWith(".rx.bytes")) rx += val;
        else if (f.endsWith(".tx.bytes")) tx += val;
    }
    virDomainStatsRecordListFree(records);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto it = m_domSamples.constFind(v.uuid);
    if (it != m_domSamples.constEnd() && it->wallMs > 0 && now > it->wallMs) {
        const double dt = (now - it->wallMs) / 1000.0;   // seconds
        if (dt > 0) {
            const int vc = qMax(1, v.vcpus);
            const double dCpu = double(cpuNs) - double(it->cpuNs);
            v.stats.cpuPercent   = qBound(0.0, (dCpu / (dt * 1e9)) / vc * 100.0, 100.0);
            v.stats.diskReadBps  = quint64(qMax(0.0, (double(rd) - double(it->rdBytes)) / dt));
            v.stats.diskWriteBps = quint64(qMax(0.0, (double(wr) - double(it->wrBytes)) / dt));
            v.stats.netRxBps     = quint64(qMax(0.0, (double(rx) - double(it->rxBytes)) / dt));
            v.stats.netTxBps     = quint64(qMax(0.0, (double(tx) - double(it->txBytes)) / dt));
        }
    }
    m_domSamples.insert(v.uuid, DomSample{ now, cpuNs, rd, wr, rx, tx });
    v.stats.timestamp = QDateTime::currentDateTime();
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

    // Host memory usage: total - (free + buffers + cached).
    virNodeMemoryStatsPtr mstats = nullptr;
    int nparams = 0;
    if (virNodeGetMemoryStats(m_conn, VIR_NODE_MEMORY_STATS_ALL_CELLS, nullptr, &nparams, 0) == 0
        && nparams > 0) {
        mstats = static_cast<virNodeMemoryStatsPtr>(calloc(nparams, sizeof(virNodeMemoryStats)));
        if (mstats && virNodeGetMemoryStats(m_conn, VIR_NODE_MEMORY_STATS_ALL_CELLS,
                                            mstats, &nparams, 0) == 0) {
            quint64 total = 0, avail = 0;
            for (int i = 0; i < nparams; ++i) {
                const QString field = QString::fromLatin1(mstats[i].field);
                if (field == QLatin1String(VIR_NODE_MEMORY_STATS_TOTAL)) total = mstats[i].value;
                else if (field == QLatin1String(VIR_NODE_MEMORY_STATS_FREE)
                      || field == QLatin1String(VIR_NODE_MEMORY_STATS_BUFFERS)
                      || field == QLatin1String(VIR_NODE_MEMORY_STATS_CACHED)) avail += mstats[i].value;
            }
            if (total > 0 && total >= avail) h.hostMemUsedKiB = total - avail;
        }
        free(mstats);
    }

    // Host CPU utilisation via delta between two virNodeGetCPUStats samples.
    int ncpu = 0;
    if (virNodeGetCPUStats(m_conn, VIR_NODE_CPU_STATS_ALL_CPUS, nullptr, &ncpu, 0) == 0 && ncpu > 0) {
        virNodeCPUStatsPtr cstats = static_cast<virNodeCPUStatsPtr>(calloc(ncpu, sizeof(virNodeCPUStats)));
        if (cstats && virNodeGetCPUStats(m_conn, VIR_NODE_CPU_STATS_ALL_CPUS, cstats, &ncpu, 0) == 0) {
            unsigned long long total = 0, idle = 0;
            for (int i = 0; i < ncpu; ++i) {
                total += cstats[i].value;
                if (QString::fromLatin1(cstats[i].field) == QLatin1String(VIR_NODE_CPU_STATS_IDLE))
                    idle = cstats[i].value;
            }
            if (m_prevCpuTotal > 0 && total > m_prevCpuTotal) {
                const double dt = double(total - m_prevCpuTotal);
                const double di = double(idle - m_prevCpuIdle);
                h.hostCpuPercent = dt > 0 ? qBound(0.0, (1.0 - di / dt) * 100.0, 100.0) : 0.0;
            }
            m_prevCpuTotal = total;
            m_prevCpuIdle = idle;
        }
        free(cstats);
    }

    // Aggregate storage across all pools (disk usage).
    virStoragePoolPtr *spools = nullptr;
    const int nsp = virConnectListAllStoragePools(m_conn, &spools, 0);
    for (int i = 0; i < qMax(0, nsp); ++i) {
        virStoragePoolInfo pi;
        if (virStoragePoolGetInfo(spools[i], &pi) == 0) {
            h.storageCapacityBytes += pi.capacity;
            h.storageAllocationBytes += pi.allocation;
        }
        virStoragePoolFree(spools[i]);
    }
    if (spools) free(spools);

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

    // Optionally delete the domain's disk images first (while it's still defined
    // so we can read its XML).
    if (removeStorage) {
        if (char *xml = virDomainGetXMLDesc(d, 0)) {
            const QString s = QString::fromUtf8(xml);
            free(xml);
            int pos = 0;
            while (true) {
                const int di = s.indexOf(QLatin1String("<disk"), pos);
                if (di < 0) break;
                const int de = s.indexOf(QLatin1String("</disk>"), di);
                if (de < 0) break;
                const QString disk = s.mid(di, de - di);
                pos = de + 7;
                if (!disk.contains(QLatin1String("device='disk'")))
                    continue;                          // skip cdrom/floppy
                QString path;
                int a = disk.indexOf(QLatin1String("file='"));
                if (a >= 0) { const int e = disk.indexOf('\'', a + 6); path = disk.mid(a + 6, e - (a + 6)); }
                else { a = disk.indexOf(QLatin1String("dev='")); if (a >= 0) { const int e = disk.indexOf('\'', a + 5); path = disk.mid(a + 5, e - (a + 5)); } }
                if (path.isEmpty()) continue;
                if (virStorageVolPtr vol = virStorageVolLookupByPath(m_conn, path.toUtf8().constData())) {
                    virStorageVolDelete(vol, 0);       // best-effort
                    virStorageVolFree(vol);
                }
            }
        }
    }

    unsigned int flags = VIR_DOMAIN_UNDEFINE_MANAGED_SAVE | VIR_DOMAIN_UNDEFINE_NVRAM
                       | VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA;
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

VmInfo LibvirtBackend::clone(const QString &uuid, const QString &newName, bool linked) {
    virDomainPtr src = lookup(uuid);
    char *xmlc = virDomainGetXMLDesc(src, VIR_DOMAIN_XML_INACTIVE);
    virDomainFree(src);
    if (!xmlc) throwLast(QStringLiteral("read source domain"));
    QString xml = QString::fromUtf8(xmlc);
    free(xmlc);

    // New name; drop uuid + MACs so libvirt regenerates them.
    { const int a = xml.indexOf(QLatin1String("<name>")); const int b = xml.indexOf(QLatin1String("</name>"), a);
      if (a >= 0 && b > a) xml.replace(a + 6, b - (a + 6), newName.toHtmlEscaped()); }
    { const int a = xml.indexOf(QLatin1String("<uuid>")); const int b = xml.indexOf(QLatin1String("</uuid>"), a);
      if (a >= 0 && b > a) xml.remove(a, (b + 7) - a); }
    xml.remove(QRegularExpression(QStringLiteral("\\s*<mac address='[^']*'\\s*/>")));

    // Clone each writable disk's backing volume and repoint the source path.
    int pos = 0;
    while (true) {
        const int di = xml.indexOf(QLatin1String("<disk"), pos); if (di < 0) break;
        const int de = xml.indexOf(QLatin1String("</disk>"), di); if (de < 0) break;
        const QString disk = xml.mid(di, de - di);
        pos = de + 7;
        if (!disk.contains(QLatin1String("device='disk'"))) continue;
        const int fa = disk.indexOf(QLatin1String("file='"));
        if (fa < 0) continue;
        const int fe = disk.indexOf('\'', fa + 6);
        const QString srcPath = disk.mid(fa + 6, fe - (fa + 6));

        virStorageVolPtr sv = virStorageVolLookupByPath(m_conn, srcPath.toUtf8().constData());
        if (!sv) continue;
        virStoragePoolPtr pool = virStoragePoolLookupByVolume(sv);
        if (!pool) { virStorageVolFree(sv); continue; }

        quint64 cap = 20ull * 1024 * 1024 * 1024;
        virStorageVolInfo vi;
        if (virStorageVolGetInfo(sv, &vi) == 0) cap = vi.capacity;
        const QString fname = srcPath.section('/', -1);
        const QString newVolName = newName + QLatin1Char('-') + fname;

        QString volXml;
        virStorageVolPtr nv = nullptr;
        if (linked) {
            volXml = QStringLiteral(
                "<volume><name>%1</name><capacity>%2</capacity>"
                "<target><format type='qcow2'/></target>"
                "<backingStore><path>%3</path><format type='qcow2'/></backingStore></volume>")
                .arg(newVolName.toHtmlEscaped()).arg(cap).arg(srcPath.toHtmlEscaped());
            nv = virStorageVolCreateXML(pool, volXml.toUtf8().constData(), 0);
        } else {
            volXml = QStringLiteral(
                "<volume><name>%1</name><capacity>%2</capacity>"
                "<target><format type='qcow2'/></target></volume>")
                .arg(newVolName.toHtmlEscaped()).arg(cap);
            nv = virStorageVolCreateXMLFrom(pool, volXml.toUtf8().constData(), sv, 0);
        }

        if (nv) {
            if (char *np = virStorageVolGetPath(nv)) {
                xml.replace(srcPath, QString::fromUtf8(np));
                free(np);
            }
            virStorageVolFree(nv);
        }
        virStoragePoolFree(pool);
        virStorageVolFree(sv);
    }

    virDomainPtr nd = virDomainDefineXML(m_conn, xml.toUtf8().constData());
    if (!nd) throwLast(QStringLiteral("define clone"));
    VmInfo v = toVmInfo(nd);
    virDomainFree(nd);
    return v;
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
        int autostart = 0;
        if (virStoragePoolGetAutostart(pools[i], &autostart) == 0)
            p.autostart = autostart != 0;
        if (char *xml = virStoragePoolGetXMLDesc(pools[i], 0)) {
            const QString s = QString::fromUtf8(xml);
            free(xml);
            const int a = s.indexOf(QLatin1String("<path>"));
            const int b = a >= 0 ? s.indexOf(QLatin1String("</path>"), a) : -1;
            if (a >= 0 && b > a) p.targetPath = s.mid(a + 6, b - (a + 6)).trimmed();
            const int t = s.indexOf(QLatin1String("type='"));
            if (t >= 0) { const int e = s.indexOf('\'', t + 6); if (e > t) p.type = s.mid(t + 6, e - (t + 6)); }
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

ConsoleInfo LibvirtBackend::consoleInfo(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    ConsoleInfo c;
    int state = 0, reason = 0;
    if (virDomainGetState(d, &state, &reason, 0) == 0)
        c.running = state == VIR_DOMAIN_RUNNING;

    if (char *xml = virDomainGetXMLDesc(d, 0)) {
        const QString s = QString::fromUtf8(xml);
        free(xml);
        // Parse the <graphics .../> element attributes (vnc/spice, port, listen).
        const int gi = s.indexOf(QLatin1String("<graphics"));
        if (gi >= 0) {
            const int ge = s.indexOf('>', gi);
            const QString tag = s.mid(gi, ge - gi);
            const auto attr = [&tag](const QString &name) -> QString {
                const QString key = name + QStringLiteral("='");
                int a = tag.indexOf(key);
                if (a < 0) return {};
                a += key.size();
                const int b = tag.indexOf('\'', a);
                return b > a ? tag.mid(a, b - a) : QString();
            };
            c.graphicsType = attr(QStringLiteral("type"));
            const QString port = attr(QStringLiteral("port"));
            bool ok = false;
            const int p = port.toInt(&ok);
            if (ok && p > 0) c.port = p;
            c.host = attr(QStringLiteral("listen"));
            if (c.host.isEmpty() || c.host == QLatin1String("0.0.0.0"))
                c.host = QStringLiteral("127.0.0.1");
        }
        c.hasSerial = s.contains(QLatin1String("<serial")) || s.contains(QLatin1String("<console"));
    }
    virDomainFree(d);
    return c;
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
