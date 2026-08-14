#pragma once

#include "Types.h"

#include <QAbstractListModel>
#include <QList>

namespace vmm {

// QAbstractListModel of hypervisor connections (hosts) for the sidebar.
class ConnectionModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        UriRole,
        DisplayNameRole,
        ConnectedRole,
        IsLocalRole,
        HypervisorRole,
        HostArchRole,
        ActiveVmsRole,
        TotalVmsRole,
        HostCpusRole,
        HostMemoryRole,     // KiB
        LastErrorRole,
    };
    Q_ENUM(Roles)

    explicit ConnectionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsert(const HostInfo &host);
    void remove(const QString &id);
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE QString displayNameFor(const QString &id) const;

signals:
    void countChanged();

private:
    QList<HostInfo> m_hosts;
};

} // namespace vmm
