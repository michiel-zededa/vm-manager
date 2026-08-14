import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Full-page detail for a single VM: identity, lifecycle toolbar, live stats,
// and tabs for Console and Snapshots.
Item {
    id: root
    property var vm: null
    signal back()

    property bool isRunning: vm && vm.state === 1
    property bool isPaused: vm && vm.state === 2
    function gib(kib) { return (kib / 1048576).toFixed(kib >= 1048576 ? 0 : 1) + " GiB"; }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            IconButton { glyph: "‹"; tooltip: qsTr("Back"); onClicked: root.back() }
            Rectangle {
                width: 44; height: 44; radius: Theme.radiusSm; color: Theme.surfaceAlt
                Text { anchors.centerIn: parent; text: "🖥"; font.pixelSize: 22 }
            }
            ColumnLayout {
                spacing: 1
                Text { text: root.vm ? root.vm.name : ""; color: Theme.text; font.pixelSize: Theme.fontXxl; font.weight: Font.DemiBold }
                RowLayout {
                    spacing: Theme.space3
                    StatusBadge { state: root.vm ? Number(root.vm.state) : 0; label: root.vm ? (root.isRunning ? qsTr("Running") : root.isPaused ? qsTr("Paused") : qsTr("Stopped")) : "" }
                    Text { text: root.vm ? root.vm.osLabel : ""; color: Theme.textDim; font.pixelSize: Theme.fontSm }
                }
            }
            Item { Layout.fillWidth: true }

            // Lifecycle toolbar
            AppButton { visible: !root.isRunning; text: qsTr("▶ Start"); variant: "primary"; onClicked: App.startVm(root.vm.connectionId, root.vm.uuid) }
            AppButton { visible: root.isPaused; text: qsTr("⏵ Resume"); variant: "primary"; onClicked: App.resumeVm(root.vm.connectionId, root.vm.uuid) }
            AppButton { visible: root.isRunning; text: qsTr("⏸ Pause"); variant: "ghost"; onClicked: App.pauseVm(root.vm.connectionId, root.vm.uuid) }
            AppButton { visible: root.isRunning; text: qsTr("↻ Reboot"); variant: "ghost"; onClicked: App.rebootVm(root.vm.connectionId, root.vm.uuid) }
            AppButton { visible: root.isRunning || root.isPaused; text: qsTr("⏻ Shut down"); variant: "ghost"; onClicked: App.shutdownVm(root.vm.connectionId, root.vm.uuid) }
            IconButton { glyph: "⋯"; tooltip: qsTr("More"); onClicked: moreMenu.open()
                Menu {
                    id: moreMenu
                    background: Rectangle { implicitWidth: 200; radius: Theme.radiusSm
                        color: Theme.surface; border.width: 1; border.color: Theme.border }
                    delegate: MenuItem {
                        id: mi
                        contentItem: Text { text: mi.text; color: Theme.text; font.pixelSize: Theme.fontMd
                            verticalAlignment: Text.AlignVCenter; leftPadding: Theme.space2 }
                        background: Rectangle { color: mi.highlighted ? Theme.accentSubtle : "transparent"; radius: Theme.radiusSm }
                    }
                    MenuItem { text: qsTr("Clone…"); onTriggered: cloneDialog.open() }
                    MenuItem { text: root.vm && root.vm.isTemplate ? qsTr("Unmark template") : qsTr("Mark as template")
                               onTriggered: App.markTemplate(root.vm.connectionId, root.vm.uuid, !(root.vm && root.vm.isTemplate)) }
                    MenuItem { text: root.vm && root.vm.autostart ? qsTr("Disable autostart") : qsTr("Enable autostart")
                               onTriggered: App.setAutostart(root.vm.connectionId, root.vm.uuid, !(root.vm && root.vm.autostart)) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Force off"); onTriggered: App.forceOffVm(root.vm.connectionId, root.vm.uuid) }
                    MenuItem { text: qsTr("Delete…"); onTriggered: deleteDialog.open() }
                }
            }
        }

        // ---- Tabs ----
        TabBar {
            id: tabs
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }
            AppTabButton { text: qsTr("Overview"); width: implicitWidth + Theme.space5 }
            AppTabButton { text: qsTr("Console"); width: implicitWidth + Theme.space5 }
            AppTabButton { text: qsTr("Snapshots"); width: implicitWidth + Theme.space5 }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // --- Overview ---
            ColumnLayout {
                spacing: Theme.space4
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    Repeater {
                        model: [
                            { l: qsTr("vCPUs"), v: root.vm ? "" + root.vm.vcpus : "–" },
                            { l: qsTr("Memory"), v: root.vm ? root.gib(root.vm.memoryMax) : "–" },
                            { l: qsTr("Autostart"), v: root.vm && root.vm.autostart ? qsTr("On") : qsTr("Off") },
                            { l: qsTr("Host"), v: root.vm ? root.vm.connectionId : "–" },
                        ]
                        delegate: Card {
                            Layout.fillWidth: true; implicitHeight: 78
                            RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                                StatTile { label: modelData.l; value: modelData.v }
                            }
                        }
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space2
                        Text { text: qsTr("Description"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                        Text {
                            text: root.vm && root.vm.title ? root.vm.title : qsTr("No description")
                            color: Theme.text; font.pixelSize: Theme.fontMd
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // --- Console ---
            ConsoleView { vm: root.vm }

            // --- Snapshots (inline panel) ---
            ColumnLayout {
                spacing: Theme.space4

                ListModel { id: snapModel }
                Connections {
                    target: App
                    function onSnapshotsLoaded(uuid, snapshots) {
                        if (!root.vm || uuid !== root.vm.uuid) return;
                        snapModel.clear();
                        for (let i = 0; i < snapshots.length; ++i)
                            snapModel.append(snapshots[i]);
                    }
                }
                Component.onCompleted: if (root.vm) App.loadSnapshots(root.vm.connectionId, root.vm.uuid)

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Snapshots"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    AppButton { text: qsTr("◷ Schedule…"); variant: "ghost"; onClicked: scheduleDialog.open() }
                    AppButton { text: qsTr("＋ Take snapshot"); variant: "primary"; onClicked: takeDialog.open() }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ListView {
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        clip: true
                        model: snapModel
                        spacing: Theme.space1
                        delegate: RowLayout {
                            width: ListView.view.width
                            height: 52
                            spacing: Theme.space3
                            Rectangle { width: 8; height: 8; radius: 4; color: isCurrent ? Theme.running : Theme.stopped; Layout.leftMargin: Theme.space2 }
                            ColumnLayout {
                                spacing: 0; Layout.fillWidth: true
                                Text { text: name; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                Text { text: (hasMemory ? qsTr("Full (with memory)") : qsTr("Disk only")) + (description ? " · " + description : ""); color: Theme.textDim; font.pixelSize: Theme.fontXs }
                            }
                            AppButton { text: qsTr("Restore"); variant: "subtle"; onClicked: App.restoreSnapshot(root.vm.connectionId, root.vm.uuid, name) }
                            IconButton { glyph: "🗑"; tooltip: qsTr("Delete"); onClicked: App.deleteSnapshot(root.vm.connectionId, root.vm.uuid, name) }
                        }
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        visible: snapModel.count === 0
                        glyph: "◷"
                        title: qsTr("No snapshots")
                        body: qsTr("Capture the current state so you can roll back after risky changes.")
                    }
                }
            }
        }
    }

    // Take-snapshot dialog
    Dialog {
        id: takeDialog
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        onOpened: snapName.text = "snap-" + Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss")
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Take snapshot"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            AppTextField { id: snapName; Layout.fillWidth: true; placeholderText: qsTr("Snapshot name") }
            AppTextField { id: snapDesc; Layout.fillWidth: true; placeholderText: qsTr("Description (optional)") }
            AppCheckBox { id: snapMem; text: qsTr("Include memory state (VM must be running)"); enabled: root.isRunning }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: takeDialog.close() }
                AppButton { text: qsTr("Take"); variant: "primary"
                    onClicked: { App.takeSnapshot(root.vm.connectionId, root.vm.uuid, snapName.text, snapDesc.text, snapMem.checked); takeDialog.close(); } }
            }
        }
    }

    // Schedule dialog
    Dialog {
        id: scheduleDialog
        anchors.centerIn: parent; modal: true; width: 460; padding: Theme.space5
        background: Card {}
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Scheduled snapshots"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text { text: qsTr("Automatically snapshot this VM on an interval and keep the newest N."); color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            LabeledSlider { id: everyMin; label: qsTr("Every"); from: 5; to: 1440; stepSize: 5; value: 60; unit: "min" }
            LabeledSlider { id: retain; label: qsTr("Keep newest"); from: 1; to: 60; stepSize: 1; value: 7; unit: "snaps" }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: qsTr("Remove schedule"); variant: "ghost"
                    onClicked: { if (root.vm) App.scheduler.removeSchedule(root.vm.uuid); scheduleDialog.close(); } }
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: scheduleDialog.close() }
                AppButton { text: qsTr("Save"); variant: "primary"
                    onClicked: { if (root.vm) App.scheduler.addSchedule(root.vm.connectionId, root.vm.uuid, root.vm.name, Math.round(everyMin.value), Math.round(retain.value)); scheduleDialog.close(); } }
            }
        }
    }

    // Clone dialog
    Dialog {
        id: cloneDialog
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        onOpened: cloneName.text = root.vm ? root.vm.name + "-clone" : ""
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Clone VM"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            AppTextField { id: cloneName; Layout.fillWidth: true; placeholderText: qsTr("New VM name") }
            AppCheckBox { id: linkedClone; text: qsTr("Linked clone (share backing image, faster)") }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: cloneDialog.close() }
                AppButton { text: qsTr("Clone"); variant: "primary"
                    onClicked: { App.cloneVm(root.vm.connectionId, root.vm.uuid, cloneName.text, linkedClone.checked); cloneDialog.close(); } }
            }
        }
    }

    // Delete confirmation
    Dialog {
        id: deleteDialog
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Delete %1?").arg(root.vm ? root.vm.name : ""); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text { text: qsTr("This removes the VM definition. Choose whether to also delete its disks."); color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            AppCheckBox { id: removeStorage; text: qsTr("Also delete disk images (irreversible)") }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: deleteDialog.close() }
                AppButton { text: qsTr("Delete"); variant: "danger"
                    onClicked: { App.deleteVm(root.vm.connectionId, root.vm.uuid, removeStorage.checked); deleteDialog.close(); root.back(); } }
            }
        }
    }
}
