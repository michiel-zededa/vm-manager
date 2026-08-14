import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Every snapshot across every VM, plus the automatic schedules. Per-VM capture
// still lives on each VM's Snapshots tab; this is the fleet-wide overview.
Item {
    id: root

    ListModel { id: snapModel }     // flat list of all snapshots
    ListModel { id: schedModel }    // schedules

    function reloadSchedules() {
        schedModel.clear();
        const list = App.scheduler.schedules();
        for (let i = 0; i < list.length; ++i) schedModel.append(list[i]);
    }
    function reloadSnapshots() {
        snapModel.clear();
        for (let i = 0; i < App.vms.count; ++i) {
            const idx = App.vms.index(i, 0);
            const uuid = App.vms.data(idx, Qt.UserRole + 1);
            const conn = App.vms.data(idx, Qt.UserRole + 2);
            App.loadSnapshots(conn, uuid);
        }
    }
    function reload() { reloadSchedules(); reloadSnapshots(); }

    Connections {
        target: App
        function onSnapshotsLoaded(uuid, snapshots) {
            // Replace this VM's rows.
            for (let i = snapModel.count - 1; i >= 0; --i)
                if (snapModel.get(i).uuid === uuid) snapModel.remove(i);
            const m = App.vms.vmMap(uuid);
            const vmName = (m && m.name) ? m.name : uuid;
            const conn = (m && m.connectionId) ? m.connectionId : "";
            for (let j = 0; j < snapshots.length; ++j)
                snapModel.append({
                    vmName: vmName, connId: conn, uuid: uuid,
                    snapName: snapshots[j].name, hasMemory: snapshots[j].hasMemory,
                    created: snapshots[j].created, isCurrent: snapshots[j].isCurrent
                });
        }
    }
    Connections { target: App.scheduler; function onSchedulesChanged() { root.reloadSchedules() } }
    Connections { target: App.vms; function onCountChanged() { root.reloadSnapshots() } }
    Component.onCompleted: reload()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4

        RowLayout {
            Layout.fillWidth: true
            SectionHeader { title: qsTr("Snapshots")
                subtitle: qsTr("Every snapshot and automatic schedule across your VMs") }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("↻  Refresh"); variant: "ghost"; onClicked: root.reload() }
        }

        // ---- Schedules (compact) ----
        ColumnLayout {
            Layout.fillWidth: true
            visible: schedModel.count > 0
            spacing: Theme.space2
            Text { text: qsTr("Schedules"); color: Theme.textDim; font.pixelSize: Theme.fontXs; font.letterSpacing: 1 }
            Repeater {
                model: schedModel
                delegate: Card {
                    required property string uuid
                    required property string vmName
                    required property int intervalMinutes
                    required property int retain
                    required property var nextRun
                    required property bool enabled
                    Layout.fillWidth: true; implicitHeight: 58
                    RowLayout {
                        anchors.fill: parent; anchors.margins: Theme.space3; spacing: Theme.space3
                        Text { text: "◷"; color: Theme.accent; font.pixelSize: Theme.fontLg }
                        ColumnLayout {
                            spacing: 0; Layout.fillWidth: true
                            Text { text: vmName; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                            Text { text: qsTr("Every %1 min · keep %2 · next %3").arg(intervalMinutes).arg(retain)
                                        .arg(Qt.formatDateTime(nextRun, "MMM d, HH:mm"))
                                   color: Theme.textDim; font.pixelSize: Theme.fontXs }
                        }
                        AppSwitch { checked: enabled; onToggled: App.scheduler.setEnabled(uuid, checked) }
                        IconButton { glyph: "🗑"; tip: qsTr("Remove schedule"); danger: true
                            onClicked: App.scheduler.removeSchedule(uuid) }
                    }
                }
            }
        }

        // ---- All snapshots ----
        Text { text: qsTr("All snapshots"); color: Theme.textDim; font.pixelSize: Theme.fontXs; font.letterSpacing: 1
            Layout.topMargin: schedModel.count > 0 ? Theme.space2 : 0 }
        Card {
            Layout.fillWidth: true; Layout.fillHeight: true
            ListView {
                anchors.fill: parent; anchors.margins: Theme.space2; clip: true
                model: snapModel; spacing: Theme.space1
                delegate: Item {
                    required property string vmName
                    required property string connId
                    required property string uuid
                    required property string snapName
                    required property bool hasMemory
                    required property bool isCurrent
                    required property var created
                    width: ListView.view.width
                    height: 56
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: Theme.space2; anchors.rightMargin: Theme.space2
                        spacing: Theme.space3
                        Rectangle { width: 8; height: 8; radius: 4; Layout.alignment: Qt.AlignVCenter
                            color: isCurrent ? Theme.running : Theme.stopped }
                        ColumnLayout {
                            spacing: 1; Layout.fillWidth: true
                            RowLayout {
                                spacing: Theme.space2
                                Text { text: snapName; color: Theme.text; font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium; elide: Text.ElideRight; Layout.maximumWidth: 300 }
                                Rectangle { height: 16; radius: 8; color: Theme.surfaceAlt
                                    implicitWidth: vmT.implicitWidth + 12
                                    Text { id: vmT; anchors.centerIn: parent; text: vmName
                                        color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                            }
                            Text { text: (hasMemory ? qsTr("Full (with memory)") : qsTr("Disk only"))
                                        + (created ? " · " + Qt.formatDateTime(created, "MMM d, HH:mm") : "")
                                   color: Theme.textDim; font.pixelSize: Theme.fontXs; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        AppButton { text: qsTr("Restore"); variant: "subtle"; Layout.alignment: Qt.AlignVCenter
                            onClicked: App.restoreSnapshot(connId, uuid, snapName) }
                        IconButton { glyph: "🗑"; tip: qsTr("Delete"); danger: true; Layout.alignment: Qt.AlignVCenter
                            onClicked: App.deleteSnapshot(connId, uuid, snapName) }
                    }
                }
            }
            EmptyState {
                anchors.centerIn: parent
                visible: snapModel.count === 0
                glyph: "◷"
                title: qsTr("No snapshots yet")
                body: qsTr("Open a VM, go to its Snapshots tab and take one — or set an automatic schedule.")
            }
        }
    }
}
