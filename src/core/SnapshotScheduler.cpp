#include "SnapshotScheduler.h"

#include <QSettings>

namespace vmm {

SnapshotScheduler::SnapshotScheduler(QObject *parent) : QObject(parent) {
    load();
    m_timer.setInterval(30 * 1000);   // check twice a minute
    connect(&m_timer, &QTimer::timeout, this, &SnapshotScheduler::tick);
    m_timer.start();
}

void SnapshotScheduler::addSchedule(const QString &connId, const QString &uuid,
                                    const QString &vmName, int intervalMinutes, int retain) {
    removeSchedule(uuid);   // one schedule per VM for now
    Schedule s;
    s.connId = connId;
    s.uuid = uuid;
    s.vmName = vmName;
    s.intervalMinutes = qMax(1, intervalMinutes);
    s.retain = qMax(1, retain);
    s.nextRun = QDateTime::currentDateTime().addSecs(s.intervalMinutes * 60);
    m_schedules.push_back(s);
    save();
    emit schedulesChanged();
}

void SnapshotScheduler::removeSchedule(const QString &uuid) {
    const int before = m_schedules.size();
    m_schedules.erase(std::remove_if(m_schedules.begin(), m_schedules.end(),
        [&](const Schedule &s){ return s.uuid == uuid; }), m_schedules.end());
    if (m_schedules.size() != before) {
        save();
        emit schedulesChanged();
    }
}

void SnapshotScheduler::setEnabled(const QString &uuid, bool enabled) {
    for (auto &s : m_schedules) {
        if (s.uuid == uuid) {
            s.enabled = enabled;
            if (enabled)
                s.nextRun = QDateTime::currentDateTime().addSecs(s.intervalMinutes * 60);
            save();
            emit schedulesChanged();
            return;
        }
    }
}

QVariantList SnapshotScheduler::schedules() const {
    QVariantList out;
    for (const auto &s : m_schedules)
        out.push_back(QVariantMap{
            {"connId", s.connId}, {"uuid", s.uuid}, {"vmName", s.vmName},
            {"intervalMinutes", s.intervalMinutes}, {"retain", s.retain},
            {"nextRun", s.nextRun}, {"enabled", s.enabled}});
    return out;
}

void SnapshotScheduler::tick() {
    const QDateTime now = QDateTime::currentDateTime();
    bool dirty = false;
    for (auto &s : m_schedules) {
        if (!s.enabled || s.nextRun > now)
            continue;
        const QString name = QStringLiteral("auto-%1").arg(now.toString("yyyyMMdd-HHmmss"));
        emit snapshotDue(s.connId, s.uuid, name);
        s.nextRun = now.addSecs(s.intervalMinutes * 60);
        dirty = true;
    }
    if (dirty)
        save();
}

void SnapshotScheduler::load() {
    QSettings settings;
    const int n = settings.beginReadArray("snapshotSchedules");
    for (int i = 0; i < n; ++i) {
        settings.setArrayIndex(i);
        Schedule s;
        s.connId = settings.value("connId").toString();
        s.uuid = settings.value("uuid").toString();
        s.vmName = settings.value("vmName").toString();
        s.intervalMinutes = settings.value("intervalMinutes", 60).toInt();
        s.retain = settings.value("retain", 7).toInt();
        s.nextRun = settings.value("nextRun").toDateTime();
        s.enabled = settings.value("enabled", true).toBool();
        if (!s.nextRun.isValid())
            s.nextRun = QDateTime::currentDateTime().addSecs(s.intervalMinutes * 60);
        m_schedules.push_back(s);
    }
    settings.endArray();
}

void SnapshotScheduler::save() const {
    QSettings settings;
    settings.beginWriteArray("snapshotSchedules");
    for (int i = 0; i < m_schedules.size(); ++i) {
        settings.setArrayIndex(i);
        const Schedule &s = m_schedules.at(i);
        settings.setValue("connId", s.connId);
        settings.setValue("uuid", s.uuid);
        settings.setValue("vmName", s.vmName);
        settings.setValue("intervalMinutes", s.intervalMinutes);
        settings.setValue("retain", s.retain);
        settings.setValue("nextRun", s.nextRun);
        settings.setValue("enabled", s.enabled);
    }
    settings.endArray();
}

} // namespace vmm
