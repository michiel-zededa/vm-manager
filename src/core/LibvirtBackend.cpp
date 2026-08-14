#include "LibvirtBackend.h"

#include <libvirt/virterror.h>

#include <QUuid>
#include <QDateTime>
#include <QByteArray>
#include <QRegularExpression>
#include <QProcess>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

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

QString LibvirtBackend::buildDomainXml(const VmCreateRequest &req, const QString &diskPath,
                                       const QString &seedPath) {
    // OS-aware domain. The wizard's OS selection drives sensible hardware:
    //  - Windows: UEFI firmware; Windows 11 also gets a TPM 2.0 + Secure Boot.
    //    SATA disk + e1000e NIC so the installer boots without extra drivers.
    //  - Linux/other: virtio disk + NIC (fast, drivers built-in).
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString osv = req.osVariant.toLower();
    const bool isWindows = osv.startsWith(QLatin1String("win"));
    const bool isWin11 = osv.contains(QLatin1String("win11")) || osv.contains(QLatin1String("windows11"));
    // UEFI if requested, or implied by Windows (11 requires it).
    const bool uefi = req.firmware.compare("uefi", Qt::CaseInsensitive) == 0 || isWindows;

    const QString diskBus = isWindows ? QStringLiteral("sata") : QStringLiteral("virtio");
    const QString diskDev = isWindows ? QStringLiteral("sda")  : QStringLiteral("vda");
    const QString nicModel = isWindows ? QStringLiteral("e1000e") : QStringLiteral("virtio");
    const QString video = isWindows ? QStringLiteral("vga") : QStringLiteral("virtio");

    QString media;
    if (!req.installMediaPath.isEmpty()) {
        media = QStringLiteral(
            "    <disk type='file' device='cdrom'>\n"
            "      <driver name='qemu' type='raw'/>\n"
            "      <source file='%1'/>\n"
            "      <target dev='sdc' bus='sata'/>\n"
            "      <readonly/>\n"
            "    </disk>\n").arg(req.installMediaPath.toHtmlEscaped());
    }
    if (!seedPath.isEmpty()) {
        media += QStringLiteral(
            "    <disk type='file' device='cdrom'>\n"
            "      <driver name='qemu' type='raw'/>\n"
            "      <source file='%1'/>\n"
            "      <target dev='sdd' bus='sata'/>\n"
            "      <readonly/>\n"
            "    </disk>\n").arg(seedPath.toHtmlEscaped());
    }

    const QString firmwareAttr = uefi ? QStringLiteral(" firmware='efi'") : QString();
    QString osExtra;
    if (uefi && isWin11)
        osExtra = QStringLiteral("    <loader secure='yes'/>\n");
    QString tpm;
    if (isWin11)
        tpm = QStringLiteral("    <tpm model='tpm-crb'><backend type='emulator' version='2.0'/></tpm>\n");
    // Secure Boot needs SMM.
    const QString features = isWin11
        ? QStringLiteral("  <features><acpi/><apic/><smm state='on'/></features>\n")
        : QStringLiteral("  <features><acpi/><apic/></features>\n");

    return QStringLiteral(
        "<domain type='kvm'>\n"
        "  <name>%1</name>\n"
        "  <uuid>%2</uuid>\n"
        "  <memory unit='MiB'>%3</memory>\n"
        "  <currentMemory unit='MiB'>%3</currentMemory>\n"
        "  <vcpu>%4</vcpu>\n"
        "  <os%5>\n"
        "    <type arch='x86_64' machine='q35'>hvm</type>\n"
        "%6"
        "    <boot dev='hd'/>\n"
        "    <boot dev='cdrom'/>\n"
        "  </os>\n"
        "%7"
        "  <cpu mode='host-passthrough'/>\n"
        "  <clock offset='%8'/>\n"
        "  <devices>\n"
        "    <disk type='file' device='disk'>\n"
        "      <driver name='qemu' type='%9'/>\n"
        "      <source file='%10'/>\n"
        "      <target dev='%11' bus='%12'/>\n"
        "    </disk>\n"
        "%13"
        "    <interface type='network'>\n"
        "      <source network='%14'/>\n"
        "      <model type='%15'/>\n"
        "    </interface>\n"
        "%16"
        "    <video><model type='%17'/></video>\n"
        "    <graphics type='vnc' port='-1' autoport='yes' listen='127.0.0.1'/>\n"
        "    <console type='pty'/>\n"
        "    <channel type='unix'><target type='virtio' name='org.qemu.guest_agent.0'/></channel>\n"
        "  </devices>\n"
        "</domain>\n")
        .arg(req.name.toHtmlEscaped(), uuid)
        .arg(req.memoryMiB)
        .arg(req.vcpus)
        .arg(firmwareAttr, osExtra, features,
             isWindows ? QStringLiteral("localtime") : QStringLiteral("utc"),
             req.diskFormat, diskPath.toHtmlEscaped())
        .arg(diskDev, diskBus, media, req.networkName.toHtmlEscaped(), nicModel, tpm, video);
}

QString LibvirtBackend::buildCloudInitSeed(const VmCreateRequest &req) {
    // The seed file must be on the hypervisor host; only wire it for local URIs.
    if (!m_uri.contains(QLatin1String("///")))
        return {};

    const QString hostname = req.ciHostname.isEmpty() ? req.name : req.ciHostname;
    QString userData = QStringLiteral("#cloud-config\nhostname: %1\nmanage_etc_hosts: true\n").arg(hostname);
    if (!req.ciUser.isEmpty()) {
        userData += QStringLiteral(
            "users:\n  - name: %1\n    sudo: ALL=(ALL) NOPASSWD:ALL\n"
            "    shell: /bin/bash\n    lock_passwd: false\n").arg(req.ciUser);
        if (!req.ciSshKey.trimmed().isEmpty())
            userData += QStringLiteral("    ssh_authorized_keys:\n      - %1\n").arg(req.ciSshKey.trimmed());
    }
    if (!req.ciPassword.isEmpty() && !req.ciUser.isEmpty())
        userData += QStringLiteral("chpasswd:\n  expire: false\n  list: |\n    %1:%2\nssh_pwauth: true\n")
                        .arg(req.ciUser, req.ciPassword);
    const QString metaData = QStringLiteral("instance-id: %1\nlocal-hostname: %2\n").arg(req.name, hostname);

    QTemporaryDir tmp;
    if (!tmp.isValid()) return {};
    { QFile f(tmp.filePath("user-data")); if (f.open(QIODevice::WriteOnly)) f.write(userData.toUtf8()); }
    { QFile f(tmp.filePath("meta-data")); if (f.open(QIODevice::WriteOnly)) f.write(metaData.toUtf8()); }

    const QString outDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/seeds";
    QDir().mkpath(outDir);
    const QString seedPath = QDir(outDir).filePath(req.name + "-seed.iso");
    QFile::remove(seedPath);

    struct Cmd { QString prog; QStringList args; };
    QList<Cmd> cmds;
    if (const QString c = QStandardPaths::findExecutable("cloud-localds"); !c.isEmpty())
        cmds.push_back({c, {seedPath, tmp.filePath("user-data"), tmp.filePath("meta-data")}});
    for (const char *t : {"genisoimage", "mkisofs", "xorrisofs"})
        if (const QString e = QStandardPaths::findExecutable(t); !e.isEmpty())
            cmds.push_back({e, {"-output", seedPath, "-volid", "cidata", "-joliet", "-rock",
                                tmp.filePath("user-data"), tmp.filePath("meta-data")}});
    if (const QString h = QStandardPaths::findExecutable("hdiutil"); !h.isEmpty())
        cmds.push_back({h, {"makehybrid", "-iso", "-joliet", "-default-volume-name", "cidata",
                            "-o", seedPath, tmp.path()}});

    for (const Cmd &c : cmds) {
        QProcess p;
        p.start(c.prog, c.args);
        p.waitForFinished(30000);
        if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0 && QFileInfo::exists(seedPath))
            return seedPath;
    }
    return {};   // no ISO tool available — VM still defines, just without the seed
}

VmInfo LibvirtBackend::define(const VmCreateRequest &req) {
    // NOTE: defines the domain; the backing volume is created separately (or
    // via importPreparedDisk). Cloud-init attaches a NoCloud seed on local hosts.
    const QString diskPath = QStringLiteral("/var/lib/libvirt/images/%1.%2")
                                 .arg(req.name, req.diskFormat);
    const QString seed = req.cloudInit ? buildCloudInitSeed(req) : QString();
    const QString xml = buildDomainXml(req, diskPath, seed);
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

NetworkInfo LibvirtBackend::createNetwork(const QString &name, const QString &mode,
                                          const QString &forwardDev) {
    const QString m = mode.isEmpty() ? QStringLiteral("nat") : mode;
    // Pick a deterministic private subnet from the name to avoid clashes.
    const int octet = 100 + int(qHash(name) % 50);
    const QString bridge = QStringLiteral("virbr-%1").arg(name.left(8));
    QString xml;
    if (m == QLatin1String("bridge")) {
        // Bridge straight onto an existing host bridge/NIC — no NAT/DHCP.
        xml = QStringLiteral(
            "<network>\n  <name>%1</name>\n  <forward mode='bridge'/>\n"
            "  <bridge name='%2'/>\n</network>\n").arg(name.toHtmlEscaped(), forwardDev.toHtmlEscaped());
    } else {
        const QString forward = (m == QLatin1String("isolated"))
            ? QString()
            : QStringLiteral("  <forward mode='%1'%2/>\n").arg(m,
                  forwardDev.isEmpty() ? QString() : QStringLiteral(" dev='%1'").arg(forwardDev.toHtmlEscaped()));
        xml = QStringLiteral(
            "<network>\n  <name>%1</name>\n%2"
            "  <bridge name='%3' stp='on' delay='0'/>\n"
            "  <ip address='192.168.%4.1' netmask='255.255.255.0'>\n"
            "    <dhcp><range start='192.168.%4.2' end='192.168.%4.254'/></dhcp>\n"
            "  </ip>\n</network>\n")
            .arg(name.toHtmlEscaped(), forward, bridge).arg(octet);
    }
    virNetworkPtr net = virNetworkDefineXML(m_conn, xml.toUtf8().constData());
    if (!net) throwLast(QStringLiteral("define network %1").arg(name));
    virNetworkSetAutostart(net, 1);
    virNetworkCreate(net);
    NetworkInfo n;
    n.name = name;
    n.mode = m;
    n.active = virNetworkIsActive(net) == 1;
    if (char *b = virNetworkGetBridgeName(net)) { n.bridge = QString::fromUtf8(b); free(b); }
    n.forwardDev = forwardDev;
    virNetworkFree(net);
    return n;
}

void LibvirtBackend::deleteNetwork(const QString &name) {
    virNetworkPtr net = virNetworkLookupByName(m_conn, name.toUtf8().constData());
    if (!net) throwLast(QStringLiteral("find network %1").arg(name));
    if (virNetworkIsActive(net) == 1)
        virNetworkDestroy(net);
    const int rc = virNetworkUndefine(net);
    virNetworkFree(net);
    if (rc < 0) throwLast(QStringLiteral("undefine network %1").arg(name));
}

void LibvirtBackend::setNetworkActive(const QString &name, bool active) {
    virNetworkPtr net = virNetworkLookupByName(m_conn, name.toUtf8().constData());
    if (!net) throwLast(QStringLiteral("find network %1").arg(name));
    const int rc = active ? virNetworkCreate(net) : virNetworkDestroy(net);
    virNetworkFree(net);
    if (rc < 0) throwLast(active ? QStringLiteral("start network") : QStringLiteral("stop network"));
}

QList<DiskInfo> LibvirtBackend::listDisks(const QString &uuid) {
    virDomainPtr d = lookup(uuid);
    QList<DiskInfo> out;
    if (char *xml = virDomainGetXMLDesc(d, 0)) {
        const QString s = QString::fromUtf8(xml);
        free(xml);
        const auto attr = [](const QString &tag, const QString &key) -> QString {
            const QString k = key + QStringLiteral("='");
            int a = tag.indexOf(k);
            if (a < 0) return {};
            a += k.size();
            const int b = tag.indexOf('\'', a);
            return b > a ? tag.mid(a, b - a) : QString();
        };
        int pos = 0;
        while (true) {
            const int di = s.indexOf(QLatin1String("<disk"), pos);
            if (di < 0) break;
            const int de = s.indexOf(QLatin1String("</disk>"), di);
            if (de < 0) break;
            const QString disk = s.mid(di, de - di);
            pos = de + 7;
            DiskInfo info;
            info.device = attr(disk, QStringLiteral("device"));
            // target dev + bus
            const int ti = disk.indexOf(QLatin1String("<target"));
            if (ti >= 0) {
                const QString t = disk.mid(ti, disk.indexOf('>', ti) - ti);
                info.target = attr(t, QStringLiteral("dev"));
                info.bus = attr(t, QStringLiteral("bus"));
            }
            // driver type (format)
            const int dri = disk.indexOf(QLatin1String("<driver"));
            if (dri >= 0) {
                const QString dr = disk.mid(dri, disk.indexOf('>', dri) - dri);
                info.format = attr(dr, QStringLiteral("type"));
            }
            // source file / dev
            const int si = disk.indexOf(QLatin1String("<source"));
            if (si >= 0) {
                const QString src = disk.mid(si, disk.indexOf('>', si) - si);
                info.path = attr(src, QStringLiteral("file"));
                if (info.path.isEmpty()) info.path = attr(src, QStringLiteral("dev"));
            }
            if (!info.path.isEmpty()) {
                if (virStorageVolPtr v = virStorageVolLookupByPath(m_conn, info.path.toUtf8().constData())) {
                    virStorageVolInfo vi;
                    if (virStorageVolGetInfo(v, &vi) == 0) info.capacityBytes = vi.capacity;
                    virStorageVolFree(v);
                }
            }
            out.push_back(info);
        }
    }
    virDomainFree(d);
    return out;
}

void LibvirtBackend::attachDisk(const QString &uuid, const QString &volumePath,
                                const QString &bus, const QString &format) {
    virDomainPtr d = lookup(uuid);
    // Choose the next free target dev on the bus.
    const QString prefix = (bus == QLatin1String("virtio")) ? QStringLiteral("vd") : QStringLiteral("sd");
    QList<DiskInfo> disks = listDisks(uuid);
    char letter = 'a';
    while (true) {
        const QString cand = prefix + QChar(letter);
        bool used = false;
        for (const auto &di : disks) if (di.target == cand) { used = true; break; }
        if (!used) break;
        if (letter++ >= 'z') break;
    }
    const QString target = prefix + QChar(letter);
    const QString xml = QStringLiteral(
        "<disk type='file' device='disk'>\n"
        "  <driver name='qemu' type='%1'/>\n"
        "  <source file='%2'/>\n"
        "  <target dev='%3' bus='%4'/>\n"
        "</disk>\n")
        .arg(format.isEmpty() ? QStringLiteral("qcow2") : format,
             volumePath.toHtmlEscaped(), target,
             bus.isEmpty() ? QStringLiteral("virtio") : bus);
    unsigned int flags = VIR_DOMAIN_AFFECT_CONFIG;
    if (virDomainIsActive(d) == 1) flags |= VIR_DOMAIN_AFFECT_LIVE;
    const int rc = virDomainAttachDeviceFlags(d, xml.toUtf8().constData(), flags);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("attach disk"));
}

void LibvirtBackend::detachDisk(const QString &uuid, const QString &target) {
    virDomainPtr d = lookup(uuid);
    const QString xml = QStringLiteral(
        "<disk type='file' device='disk'><target dev='%1'/></disk>\n").arg(target.toHtmlEscaped());
    unsigned int flags = VIR_DOMAIN_AFFECT_CONFIG;
    if (virDomainIsActive(d) == 1) flags |= VIR_DOMAIN_AFFECT_LIVE;
    const int rc = virDomainDetachDeviceFlags(d, xml.toUtf8().constData(), flags);
    virDomainFree(d);
    if (rc < 0) throwLast(QStringLiteral("detach disk"));
}

void LibvirtBackend::resizeVolume(const QString &poolName, const QString &volumeName, quint64 capacityBytes) {
    virStoragePoolPtr pool = virStoragePoolLookupByName(m_conn, poolName.toUtf8().constData());
    if (!pool) throwLast(QStringLiteral("find pool %1").arg(poolName));
    virStorageVolPtr vol = virStorageVolLookupByName(pool, volumeName.toUtf8().constData());
    virStoragePoolFree(pool);
    if (!vol) throwLast(QStringLiteral("find volume %1").arg(volumeName));
    const int rc = virStorageVolResize(vol, capacityBytes, 0);
    virStorageVolFree(vol);
    if (rc < 0) throwLast(QStringLiteral("resize volume %1").arg(volumeName));
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
