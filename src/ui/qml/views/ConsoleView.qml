import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Console surface for a VM. Shows the real graphical (VNC/SPICE) endpoint from
// libvirt with one-click open in the OS viewer, plus a serial-console panel.
// In-app pixel streaming (embedded RFB) is the next step; the plumbing and the
// endpoint are live now.
Item {
    id: root
    property var vm: null

    property var info: ({})
    property int mode: 0            // 0 = graphical, 1 = serial
    readonly property bool running: vm && vm.state === 1

    function reload() { if (vm) App.loadConsole(vm.connectionId, vm.uuid) }
    Connections {
        target: App
        function onConsoleLoaded(uuid, console) { if (vm && uuid === vm.uuid) root.info = console }
    }
    Component.onCompleted: reload()
    onVmChanged: reload()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space3

        // Mode toggle + status
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            Repeater {
                model: [{ m: 0, t: qsTr("Graphical") }, { m: 1, t: qsTr("Serial") }]
                delegate: AppButton {
                    required property var modelData
                    text: modelData.t
                    variant: root.mode === modelData.m ? "subtle" : "ghost"
                    onClicked: root.mode = modelData.m
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle { width: 8; height: 8; radius: 4; color: root.running ? Theme.running : Theme.stopped }
            Text {
                text: root.running ? qsTr("running") : qsTr("stopped")
                color: root.running ? Theme.running : Theme.textDim; font.pixelSize: Theme.fontSm
            }
        }

        // Console frame
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: "#0A0C10"
            border.color: Theme.border
            border.width: 1
            clip: true

            // ---- Graphical ----
            ColumnLayout {
                anchors.centerIn: parent
                visible: root.mode === 0
                spacing: Theme.space3
                width: Math.min(500, parent.width - Theme.space6)

                Text { text: "🖥"; font.pixelSize: 48; color: "#3B4252"; Layout.alignment: Qt.AlignHCenter }

                Text {
                    text: !root.running ? qsTr("Start the VM to open its console")
                         : (root.info.port > 0 ? qsTr("Graphical console ready")
                                               : qsTr("No graphical device configured"))
                    color: "#C9D1E0"; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter; horizontalAlignment: Text.AlignHCenter
                }

                // Endpoint chip
                Rectangle {
                    visible: root.running && root.info.port > 0
                    Layout.alignment: Qt.AlignHCenter
                    radius: Theme.radiusSm; color: "#12151C"; border.color: Theme.border; border.width: 1
                    implicitWidth: ep.implicitWidth + Theme.space4; implicitHeight: 34
                    Text {
                        id: ep; anchors.centerIn: parent
                        text: (root.info.graphicsType || "vnc") + "://" + (root.info.host || "127.0.0.1") + ":" + root.info.port
                        color: "#8FA0BC"; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                    }
                }

                RowLayout {
                    visible: root.running && root.info.port > 0
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.space2
                    AppButton {
                        text: qsTr("Open in viewer"); variant: "primary"
                        onClicked: App.openConsoleExternally(root.vm.connectionId, root.vm.uuid)
                    }
                    AppButton {
                        text: qsTr("Copy address"); variant: "ghost"
                        onClicked: { epClip.text = ep.text; epClip.selectAll(); epClip.copy(); }
                    }
                }
                TextEdit { id: epClip; visible: false }

                Text {
                    text: qsTr("Embedded in-app streaming is coming; for now the console opens in your system viewer.")
                    color: "#5C6575"; font.pixelSize: Theme.fontXs
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }

            // ---- Serial ----
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space3
                visible: root.mode === 1
                spacing: Theme.space2

                Text {
                    text: root.info.hasSerial ? qsTr("Serial console") : qsTr("No serial device on this VM")
                    color: "#C9D1E0"; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    radius: Theme.radiusSm; color: "#05070A"; border.color: Theme.border; border.width: 1
                    Text {
                        anchors.fill: parent; anchors.margins: Theme.space3
                        text: root.running
                              ? qsTr("$ virsh -c %1 console %2\nConnected to domain.\nEscape character is ^]\n\n")
                                    .arg(root.vm ? root.vm.connectionId : "").arg(root.vm ? root.vm.name : "")
                              : qsTr("VM is not running.")
                        color: "#7FE0A0"; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                        wrapMode: Text.WrapAnywhere; verticalAlignment: Text.AlignTop
                    }
                }
                Text {
                    text: qsTr("Live serial attach (libvirt stream) lands next; the command above works today.")
                    color: "#5C6575"; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
    }
}
