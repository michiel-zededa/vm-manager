import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Virtual networks and bridges on the selected host.
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
        case "route": return Theme.accent;
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
            AppButton { text: qsTr("↻ Refresh"); variant: "ghost"; onClicked: root.reload() }
            AppButton { text: qsTr("＋ New network"); variant: "primary"; enabled: false }
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
                        width: 280; implicitHeight: 96
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space2
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: name; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                                Rectangle {
                                    height: 18; radius: 9; implicitWidth: modeText.implicitWidth + 14
                                    color: Qt.rgba(root.modeColor(mode).r, root.modeColor(mode).g, root.modeColor(mode).b, 0.18)
                                    Text { id: modeText; anchors.centerIn: parent; text: mode; color: root.modeColor(mode); font.pixelSize: Theme.fontXs; font.weight: Font.Medium }
                                }
                            }
                            Text { text: qsTr("Bridge: %1").arg(bridge || "–"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                            RowLayout {
                                spacing: Theme.space2
                                Rectangle { width: 8; height: 8; radius: 4; color: active ? Theme.running : Theme.stopped }
                                Text { text: active ? qsTr("Active") : qsTr("Inactive"); color: Theme.textDim; font.pixelSize: Theme.fontXs }
                                Item { Layout.fillWidth: true }
                                Text { visible: forwardDev.length > 0; text: "→ " + forwardDev; color: Theme.textFaint; font.pixelSize: Theme.fontXs }
                            }
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.alignment: Qt.AlignHCenter
            visible: netModel.count === 0
            glyph: "⇄"; title: qsTr("No networks"); body: qsTr("Connect a host to see its virtual networks.")
        }
    }
}
