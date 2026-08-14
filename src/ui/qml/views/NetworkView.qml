import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Virtual networks and bridges on the selected host: create, start/stop, delete.
Item {
    id: root

    ListModel { id: netModel }
    Connections {
        target: App
        function onNetworksLoaded(connId, nets) {
            netModel.clear();
            for (let i = 0; i < nets.length; ++i) netModel.append(nets[i]);
        }
    }
    function reload() { App.loadNetworks(App.currentConnectionId); }
    Component.onCompleted: reload()
    Connections { target: App; function onCurrentConnectionChanged() { root.reload(); } }

    function modeColor(mode) {
        switch (mode) {
        case "nat": return Theme.info;
        case "bridge": return Theme.running;
        case "route": case "isolated": return Theme.accent;
        default: return Theme.stopped;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space5

        RowLayout {
            Layout.fillWidth: true
            SectionHeader { title: qsTr("Networks"); subtitle: qsTr("Virtual networks and bridges on this host") }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("↻  Refresh"); variant: "ghost"; onClicked: root.reload() }
            AppButton { text: qsTr("＋  New network"); variant: "primary"; onClicked: newNetDialog.open() }
        }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            Flow {
                width: parent.width
                spacing: Theme.space4
                Repeater {
                    model: netModel
                    delegate: Card {
                        id: netCard
                        required property string name
                        required property string mode
                        required property string bridge
                        required property bool active
                        required property string forwardDev
                        width: 300; implicitHeight: 104
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space2
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: netCard.name; color: Theme.text; font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                                Rectangle {
                                    height: 18; radius: 9; implicitWidth: modeText.implicitWidth + 14
                                    color: Qt.rgba(root.modeColor(netCard.mode).r, root.modeColor(netCard.mode).g, root.modeColor(netCard.mode).b, 0.18)
                                    Text { id: modeText; anchors.centerIn: parent; text: netCard.mode
                                        color: root.modeColor(netCard.mode); font.pixelSize: Theme.fontXs; font.weight: Font.Medium }
                                }
                            }
                            Text { text: qsTr("Bridge: %1").arg(netCard.bridge || "–") + (netCard.forwardDev ? "  → " + netCard.forwardDev : "")
                                color: Theme.textDim; font.pixelSize: Theme.fontSm; elide: Text.ElideRight; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.space2
                                Rectangle { width: 8; height: 8; radius: 4; color: netCard.active ? Theme.running : Theme.stopped }
                                Text { text: netCard.active ? qsTr("Active") : qsTr("Inactive"); color: Theme.textDim; font.pixelSize: Theme.fontXs }
                                Item { Layout.fillWidth: true }
                                IconButton { glyph: netCard.active ? "⏸" : "▶"
                                    tip: netCard.active ? qsTr("Stop") : qsTr("Start")
                                    onClicked: App.setNetworkActive(App.currentConnectionId, netCard.name, !netCard.active) }
                                IconButton { glyph: "🗑"; tip: qsTr("Delete"); danger: true
                                    enabled: netCard.name !== "default"
                                    onClicked: { delNetDialog.netName = netCard.name; delNetDialog.open(); } }
                            }
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            visible: netModel.count === 0
            glyph: "⇄"; title: qsTr("No networks")
            body: qsTr("Create a NAT, isolated or bridged virtual network for your VMs.")
        }
    }

    // ===== New network dialog =====
    Dialog {
        id: newNetDialog
        anchors.centerIn: parent; modal: true; width: 480; padding: Theme.space5
        background: Card {}
        onOpened: { netName.text = ""; netMode.currentIndex = 0; fwdField.text = ""; netName.forceActiveFocus(); }
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("New virtual network"); color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            AppTextField { id: netName; Layout.fillWidth: true; placeholderText: qsTr("Network name (e.g. lab-net)") }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.space3
                Text { text: qsTr("Mode"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 70 }
                AppComboBox { id: netMode; Layout.fillWidth: true
                    model: [qsTr("NAT (internet via host)"), qsTr("Isolated (VMs only)"), qsTr("Bridged (host LAN)")] }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.space3
                visible: netMode.currentIndex === 2
                Text { text: qsTr("Host bridge"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 70 }
                AppTextField { id: fwdField; Layout.fillWidth: true; placeholderText: qsTr("e.g. br0") }
            }
            Text {
                text: netMode.currentIndex === 0 ? qsTr("A private subnet with DHCP; VMs reach the internet through the host.")
                    : netMode.currentIndex === 1 ? qsTr("A private subnet with DHCP; no outside access.")
                    : qsTr("Attaches VMs directly to an existing host bridge on your LAN.")
                color: Theme.textFaint; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: newNetDialog.close() }
                AppButton {
                    text: qsTr("Create"); variant: "primary"
                    enabled: netName.text.trim() && (netMode.currentIndex !== 2 || fwdField.text.trim())
                    onClicked: {
                        var mode = netMode.currentIndex === 1 ? "isolated" : netMode.currentIndex === 2 ? "bridge" : "nat";
                        App.createNetwork(App.currentConnectionId, netName.text.trim(), mode, fwdField.text.trim());
                        newNetDialog.close();
                    }
                }
            }
        }
    }

    // ===== Delete confirmation =====
    Dialog {
        id: delNetDialog
        property string netName: ""
        anchors.centerIn: parent; modal: true; width: 440; padding: Theme.space5
        background: Card {}
        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Delete network ") + delNetDialog.netName + "?"; color: Theme.text
                font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text { text: qsTr("VMs attached to it will lose this network until reconfigured.")
                color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: delNetDialog.close() }
                AppButton { text: qsTr("Delete"); variant: "danger"
                    onClicked: { App.deleteNetwork(App.currentConnectionId, delNetDialog.netName); delNetDialog.close(); } }
            }
        }
    }
}
