import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import VMManager

// Storage pools + volumes on the selected host. Pools expand to reveal their
// volumes; supports creating/deleting pools (including adopting a host folder)
// and creating/deleting volumes.
Item {
    id: root

    function fmtBytes(b) {
        if (b >= 1099511627776) return (b / 1099511627776).toFixed(1) + " TiB";
        if (b >= 1073741824) return (b / 1073741824).toFixed(0) + " GiB";
        if (b >= 1048576) return (b / 1048576).toFixed(0) + " MiB";
        return b + " B";
    }
    // Friendly, capitalized labels for libvirt pool types.
    function friendlyType(t) {
        switch ((t || "").toLowerCase()) {
        case "dir":     return qsTr("Directory");
        case "fs":      return qsTr("Filesystem");
        case "netfs":   return qsTr("Network FS");
        case "nfs":     return qsTr("NFS");
        case "logical": return qsTr("LVM group");
        case "disk":    return qsTr("Disk");
        case "iscsi":
        case "iscsi-direct": return qsTr("iSCSI");
        case "scsi":    return qsTr("SCSI");
        case "zfs":     return qsTr("ZFS");
        case "rbd":     return qsTr("Ceph RBD");
        case "gluster": return qsTr("Gluster");
        default:        return t ? t.charAt(0).toUpperCase() + t.slice(1) : qsTr("Pool");
        }
    }

    property var volumesByPool: ({})     // poolName -> array of volume maps
    property var expanded: ({})          // poolName -> bool

    ListModel { id: poolModel }
    Connections {
        target: App
        function onStorageLoaded(connId, pools) {
            poolModel.clear();
            for (let i = 0; i < pools.length; ++i) poolModel.append(pools[i]);
        }
        function onVolumesLoaded(connId, poolName, vols) {
            let m = Object.assign({}, root.volumesByPool);
            m[poolName] = vols;
            root.volumesByPool = m;
        }
    }
    function reload() { App.loadStorage(App.currentConnectionId); }
    function toggle(poolName) {
        let e = Object.assign({}, expanded);
        e[poolName] = !e[poolName];
        expanded = e;
        if (e[poolName]) App.loadVolumes(App.currentConnectionId, poolName);
    }
    Component.onCompleted: reload()
    Connections { target: App; function onCurrentConnectionChanged() { root.reload(); } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space5

        RowLayout {
            Layout.fillWidth: true
            SectionHeader { title: qsTr("Storage"); subtitle: qsTr("Pools and volumes on this host") }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("↻  Refresh"); variant: "ghost"; onClicked: root.reload() }
            AppButton { text: qsTr("＋  New pool"); variant: "primary"; onClicked: newPoolDialog.open() }
        }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.space3
                Repeater {
                    model: poolModel
                    delegate: Card {
                        id: poolCard
                        required property int index
                        required property string name
                        required property string type
                        required property bool active
                        required property var allocationBytes
                        required property var capacityBytes
                        required property string targetPath
                        required property bool autostart
                        readonly property bool isOpen: root.expanded[name] === true
                        readonly property var vols: root.volumesByPool[name] || []
                        Layout.fillWidth: true
                        implicitHeight: body.implicitHeight + Theme.space4 * 2

                        ColumnLayout {
                            id: body
                            anchors.fill: parent
                            anchors.margins: Theme.space4
                            spacing: Theme.space3

                            // ---- Pool header row ----
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space3
                                Rectangle {
                                    width: 28; height: 28; radius: Theme.radiusSm
                                    color: chevHover.hovered ? Theme.surfaceHover : Theme.surfaceAlt
                                    border.width: 1; border.color: Theme.border
                                    Text { anchors.centerIn: parent; text: poolCard.isOpen ? "▾" : "▸"
                                        color: Theme.accent; font.pixelSize: Theme.fontMd; font.bold: true }
                                    HoverHandler { id: chevHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: root.toggle(poolCard.name) }
                                }
                                Rectangle { width: 8; height: 8; radius: 4
                                    color: poolCard.active ? Theme.running : Theme.stopped }
                                Text { text: poolCard.name; color: Theme.text
                                    font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
                                    TapHandler { onTapped: root.toggle(poolCard.name) }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor } }
                                Rectangle { height: 18; radius: 9; color: Theme.surfaceAlt
                                    implicitWidth: typeText.implicitWidth + 14
                                    Text { id: typeText; anchors.centerIn: parent; text: root.friendlyType(poolCard.type)
                                        color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: root.fmtBytes(poolCard.allocationBytes) + " / " + root.fmtBytes(poolCard.capacityBytes)
                                    color: Theme.textDim; font.pixelSize: Theme.fontSm
                                }
                                IconButton { glyph: poolCard.active ? "⏸" : "▶"
                                    tip: poolCard.active ? qsTr("Stop pool") : qsTr("Start pool")
                                    onClicked: App.setStoragePoolActive(App.currentConnectionId, poolCard.name, !poolCard.active) }
                                IconButton { glyph: "🗑"; tip: qsTr("Delete pool"); danger: true
                                    onClicked: { delPoolDialog.poolName = poolCard.name; delPoolDialog.open(); } }
                            }

                            // ---- Usage bar ----
                            Rectangle {
                                Layout.fillWidth: true; height: 8; radius: 4; color: Theme.surfaceAlt
                                Rectangle {
                                    height: parent.height; radius: 4
                                    width: parent.width * (poolCard.capacityBytes > 0 ? Math.min(1, poolCard.allocationBytes / poolCard.capacityBytes) : 0)
                                    color: (poolCard.allocationBytes / Math.max(1, poolCard.capacityBytes)) > 0.85 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.durMed; easing.type: Theme.easing } }
                                }
                            }

                            // ---- Volumes (when expanded) ----
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: poolCard.isOpen
                                spacing: Theme.space1

                                // Pool metadata: folder path + autostart
                                RowLayout {
                                    Layout.fillWidth: true; spacing: Theme.space2
                                    Text { text: "📁"; font.pixelSize: Theme.fontSm }
                                    Text {
                                        text: poolCard.targetPath || qsTr("(path unknown)")
                                        color: Theme.textDim; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                                        Layout.fillWidth: true; elide: Text.ElideMiddle
                                    }
                                    Rectangle {
                                        visible: poolCard.autostart
                                        height: 18; radius: 9; color: Theme.accentSubtle
                                        implicitWidth: asText.implicitWidth + 14
                                        Text { id: asText; anchors.centerIn: parent; text: qsTr("autostart")
                                            color: Theme.accent; font.pixelSize: Theme.fontXs }
                                    }
                                }

                                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                                Repeater {
                                    model: poolCard.vols
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: Theme.space3
                                        Text { text: "📄"; font.pixelSize: Theme.fontMd }
                                        Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontSm }
                                        Rectangle { height: 16; radius: 8; color: Theme.surfaceAlt
                                            implicitWidth: fmtText.implicitWidth + 12
                                            Text { id: fmtText; anchors.centerIn: parent; text: modelData.format
                                                color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                                        Item { Layout.fillWidth: true }
                                        Text { text: root.fmtBytes(modelData.capacityBytes)
                                            color: Theme.textDim; font.pixelSize: Theme.fontXs }
                                        IconButton { glyph: "🗑"; tip: qsTr("Delete volume"); danger: true
                                            onClicked: { delVolDialog.poolName = poolCard.name;
                                                         delVolDialog.volName = modelData.name; delVolDialog.open(); } }
                                    }
                                }
                                Text {
                                    visible: poolCard.vols.length === 0
                                    text: qsTr("No volumes in this pool yet.")
                                    color: Theme.textFaint; font.pixelSize: Theme.fontSm
                                    Layout.topMargin: Theme.space1
                                }
                                AppButton {
                                    text: qsTr("＋  New volume"); variant: "subtle"
                                    Layout.topMargin: Theme.space1
                                    onClicked: { newVolDialog.poolName = poolCard.name; newVolDialog.open(); }
                                }
                            }
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            visible: poolModel.count === 0
            glyph: "▤"; title: qsTr("No storage pools")
            body: qsTr("Create a pool, or adopt an existing folder of disk images.")
        }
    }

    // ===== New pool dialog =====
    FolderDialog {
        id: folderPicker
        onAccepted: newPoolPath.text = selectedFolder.toString().replace("file://", "")
    }
    Dialog {
        id: newPoolDialog
        anchors.centerIn: parent
        modal: true; width: 480; padding: Theme.space5
        background: Card {}
        onOpened: { newPoolName.text = ""; newPoolPath.text = ""; newPoolName.forceActiveFocus(); }
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("New storage pool"); color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text { text: qsTr("A directory pool stores disk images as files in a host folder.")
                color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            AppTextField { id: newPoolName; Layout.fillWidth: true; placeholderText: qsTr("Pool name (e.g. vms)") }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.space2
                AppTextField { id: newPoolPath; Layout.fillWidth: true; placeholderText: qsTr("/path/to/folder") }
                AppButton { text: qsTr("Browse…"); variant: "ghost"; onClicked: folderPicker.open() }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: newPoolDialog.close() }
                AppButton {
                    text: qsTr("Create pool"); variant: "primary"
                    enabled: newPoolName.text.trim() && newPoolPath.text.trim()
                    onClicked: {
                        App.createStoragePool(App.currentConnectionId, newPoolName.text.trim(),
                                              "dir", newPoolPath.text.trim());
                        newPoolDialog.close();
                    }
                }
            }
        }
    }

    // ===== New volume dialog =====
    Dialog {
        id: newVolDialog
        property string poolName: ""
        anchors.centerIn: parent
        modal: true; width: 460; padding: Theme.space5
        background: Card {}
        onOpened: { newVolName.text = ""; newVolSize.value = 20; newVolName.forceActiveFocus(); }
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("New volume in ") + newVolDialog.poolName; color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            AppTextField { id: newVolName; Layout.fillWidth: true; placeholderText: qsTr("Volume name (e.g. data)") }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.space3
                Text { text: qsTr("Format"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 70 }
                AppComboBox { id: newVolFormat; Layout.fillWidth: true; model: ["qcow2", "raw"] }
            }
            LabeledSlider {
                id: newVolSize; label: qsTr("Size"); from: 1; to: 2048; stepSize: 1; value: 20; decimals: 0
                unitOptions: [{name: "GiB", factor: 1}, {name: "TiB", factor: 1024}]
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: newVolDialog.close() }
                AppButton {
                    text: qsTr("Create"); variant: "primary"
                    enabled: newVolName.text.trim()
                    onClicked: {
                        App.createVolume(App.currentConnectionId, newVolDialog.poolName,
                                         newVolName.text.trim(), newVolFormat.currentText, newVolSize.value);
                        newVolDialog.close();
                    }
                }
            }
        }
    }

    // ===== Delete confirmations =====
    Dialog {
        id: delPoolDialog
        property string poolName: ""
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Delete pool ") + delPoolDialog.poolName + "?"; color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            AppCheckBox { id: delPoolContents; text: qsTr("Also delete the pool's files on disk (irreversible)") }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: delPoolDialog.close() }
                AppButton { text: qsTr("Delete"); variant: "danger"
                    onClicked: { App.deleteStoragePool(App.currentConnectionId, delPoolDialog.poolName, delPoolContents.checked);
                                 delPoolDialog.close(); } }
            }
        }
    }
    Dialog {
        id: delVolDialog
        property string poolName: ""
        property string volName: ""
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Delete volume ") + delVolDialog.volName + "?"; color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text { text: qsTr("This permanently removes the disk image file.")
                color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: delVolDialog.close() }
                AppButton { text: qsTr("Delete"); variant: "danger"
                    onClicked: { App.deleteVolume(App.currentConnectionId, delVolDialog.poolName, delVolDialog.volName);
                                 delVolDialog.close(); } }
            }
        }
    }
}
