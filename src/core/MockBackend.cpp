#include "MockBackend.h"

#include <QUuid>
#include <QMutexLocker>
#include <algorithm>

namespace vmm {

namespace {
quint64 gib(quint64 g) { return g * 1024ull * 1024ull; }   // -> KiB
quint64 mib(quint64 m) { return m * 1024ull; }             // -> KiB

VmInfo makeVm(const QString &conn, const QString &name, const QString &os,
              VmState state, int vcpus, quint64 memMiB, const QString &title) {
    VmInfo v;
    v.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    v.connectionId = conn;
    v.name = name;
    v.osLabel = os;
    v.state = state;
    v.vcpus = vcpus;
    v.memoryMaxKiB = mib(memMiB);
    v.memoryCurrentKiB = state == VmState::Running ? mib(memMiB) : 0;
    v.title = title;
    v.autostart = false;
    return v;
}
} // namespace

MockBackend::MockBackend(QString connectionId, QString displayName, bool demo)
    : m_connectionId(std::move(connectionId)), m_displayName(std::move(displayName)),
      m_demo(demo) {}

void MockBackend::open() {
    QMutexLocker lock(&m_mutex);
    if (m_open)
        return;
    m_open = true;

    // Non-demo mock (a libvirt fallback) stays empty — never fabricate VMs.
    if (!m_demo)
        return;

    const auto add = [&](VmInfo v) { m_vms.insert(v.uuid, v); };
    add(makeVm(m_connectionId, "ubuntu-dev",   "Ubuntu 24.04 LTS", VmState::Running, 4, 8192,  "Primary dev box"));
    add(makeVm(m_connectionId, "win11-test",   "Windows 11",       VmState::Running, 4, 8192,  "QA — Windows target"));
    add(makeVm(m_connectionId, "fedora-build", "Fedora 40",        VmState::ShutOff, 8, 16384, "CI builder"));
    add(makeVm(m_connectionId, "debian-router","Debian 12",        VmState::Running, 2, 2048,  "pfSense-style router"));
    add(makeVm(m_connectionId, "k3s-node-1",   "Alpine 3.20",      VmState::Paused,  2, 4096,  "k3s cluster node"));

    VmInfo tmpl = makeVm(m_connectionId, "golden-ubuntu", "Ubuntu 24.04 LTS", VmState::ShutOff, 2, 4096, "Golden template");
    tmpl.isTemplate = true;
    add(tmpl);

    m_pools = {
        {"default",  "dir", 512ull*1024*1024*1024, 210ull*1024*1024*1024, true,
         "/var/lib/libvirt/images", true},
        {"fast-nvme","dir", 1024ull*1024*1024*1024, 640ull*1024*1024*1024, true,
         "/mnt/nvme/images", false},
    };
    const auto vol = [](const QString &n, const QString &dir, quint64 capG, quint64 allocG) {
        return VolumeInfo{n, dir + "/" + n, "qcow2",
                          capG*1024*1024*1024, allocG*1024*1024*1024};
    };
    m_volumes["default"] = {
        vol("ubuntu-dev.qcow2",    "/var/lib/libvirt/images", 40, 12),
        vol("win11-test.qcow2",    "/var/lib/libvirt/images", 80, 41),
        vol("debian-router.qcow2", "/var/lib/libvirt/images", 20, 4),
    };
    m_volumes["fast-nvme"] = {
        vol("fedora-build.qcow2", "/mnt/nvme/images", 120, 96),
        vol("k3s-node-1.qcow2",   "/mnt/nvme/images", 40, 18),
    };
    m_networks = {
        {"default", "nat",    "virbr0", true,  "eth0"},
        {"isolated","isolated","virbr1", true,  {}},
        {"lab-br0", "bridge", "br0",    true,  "eno1"},
    };
}

void MockBackend::close() { QMutexLocker lock(&m_mutex); m_open = false; }
bool MockBackend::isOpen() const { return m_open; }

HostInfo MockBackend::hostInfo() {
    QMutexLocker lock(&m_mutex);
    HostInfo h;
    h.id = m_connectionId;
    h.uri = m_connectionId;
    h.displayName = m_displayName;
    h.connected = m_open;
    h.isLocal = m_connectionId.contains("///");
    h.hypervisor = "QEMU/KVM";
    h.hostArch = "x86_64";
    h.hostCpus = 16;
    h.hostMemoryKiB = gib(64);
    h.hostMemUsedKiB = gib(21);
    h.totalVms = m_vms.size();
    h.activeVms = std::count_if(m_vms.cbegin(), m_vms.cend(),
        [](const VmInfo &v){ return v.state == VmState::Running; });
    return h;
}

VmInfo &MockBackend::require(const QString &uuid) {
    auto it = m_vms.find(uuid);
    if (it == m_vms.end())
        throw BackendError(QStringLiteral("No such VM: %1").arg(uuid));
    return it.value();
}

QList<VmInfo> MockBackend::listVms() {
    QMutexLocker lock(&m_mutex);
    QList<VmInfo> out;
    out.reserve(m_vms.size());
    for (auto &v : m_vms) {
        v.stats = rollStats(v);
        out.push_back(v);
    }
    std::sort(out.begin(), out.end(),
        [](const VmInfo &a, const VmInfo &b){ return a.name < b.name; });
    return out;
}

VmInfo MockBackend::vm(const QString &uuid) {
    QMutexLocker lock(&m_mutex);
    VmInfo v = require(uuid);
    v.stats = rollStats(v);
    return v;
}

StatSample MockBackend::rollStats(const VmInfo &v) {
    StatSample s;
    s.timestamp = QDateTime::currentDateTime();
    s.memMaxKiB = v.memoryMaxKiB;
    if (v.state != VmState::Running) {
        return s;
    }
    std::uniform_real_distribution<double> cpu(2.0, 65.0);
    std::uniform_int_distribution<quint64> rate(0, 40ull * 1024 * 1024);
    s.cpuPercent = cpu(m_rng);
    s.memUsedKiB = quint64(double(v.memoryMaxKiB) * (0.35 + 0.4 * (cpu(m_rng) / 100.0)));
    s.diskReadBps = rate(m_rng);
    s.diskWriteBps = rate(m_rng);
    s.netRxBps = rate(m_rng);
    s.netTxBps = rate(m_rng);
    return s;
}

StatSample MockBackend::sampleStats(const QString &uuid) {
    QMutexLocker lock(&m_mutex);
    return rollStats(require(uuid));
}

void MockBackend::start(const QString &uuid)   { QMutexLocker l(&m_mutex); require(uuid).state = VmState::Running; }
void MockBackend::shutdown(const QString &uuid){ QMutexLocker l(&m_mutex); require(uuid).state = VmState::ShutOff; }
void MockBackend::forceOff(const QString &uuid){ QMutexLocker l(&m_mutex); require(uuid).state = VmState::ShutOff; }
void MockBackend::reboot(const QString &uuid)  { QMutexLocker l(&m_mutex); require(uuid).state = VmState::Running; }
void MockBackend::pause(const QString &uuid)   { QMutexLocker l(&m_mutex); require(uuid).state = VmState::Paused; }
void MockBackend::resume(const QString &uuid)  { QMutexLocker l(&m_mutex); require(uuid).state = VmState::Running; }
void MockBackend::setAutostart(const QString &uuid, bool on) { QMutexLocker l(&m_mutex); require(uuid).autostart = on; }

VmInfo MockBackend::define(const VmCreateRequest &req) {
    QMutexLocker lock(&m_mutex);
    VmInfo v = makeVm(m_connectionId, req.name, req.osVariant.isEmpty() ? "Other" : req.osVariant,
                      VmState::ShutOff, req.vcpus, req.memoryMiB, QStringLiteral("Created by wizard"));
    m_vms.insert(v.uuid, v);
    return v;
}

void MockBackend::undefine(const QString &uuid, bool) {
    QMutexLocker lock(&m_mutex);
    m_vms.remove(uuid);
    m_snapshots.remove(uuid);
}

void MockBackend::markTemplate(const QString &uuid, bool on) {
    QMutexLocker lock(&m_mutex);
    require(uuid).isTemplate = on;
}

VmInfo MockBackend::clone(const QString &uuid, const QString &newName, bool linked) {
    QMutexLocker lock(&m_mutex);
    VmInfo src = require(uuid);
    VmInfo v = src;
    v.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    v.name = newName;
    v.state = VmState::ShutOff;
    v.isTemplate = false;
    v.title = linked ? QStringLiteral("Linked clone of %1").arg(src.name)
                     : QStringLiteral("Clone of %1").arg(src.name);
    m_vms.insert(v.uuid, v);
    return v;
}

QList<SnapshotInfo> MockBackend::listSnapshots(const QString &uuid) {
    QMutexLocker lock(&m_mutex);
    return m_snapshots.value(uuid);
}

SnapshotInfo MockBackend::createSnapshot(const QString &uuid, const QString &name,
                                         const QString &description, bool includeMemory) {
    QMutexLocker lock(&m_mutex);
    require(uuid);
    auto &list = m_snapshots[uuid];
    for (auto &s : list) s.isCurrent = false;
    SnapshotInfo s;
    s.name = name;
    s.description = description;
    s.created = QDateTime::currentDateTime();
    s.hasMemory = includeMemory;
    s.isCurrent = true;
    s.parent = list.isEmpty() ? QString() : list.last().name;
    list.push_back(s);
    return s;
}

void MockBackend::restoreSnapshot(const QString &uuid, const QString &name) {
    QMutexLocker lock(&m_mutex);
    auto &list = m_snapshots[uuid];
    for (auto &s : list) s.isCurrent = (s.name == name);
}

void MockBackend::deleteSnapshot(const QString &uuid, const QString &name) {
    QMutexLocker lock(&m_mutex);
    auto &list = m_snapshots[uuid];
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const SnapshotInfo &s){ return s.name == name; }), list.end());
}

QList<StoragePoolInfo> MockBackend::listStoragePools() { QMutexLocker l(&m_mutex); return m_pools; }

QList<VolumeInfo> MockBackend::listVolumes(const QString &poolName) {
    QMutexLocker lock(&m_mutex);
    return m_volumes.value(poolName);
}

QList<NetworkInfo> MockBackend::listNetworks() { QMutexLocker l(&m_mutex); return m_networks; }

StoragePoolInfo MockBackend::createStoragePool(const QString &name, const QString &type,
                                               const QString &path) {
    QMutexLocker lock(&m_mutex);
    for (const auto &p : m_pools)
        if (p.name == name)
            throw BackendError(QStringLiteral("A pool named '%1' already exists").arg(name));
    StoragePoolInfo p;
    p.name = name;
    p.type = type.isEmpty() ? QStringLiteral("dir") : type;
    p.capacityBytes = 512ull*1024*1024*1024;
    p.allocationBytes = 0;
    p.active = true;
    p.targetPath = path;
    m_pools.push_back(p);
    m_volumes.insert(name, {});
    return p;
}

void MockBackend::deleteStoragePool(const QString &name, bool) {
    QMutexLocker lock(&m_mutex);
    m_pools.erase(std::remove_if(m_pools.begin(), m_pools.end(),
        [&](const StoragePoolInfo &p){ return p.name == name; }), m_pools.end());
    m_volumes.remove(name);
}

void MockBackend::setStoragePoolActive(const QString &name, bool active) {
    QMutexLocker lock(&m_mutex);
    for (auto &p : m_pools)
        if (p.name == name) p.active = active;
}

VolumeInfo MockBackend::createVolume(const QString &poolName, const QString &name,
                                     const QString &format, quint64 capacityBytes) {
    QMutexLocker lock(&m_mutex);
    bool found = false;
    QString dir = QStringLiteral("/var/lib/libvirt/images");
    for (auto &p : m_pools)
        if (p.name == poolName) { found = true; p.allocationBytes += capacityBytes / 10; }
    if (!found)
        throw BackendError(QStringLiteral("No such pool: %1").arg(poolName));
    const QString fmt = format.isEmpty() ? QStringLiteral("qcow2") : format;
    const QString fname = name.contains('.') ? name : (name + "." + fmt);
    VolumeInfo v{fname, dir + "/" + fname, fmt, capacityBytes, capacityBytes / 20};
    m_volumes[poolName].push_back(v);
    return v;
}

void MockBackend::deleteVolume(const QString &poolName, const QString &volumeName) {
    QMutexLocker lock(&m_mutex);
    auto &vols = m_volumes[poolName];
    vols.erase(std::remove_if(vols.begin(), vols.end(),
        [&](const VolumeInfo &v){ return v.name == volumeName; }), vols.end());
}

ConsoleInfo MockBackend::consoleInfo(const QString &uuid) {
    QMutexLocker lock(&m_mutex);
    const VmInfo v = require(uuid);
    ConsoleInfo c;
    c.running = v.state == VmState::Running;
    c.graphicsType = "vnc";
    c.host = "127.0.0.1";
    // Stable pseudo-port derived from the uuid so the demo looks believable.
    c.port = c.running ? 5900 + int(qHash(uuid) % 64) : -1;
    c.hasSerial = true;
    return c;
}

VmInfo MockBackend::importPreparedDisk(const QString &, const VmCreateRequest &req) {
    return define(req);
}

} // namespace vmm
