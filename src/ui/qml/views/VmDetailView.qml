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

    // Live view of the VM: the passed-in map is a point-in-time capture, so we
    // re-read it from the model to keep state/console/actions current.
    property var live: vm
    function refresh() {
        if (vm && vm.uuid) { var m = App.vms.vmMap(vm.uuid); if (m && m.uuid) live = m; }
    }
    Component.onCompleted: refresh()
    Connections { target: App; function onVmActionCompleted(uuid, action) { if (root.vm && uuid === root.vm.uuid) root.refresh(); } }
    Connections { target: App.vms; function onCountChanged() { root.refresh(); } }
    Timer { interval: 2000; running: true; repeat: true; onTriggered: root.refresh() }

    property bool isRunning: live && live.state === 1
    property bool isPaused: live && live.state === 2
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
                    StatusBadge { state: root.live ? Number(root.live.state) : 0; label: root.live ? (root.isRunning ? qsTr("Running") : root.isPaused ? qsTr("Paused") : qsTr("Stopped")) : "" }
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
            AppTabButton { text: qsTr("Hardware"); width: implicitWidth + Theme.space5 }
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
            ConsoleView { vm: root.live }

            // --- Hardware (disks) ---
            ColumnLayout {
                spacing: Theme.space4

                ListModel { id: diskModel }
                Connections {
                    target: App
                    function onDisksLoaded(uuid, disks) {
                        if (!root.vm || uuid !== root.vm.uuid) return;
                        diskModel.clear();
                        for (let i = 0; i < disks.length; ++i) diskModel.append(disks[i]);
                    }
                }
                Component.onCompleted: if (root.vm) App.loadDisks(root.vm.connectionId, root.vm.uuid)

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Disks"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    AppButton { text: qsTr("＋ Attach disk"); variant: "primary"; onClicked: attachDialog.open() }
                }
                Card {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ListView {
                        anchors.fill: parent; anchors.margins: Theme.space2; clip: true
                        model: diskModel; spacing: Theme.space1
                        delegate: Item {
                            required property string target
                            required property string path
                            required property string bus
                            required property string format
                            required property string device
                            required property var capacityBytes
                            width: ListView.view.width
                            height: 52
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.space2; anchors.rightMargin: Theme.space2
                                spacing: Theme.space3
                                Text { text: device === "cdrom" ? "💿" : "💽"; font.pixelSize: Theme.fontMd }
                                ColumnLayout {
                                    spacing: 0; Layout.fillWidth: true
                                    RowLayout {
                                        spacing: Theme.space2
                                        Text { text: target; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                        Rectangle { height: 16; radius: 8; color: Theme.surfaceAlt; implicitWidth: busT.implicitWidth + 12
                                            Text { id: busT; anchors.centerIn: parent; text: bus + (format ? " · " + format : "")
                                                color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                                    }
                                    Text { text: path; color: Theme.textFaint; font.pixelSize: Theme.fontXs; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                }
                                Text { text: capacityBytes > 0 ? (capacityBytes/1073741824).toFixed(0) + " GiB" : ""
                                    color: Theme.textDim; font.pixelSize: Theme.fontXs }
                                IconButton { glyph: "⏏"; tip: qsTr("Detach"); danger: true
                                    enabled: device !== "disk" || target !== "vda"   // don't detach the boot disk
                                    onClicked: App.detachDisk(root.vm.connectionId, root.vm.uuid, target) }
                            }
                        }
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        visible: diskModel.count === 0
                        glyph: "💽"; title: qsTr("No disks"); body: qsTr("Attach a volume from a storage pool.")
                    }
                }
            }

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
                        delegate: Item {
                            required property string name
                            required property bool isCurrent
                            required property bool hasMemory
                            required property string description
                            required property var created
                            width: ListView.view.width
                            height: 56
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.space2
                                anchors.rightMargin: Theme.space2
                                spacing: Theme.space3
                                Rectangle { width: 8; height: 8; radius: 4; Layout.alignment: Qt.AlignVCenter
                                    color: isCurrent ? Theme.running : Theme.stopped }
                                ColumnLayout {
                                    spacing: 1; Layout.fillWidth: true
                                    RowLayout {
                                        spacing: Theme.space2
                                        Text { text: name; color: Theme.text; font.pixelSize: Theme.fontMd
                                            font.weight: Font.Medium; elide: Text.ElideRight; Layout.maximumWidth: 340 }
                                        Rectangle { visible: isCurrent; height: 16; radius: 8; color: Theme.accentSubtle
                                            implicitWidth: curT.implicitWidth + 12
                                            Text { id: curT; anchors.centerIn: parent; text: qsTr("current")
                                                color: Theme.accent; font.pixelSize: Theme.fontXs } }
                                    }
                                    Text {
                                        text: (hasMemory ? qsTr("Full (with memory)") : qsTr("Disk only"))
                                              + (created ? " · " + Qt.formatDateTime(created, "MMM d, HH:mm") : "")
                                              + (description ? " · " + description : "")
                                        color: Theme.textDim; font.pixelSize: Theme.fontXs
                                        elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                }
                                AppButton { text: qsTr("Restore"); variant: "subtle"; Layout.alignment: Qt.AlignVCenter
                                    onClicked: App.restoreSnapshot(root.vm.connectionId, root.vm.uuid, name) }
                                IconButton { glyph: "🗑"; tip: qsTr("Delete"); danger: true; Layout.alignment: Qt.AlignVCenter
                                    onClicked: App.deleteSnapshot(root.vm.connectionId, root.vm.uuid, name) }
                            }
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
        // Prefill from an existing schedule so it can be edited, not just re-created.
        onOpened: {
            everyMin.value = 60; retain.value = 7;
            var list = App.scheduler.schedules();
            for (var i = 0; i < list.length; ++i)
                if (root.vm && list[i].uuid === root.vm.uuid) {
                    everyMin.value = list[i].intervalMinutes; retain.value = list[i].retain; break;
                }
        }
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

    // Attach-disk dialog: pick an existing volume from a pool + bus.
    Dialog {
        id: attachDialog
        anchors.centerIn: parent; modal: true; width: 480; padding: Theme.space5
        background: Card {}
        ListModel { id: apPools }
        ListModel { id: apVols }
        Connections {
            target: App
            function onStorageLoaded(cid, pools) {
                if (!root.vm || cid !== root.vm.connectionId || !attachDialog.visible) return;
                apPools.clear();
                for (let i = 0; i < pools.length; ++i) apPools.append({ text: pools[i].name });
                if (pools.length > 0) App.loadVolumes(root.vm.connectionId, pools[0].name);
            }
            function onVolumesLoaded(cid, poolName, vols) {
                if (!root.vm || cid !== root.vm.connectionId || !attachDialog.visible) return;
                apVols.clear();
                for (let i = 0; i < vols.length; ++i)
                    apVols.append({ text: vols[i].name, path: vols[i].path, format: vols[i].format || "qcow2" });
            }
        }
        onOpened: { apVols.clear(); App.loadStorage(root.vm.connectionId); }
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Attach a disk"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            RowLayout { Layout.fillWidth: true; spacing: Theme.space3
                Text { text: qsTr("Pool"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 64 }
                AppComboBox { id: apPool; Layout.fillWidth: true; textRole: "text"; model: apPools
                    onActivated: App.loadVolumes(root.vm.connectionId, currentText) } }
            RowLayout { Layout.fillWidth: true; spacing: Theme.space3
                Text { text: qsTr("Volume"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 64 }
                AppComboBox { id: apVol; Layout.fillWidth: true; textRole: "text"; model: apVols } }
            RowLayout { Layout.fillWidth: true; spacing: Theme.space3
                Text { text: qsTr("Bus"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 64 }
                AppComboBox { id: apBus; Layout.fillWidth: true; model: ["virtio", "sata", "scsi"] } }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: attachDialog.close() }
                AppButton { text: qsTr("Attach"); variant: "primary"
                    enabled: apVol.currentIndex >= 0 && apVols.count > 0
                    onClicked: {
                        var v = apVols.get(apVol.currentIndex);
                        App.attachDisk(root.vm.connectionId, root.vm.uuid, v.path, apBus.currentText, v.format);
                        attachDialog.close();
                    }
                }
            }
        }
    }
}
