#pragma once

#include <QString>
#include <QList>
#include <QMetaType>
#include <QDateTime>

namespace vmm {

// State of a virtual machine, mirroring libvirt's virDomainState but decoupled
// from it so the UI and mock backend never include libvirt headers.
enum class VmState {
    NoState,
    Running,
    Paused,
    ShuttingDown,
    ShutOff,
    Crashed,
    Suspended,   // managed-save / pmsuspended
};

// A point-in-time resource sample for the live sparklines.
struct StatSample {
    double cpuPercent = 0.0;      // 0..100 across all vCPUs
    quint64 memUsedKiB = 0;
    quint64 memMaxKiB = 0;
    quint64 diskReadBps = 0;
    quint64 diskWriteBps = 0;
    quint64 netRxBps = 0;
    quint64 netTxBps = 0;
    QDateTime timestamp;
};

// Everything the dashboard/detail views need about one VM.
struct VmInfo {
    QString uuid;
    QString connectionId;         // which host this VM lives on
    QString name;
    QString osLabel;              // e.g. "Ubuntu 24.04", best-effort
    VmState state = VmState::NoState;
    int vcpus = 0;
    quint64 memoryMaxKiB = 0;
    quint64 memoryCurrentKiB = 0;
    bool autostart = false;
    bool isTemplate = false;
    QString title;                // human title / description
    StatSample stats;
};

// A libvirt connection (local or remote host).
struct HostInfo {
    QString id;                   // stable id (usually the URI)
    QString uri;                  // e.g. qemu:///system, qemu+ssh://user@host/system
    QString displayName;
    bool connected = false;
    bool isLocal = false;
    QString hypervisor;           // "QEMU", "KVM", ...
    QString hostArch;
    int activeVms = 0;
    int totalVms = 0;
    quint64 hostMemoryKiB = 0;
    int hostCpus = 0;
    QString lastError;
};

struct SnapshotInfo {
    QString name;
    QString parent;
    QString description;
    QDateTime created;
    bool hasMemory = false;       // full (with RAM) vs disk-only
    bool isCurrent = false;
};

struct StoragePoolInfo {
    QString name;
    QString type;                 // dir, logical, nfs, ...
    quint64 capacityBytes = 0;
    quint64 allocationBytes = 0;
    bool active = false;
};

struct VolumeInfo {
    QString name;
    QString path;
    QString format;               // qcow2, raw, ...
    quint64 capacityBytes = 0;
    quint64 allocationBytes = 0;
};

struct NetworkInfo {
    QString name;
    QString mode;                 // nat, route, bridge, isolated
    QString bridge;
    bool active = false;
    QString forwardDev;
};

// A request to define a new VM (from the create wizard).
struct VmCreateRequest {
    QString name;
    QString osVariant;            // libosinfo id, best-effort
    int vcpus = 2;
    quint64 memoryMiB = 2048;
    quint64 diskGiB = 20;
    QString installMediaPath;     // ISO path, empty for none
    QString networkName = "default";
    QString firmware = "bios";    // "bios" | "uefi"
    QString diskFormat = "qcow2";
};

// Supported source formats for cross-platform image import.
struct ImportableFormat {
    QString extension;            // "vmdk"
    QString label;                // "VMware disk"
    bool needsConversion = true;  // false for qcow2/raw (register in place)
};

} // namespace vmm

Q_DECLARE_METATYPE(vmm::VmInfo)
Q_DECLARE_METATYPE(vmm::HostInfo)
Q_DECLARE_METATYPE(vmm::SnapshotInfo)
Q_DECLARE_METATYPE(vmm::StoragePoolInfo)
Q_DECLARE_METATYPE(vmm::VolumeInfo)
Q_DECLARE_METATYPE(vmm::NetworkInfo)
