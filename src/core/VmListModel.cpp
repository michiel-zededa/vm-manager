#include "VmListModel.h"

#include <algorithm>

namespace vmm {

VmListModel::VmListModel(QObject *parent) : QAbstractListModel(parent) {}

int VmListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_vms.size());
}

QString VmListModel::stateText(VmState s) {
    switch (s) {
    case VmState::Running:      return QStringLiteral("Running");
    case VmState::Paused:       return QStringLiteral("Paused");
    case VmState::ShuttingDown: return QStringLiteral("Shutting down");
    case VmState::ShutOff:      return QStringLiteral("Stopped");
    case VmState::Crashed:      return QStringLiteral("Crashed");
    case VmState::Suspended:    return QStringLiteral("Suspended");
    case VmState::NoState:      break;
    }
    return QStringLiteral("Unknown");
}

QVariant VmListModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_vms.size())
        return {};
    const VmInfo &v = m_vms.at(index.row());
    const StatSample &s = v.stats;
    switch (role) {
    case UuidRole:          return v.uuid;
    case ConnectionIdRole:  return v.connectionId;
    case NameRole:          return v.name;
    case TitleRole:         return v.title;
    case OsLabelRole:       return v.osLabel;
    case StateRole:         return int(v.state);
    case StateTextRole:     return stateText(v.state);
    case VcpusRole:         return v.vcpus;
    case MemoryMaxRole:     return QVariant::fromValue(v.memoryMaxKiB);
    case MemoryCurrentRole: return QVariant::fromValue(v.memoryCurrentKiB);
    case AutostartRole:     return v.autostart;
    case IsTemplateRole:    return v.isTemplate;
    case CpuPercentRole:    return s.cpuPercent;
    case MemPercentRole:    return s.memMaxKiB ? (100.0 * double(s.memUsedKiB) / double(s.memMaxKiB)) : 0.0;
    case DiskReadRole:      return QVariant::fromValue(s.diskReadBps);
    case DiskWriteRole:     return QVariant::fromValue(s.diskWriteBps);
    case NetRxRole:         return QVariant::fromValue(s.netRxBps);
    case NetTxRole:         return QVariant::fromValue(s.netTxBps);
    default:                return {};
    }
}

QHash<int, QByteArray> VmListModel::roleNames() const {
    return {
        {UuidRole, "uuid"}, {ConnectionIdRole, "connectionId"}, {NameRole, "name"},
        {TitleRole, "title"}, {OsLabelRole, "osLabel"}, {StateRole, "state"},
        {StateTextRole, "stateText"}, {VcpusRole, "vcpus"}, {MemoryMaxRole, "memoryMax"},
        {MemoryCurrentRole, "memoryCurrent"}, {AutostartRole, "autostart"},
        {IsTemplateRole, "isTemplate"}, {CpuPercentRole, "cpuPercent"},
        {MemPercentRole, "memPercent"}, {DiskReadRole, "diskRead"},
        {DiskWriteRole, "diskWrite"}, {NetRxRole, "netRx"}, {NetTxRole, "netTx"},
    };
}

int VmListModel::runningCount() const {
    int n = 0;
    for (const VmInfo &v : m_vms)
        if (v.state == VmState::Running)
            ++n;
    return n;
}

int VmListModel::indexOfUuid(const QString &uuid) const {
    for (int i = 0; i < m_vms.size(); ++i)
        if (m_vms.at(i).uuid == uuid)
            return i;
    return -1;
}

void VmListModel::mergeConnection(const QString &connId, const QList<VmInfo> &vms) {
    // Simple + correct: drop this connection's rows and re-insert. The set of
    // VMs on a host changes rarely; per-row stat updates use applyStats().
    beginResetModel();
    m_vms.erase(std::remove_if(m_vms.begin(), m_vms.end(),
        [&](const VmInfo &v){ return v.connectionId == connId; }), m_vms.end());
    m_vms.append(vms);
    std::sort(m_vms.begin(), m_vms.end(), [](const VmInfo &a, const VmInfo &b){
        if (a.connectionId != b.connectionId) return a.connectionId < b.connectionId;
        return a.name < b.name;
    });
    endResetModel();
    emit countChanged();
}

void VmListModel::applyStats(const QString &connId, const QHash<QString, StatSample> &byUuid) {
    static const QVector<int> statRoles = {
        CpuPercentRole, MemPercentRole, DiskReadRole, DiskWriteRole, NetRxRole, NetTxRole,
        MemoryCurrentRole };
    for (int i = 0; i < m_vms.size(); ++i) {
        VmInfo &v = m_vms[i];
        if (v.connectionId != connId)
            continue;
        auto it = byUuid.find(v.uuid);
        if (it == byUuid.end())
            continue;
        v.stats = it.value();
        v.memoryCurrentKiB = it.value().memUsedKiB;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, statRoles);
    }
}

void VmListModel::removeConnection(const QString &connId) {
    beginResetModel();
    m_vms.erase(std::remove_if(m_vms.begin(), m_vms.end(),
        [&](const VmInfo &v){ return v.connectionId == connId; }), m_vms.end());
    endResetModel();
    emit countChanged();
}

} // namespace vmm
