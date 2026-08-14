#include "ConnectionModel.h"

namespace vmm {

ConnectionModel::ConnectionModel(QObject *parent) : QAbstractListModel(parent) {}

int ConnectionModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_hosts.size());
}

QVariant ConnectionModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_hosts.size())
        return {};
    const HostInfo &h = m_hosts.at(index.row());
    switch (role) {
    case IdRole:          return h.id;
    case UriRole:         return h.uri;
    case DisplayNameRole: return h.displayName;
    case ConnectedRole:   return h.connected;
    case IsLocalRole:     return h.isLocal;
    case HypervisorRole:  return h.hypervisor;
    case HostArchRole:    return h.hostArch;
    case ActiveVmsRole:   return h.activeVms;
    case TotalVmsRole:    return h.totalVms;
    case HostCpusRole:    return h.hostCpus;
    case HostMemoryRole:  return QVariant::fromValue(h.hostMemoryKiB);
    case LastErrorRole:   return h.lastError;
    default:              return {};
    }
}

QHash<int, QByteArray> ConnectionModel::roleNames() const {
    return {
        {IdRole, "id"}, {UriRole, "uri"}, {DisplayNameRole, "displayName"},
        {ConnectedRole, "connected"}, {IsLocalRole, "isLocal"},
        {HypervisorRole, "hypervisor"}, {HostArchRole, "hostArch"},
        {ActiveVmsRole, "activeVms"}, {TotalVmsRole, "totalVms"},
        {HostCpusRole, "hostCpus"}, {HostMemoryRole, "hostMemory"},
        {LastErrorRole, "lastError"},
    };
}

int ConnectionModel::indexOfId(const QString &id) const {
    for (int i = 0; i < m_hosts.size(); ++i)
        if (m_hosts.at(i).id == id)
            return i;
    return -1;
}

void ConnectionModel::upsert(const HostInfo &host) {
    const int i = indexOfId(host.id);
    if (i >= 0) {
        m_hosts[i] = host;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx);
    } else {
        beginInsertRows({}, m_hosts.size(), m_hosts.size());
        m_hosts.append(host);
        endInsertRows();
        emit countChanged();
    }
}

void ConnectionModel::remove(const QString &id) {
    const int i = indexOfId(id);
    if (i < 0)
        return;
    beginRemoveRows({}, i, i);
    m_hosts.removeAt(i);
    endRemoveRows();
    emit countChanged();
}

} // namespace vmm
