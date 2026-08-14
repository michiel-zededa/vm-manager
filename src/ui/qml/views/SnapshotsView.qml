import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Overview of scheduled snapshots across all VMs (the scheduler's view).
// Per-VM snapshot management lives on each VM's detail page.
Item {
    id: root

    ListModel { id: schedModel }
    function reload() {
        schedModel.clear();
        const list = App.scheduler.schedules();
        for (let i = 0; i < list.length; ++i) schedModel.append(list[i]);
    }
    Component.onCompleted: reload()
    Connections { target: App.scheduler; function onSchedulesChanged() { root.reload(); } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space5

        SectionHeader {
            title: qsTr("Scheduled snapshots")
            subtitle: qsTr("Automatic snapshot policies with retention, across every VM")
        }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.space3
                Repeater {
                    model: schedModel
                    delegate: Card {
                        Layout.fillWidth: true; implicitHeight: 76
                        RowLayout {
                            anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space4
                            Rectangle { width: 10; height: 10; radius: 5; color: enabled ? Theme.running : Theme.stopped }
                            ColumnLayout {
                                spacing: 1; Layout.fillWidth: true
                                Text { text: vmName; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Text { text: qsTr("Every %1 min · keep %2 · next %3")
                                            .arg(intervalMinutes).arg(retain)
                                            .arg(Qt.formatDateTime(nextRun, "MMM d, HH:mm"))
                                       color: Theme.textDim; font.pixelSize: Theme.fontXs }
                            }
                            Switch { checked: enabled; onToggled: App.scheduler.setEnabled(uuid, checked) }
                            IconButton { glyph: "🗑"; tooltip: qsTr("Remove"); onClicked: App.scheduler.removeSchedule(uuid) }
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            visible: schedModel.count === 0
            glyph: "◷"
            title: qsTr("No snapshot schedules")
            body: qsTr("Open a VM, go to the Snapshots tab and choose “Schedule…” to snapshot it automatically on an interval.")
        }
    }
}
