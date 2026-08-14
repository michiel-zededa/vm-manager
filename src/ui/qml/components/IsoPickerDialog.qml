import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Browse install ISOs that live in the selected host's storage pools. Essential
// for remote hosts, where a local Mac file isn't reachable by the remote qemu.
Dialog {
    id: dlg
    anchors.centerIn: Overlay.overlay
    modal: true
    width: 560
    height: 520
    padding: Theme.space5
    background: Card {}

    property string connId: ""
    signal picked(string path)

    function looksLikeIso(name) {
        const n = (name || "").toLowerCase();
        return n.endsWith(".iso") || n.endsWith(".img");
    }
    function fmtBytes(b) {
        if (b >= 1073741824) return (b / 1073741824).toFixed(1) + " GiB";
        if (b >= 1048576) return (b / 1048576).toFixed(0) + " MiB";
        return b + " B";
    }

    ListModel { id: isoModel }
    property var _pending: ({})    // pools we're still waiting on

    function reload() {
        isoModel.clear();
        _pending = ({});
        App.loadStorage(connId);
    }
    onOpened: reload()

    Connections {
        target: App
        function onStorageLoaded(cid, pools) {
            if (cid !== dlg.connId) return;
            for (let i = 0; i < pools.length; ++i)
                App.loadVolumes(dlg.connId, pools[i].name);
        }
        function onVolumesLoaded(cid, poolName, vols) {
            if (cid !== dlg.connId) return;
            for (let i = 0; i < vols.length; ++i)
                if (dlg.looksLikeIso(vols[i].name))
                    isoModel.append({ name: vols[i].name, path: vols[i].path,
                                      pool: poolName, sizeBytes: vols[i].capacityBytes });
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space4
        Text { text: qsTr("ISOs on this host"); color: Theme.text
            font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
        Text { text: qsTr("Install media found in the host's storage pools.")
            color: Theme.textDim; font.pixelSize: Theme.fontSm }

        Card {
            Layout.fillWidth: true; Layout.fillHeight: true
            ListView {
                anchors.fill: parent; anchors.margins: Theme.space2; clip: true
                model: isoModel; spacing: Theme.space1
                delegate: Item {
                    required property string name
                    required property string path
                    required property string pool
                    required property var sizeBytes
                    width: ListView.view.width
                    height: 48
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 1
                        radius: Theme.radiusSm
                        color: rowHover.hovered ? Theme.surfaceAlt : "transparent"
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: Theme.space3; anchors.rightMargin: Theme.space3
                            spacing: Theme.space3
                            Text { text: "💿"; font.pixelSize: Theme.fontMd }
                            ColumnLayout {
                                spacing: 0; Layout.fillWidth: true
                                Text { text: name; color: Theme.text; font.pixelSize: Theme.fontMd
                                    elide: Text.ElideRight; Layout.fillWidth: true }
                                Text { text: pool + " · " + path; color: Theme.textFaint
                                    font.pixelSize: Theme.fontXs; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            }
                            Text { text: dlg.fmtBytes(sizeBytes); color: Theme.textDim; font.pixelSize: Theme.fontXs }
                        }
                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: { dlg.picked(path); dlg.close(); } }
                    }
                }
            }
            EmptyState {
                anchors.centerIn: parent
                visible: isoModel.count === 0
                glyph: "💿"; title: qsTr("No ISOs found")
                body: qsTr("Upload an .iso into one of this host's storage pools, then refresh.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            AppButton { text: qsTr("↻  Refresh"); variant: "ghost"; onClicked: dlg.reload() }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: dlg.close() }
        }
    }
}
