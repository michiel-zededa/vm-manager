import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Embedded console surface. The graphical (VNC) and serial consoles land in
// phase 2; this presents the connection target and a clear placeholder so the
// flow is in place and testable now.
Item {
    id: root
    property var vm: null

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            AppButton { text: qsTr("Graphical (VNC)"); variant: "subtle" }
            AppButton { text: qsTr("Serial"); variant: "ghost" }
            Item { Layout.fillWidth: true }
            Text {
                text: root.vm && root.vm.state === 1 ? qsTr("● live") : qsTr("VM is not running")
                color: root.vm && root.vm.state === 1 ? Theme.running : Theme.textDim
                font.pixelSize: Theme.fontSm
            }
        }

        // Faux console frame
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: "#0A0C10"
            border.color: Theme.border
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.space3
                width: Math.min(460, parent.width - Theme.space6)

                Text { text: "🖳"; font.pixelSize: 46; color: Theme.textDim; Layout.alignment: Qt.AlignHCenter }
                Text {
                    text: qsTr("Embedded console arrives in phase 2")
                    color: "#C9D1E0"; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: qsTr("A VNC widget (then SPICE with USB redirect) will render right here. "
                             + "Until then, the VM exposes a VNC endpoint you can reach with any viewer.")
                    color: Theme.textDim; font.pixelSize: Theme.fontSm
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Rectangle {
                    visible: root.vm !== null
                    Layout.alignment: Qt.AlignHCenter
                    radius: Theme.radiusSm; color: "#12151C"; border.color: Theme.border; border.width: 1
                    implicitWidth: endpoint.implicitWidth + Theme.space4
                    implicitHeight: 30
                    Text {
                        id: endpoint
                        anchors.centerIn: parent
                        text: root.vm ? "vnc://" + (root.vm.connectionId.indexOf("ssh") >= 0 ? "<host>" : "127.0.0.1") + ":auto" : ""
                        color: "#7C89A0"; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                    }
                }
            }
        }
    }
}
