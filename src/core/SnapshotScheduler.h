#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include <QDateTime>
#include <QVariantList>

namespace vmm {

// Fires periodic snapshot requests for VMs the user has put on a schedule.
// Persistence of schedules is via QSettings for now; a later phase stores them
// as libvirt domain metadata so they survive across machines (see ROADMAP).
class SnapshotScheduler : public QObject {
    Q_OBJECT
public:
    explicit SnapshotScheduler(QObject *parent = nullptr);

    struct Schedule {
        QString connId;
        QString uuid;
        QString vmName;
        int intervalMinutes = 60;
        int retain = 7;             // keep newest N scheduled snapshots
        QDateTime nextRun;
        bool enabled = true;
    };

    // QML API
    Q_INVOKABLE void addSchedule(const QString &connId, const QString &uuid,
                                 const QString &vmName, int intervalMinutes, int retain);
    Q_INVOKABLE void removeSchedule(const QString &uuid);
    Q_INVOKABLE void setEnabled(const QString &uuid, bool enabled);
    Q_INVOKABLE QVariantList schedules() const;

signals:
    // Emitted when a schedule is due. AppController performs the snapshot and
    // applies the retention policy.
    void snapshotDue(const QString &connId, const QString &uuid, const QString &snapshotName);
    void schedulesChanged();

private:
    void tick();
    void load();
    void save() const;

    QList<Schedule> m_schedules;
    QTimer m_timer;
};

} // namespace vmm
