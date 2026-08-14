#pragma once

#include "Types.h"

#include <QAbstractListModel>
#include <QList>
#include <QHash>
#include <QVariantMap>

namespace vmm {

// QAbstractListModel of VMs across all connections, consumed directly by QML
// ListView/Repeater. Stats roles are updated in place on a timer so sparklines
// animate rather than the list re-populating.
class VmListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY countChanged)
public:
    enum Roles {
        UuidRole = Qt::UserRole + 1,
        ConnectionIdRole,
        NameRole,
        TitleRole,
        OsLabelRole,
        StateRole,          // int (VmState)
        StateTextRole,      // "Running", ...
        VcpusRole,
        MemoryMaxRole,      // KiB
        MemoryCurrentRole,  // KiB
        AutostartRole,
        IsTemplateRole,
        CpuPercentRole,
        MemPercentRole,
        DiskReadRole,
        DiskWriteRole,
        NetRxRole,
        NetTxRole,
    };
    Q_ENUM(Roles)

    explicit VmListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Wholesale replace the VMs belonging to one connection (from listVms()).
    void mergeConnection(const QString &connId, const QList<VmInfo> &vms);
    // Update just the stats of matching rows (from a stats poll).
    void applyStats(const QString &connId, const QHash<QString, StatSample> &byUuid);
    void removeConnection(const QString &connId);

    int runningCount() const;
    Q_INVOKABLE int indexOfUuid(const QString &uuid) const;
    // Live snapshot of one VM's fields (for the detail view to stay current).
    Q_INVOKABLE QVariantMap vmMap(const QString &uuid) const;

signals:
    void countChanged();

private:
    static QString stateText(VmState s);
    QList<VmInfo> m_vms;
};

} // namespace vmm
