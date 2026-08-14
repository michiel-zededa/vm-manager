import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// First-run / on-demand dependency check. VM Manager never installs system
// packages itself (that needs elevated privileges); instead it detects what is
// missing and hands over the exact command to run.
Dialog {
    id: dlg
    anchors.centerIn: Overlay.overlay
    modal: true
    width: 560
    padding: Theme.space5
    background: Card {}

    property var status: ({})
    function refresh() { status = App.dependencyStatus(); }
    onAboutToShow: refresh()

    contentItem: ColumnLayout {
        spacing: Theme.space4

        RowLayout {
            Layout.fillWidth: true; spacing: Theme.space3
            Text { text: "🧩"; font.pixelSize: 26 }
            ColumnLayout {
                spacing: 1; Layout.fillWidth: true
                Text { text: qsTr("Virtualization tools"); color: Theme.text
                    font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
                Text { text: qsTr("What VM Manager needs to run real VMs on this machine.")
                    color: Theme.textDim; font.pixelSize: Theme.fontSm }
            }
        }

        // Status rows
        Repeater {
            model: [
                { key: "libvirt",  label: qsTr("libvirt"),  desc: qsTr("hypervisor management daemon") },
                { key: "qemu",     label: qsTr("QEMU"),      desc: qsTr("the virtual machine engine") },
                { key: "qemuImg",  label: qsTr("qemu-img"),  desc: qsTr("disk image conversion (import)") },
            ]
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true; spacing: Theme.space3
                readonly property bool ok: dlg.status[modelData.key] === true
                Rectangle { width: 22; height: 22; radius: 11
                    color: ok ? Theme.success : Theme.warning
                    Text { anchors.centerIn: parent; text: ok ? "✓" : "!"; color: "#fff"; font.bold: true; font.pixelSize: Theme.fontSm } }
                ColumnLayout {
                    spacing: 0; Layout.fillWidth: true
                    Text { text: modelData.label; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                    Text { text: modelData.desc; color: Theme.textDim; font.pixelSize: Theme.fontXs }
                }
                Text { text: ok ? qsTr("Found") : qsTr("Missing")
                    color: ok ? Theme.success : Theme.warning; font.pixelSize: Theme.fontSm }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        // Install command (copyable) when something is missing
        ColumnLayout {
            Layout.fillWidth: true; spacing: Theme.space2
            visible: dlg.status.libvirt !== true || dlg.status.qemu !== true
            Text { text: qsTr("Run this in a terminal to install what's missing:")
                color: Theme.textDim; font.pixelSize: Theme.fontSm }
            Rectangle {
                Layout.fillWidth: true; radius: Theme.radiusSm; color: Theme.surfaceAlt
                border.width: 1; border.color: Theme.border
                implicitHeight: cmdText.implicitHeight + Theme.space3
                RowLayout {
                    anchors.fill: parent; anchors.margins: Theme.space2; spacing: Theme.space2
                    TextEdit {
                        id: cmdText; Layout.fillWidth: true
                        text: App.installHint("all")
                        readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                        color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                    }
                    AppButton { text: copied ? qsTr("Copied ✓") : qsTr("Copy"); variant: "subtle"
                        property bool copied: false
                        onClicked: { cmdText.selectAll(); cmdText.copy(); cmdText.deselect();
                                     copied = true; copyReset.restart(); }
                        Timer { id: copyReset; interval: 1500; onTriggered: parent.copied = false } }
                }
            }
            Text {
                text: qsTr("VM Manager won't install anything for you — this needs your administrator password.")
                color: Theme.textFaint; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
        }

        Text {
            visible: dlg.status.libvirt === true && dlg.status.qemu === true
            text: qsTr("All set — real virtualization is available on this machine.")
            color: Theme.success; font.pixelSize: Theme.fontSm
        }

        RowLayout {
            Layout.fillWidth: true
            AppButton { text: qsTr("Re-check"); variant: "ghost"; onClicked: dlg.refresh() }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Close"); variant: "primary"; onClicked: dlg.close() }
        }
    }
}
