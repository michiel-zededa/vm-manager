import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Storage pools on the selected host, with usage bars.
Item {
    id: root

    function fmtBytes(b) {
        if (b >= 1099511627776) return (b / 1099511627776).toFixed(1) + " TiB";
        if (b >= 1073741824) return (b / 1073741824).toFixed(0) + " GiB";
        if (b >= 1048576) return (b / 1048576).toFixed(0) + " MiB";
        return b + " B";
    }

    ListModel { id: poolModel }
    Connections {
        target: App
        function onStorageLoaded(connId, pools) {
            poolModel.clear();
            for (let i = 0; i < pools.length; ++i) poolModel.append(pools[i]);
        }
    }
    function reload() { App.loadStorage(App.currentConnectionId); }
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
            AppButton { text: qsTr("↻ Refresh"); variant: "ghost"; onClicked: root.reload() }
            AppButton { text: qsTr("＋ New pool"); variant: "primary"; enabled: false }
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
                        Layout.fillWidth: true
                        implicitHeight: 96
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space4
                            spacing: Theme.space2
                            RowLayout {
                                Layout.fillWidth: true
                                Rectangle { width: 8; height: 8; radius: 4; color: active ? Theme.running : Theme.stopped }
                                Text { text: name; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Rectangle { height: 16; radius: 8; color: Theme.surfaceAlt; implicitWidth: typeText.implicitWidth + 12
                                    Text { id: typeText; anchors.centerIn: parent; text: type; color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: root.fmtBytes(allocationBytes) + " / " + root.fmtBytes(capacityBytes)
                                    color: Theme.textDim; font.pixelSize: Theme.fontSm
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true; height: 8; radius: 4; color: Theme.surfaceAlt
                                Rectangle {
                                    height: parent.height; radius: 4
                                    width: parent.width * (capacityBytes > 0 ? Math.min(1, allocationBytes / capacityBytes) : 0)
                                    color: (allocationBytes / Math.max(1, capacityBytes)) > 0.85 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.durMed; easing.type: Theme.easing } }
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
            glyph: "▤"; title: qsTr("No storage pools"); body: qsTr("Connect a host to see its storage pools.")
        }
    }
}
