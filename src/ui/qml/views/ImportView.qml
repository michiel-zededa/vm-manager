import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import VMManager

// Import an existing disk image (VMware/Hyper-V/VirtualBox/QEMU) or appliance.
// Converts to qcow2 via qemu-img (progress shown) then defines a VM.
Popup {
    id: importer
    anchors.centerIn: parent
    width: 620
    height: 520
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    background: Card {}

    property string sourcePath: ""
    property string detected: ""

    onOpened: { sourcePath = ""; detected = ""; nameField.text = ""; }

    Connections {
        target: App.importer
        function onFinished(ok, path, error) { /* toast handles messaging */ if (ok) importer.close(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4

        SectionHeader {
            title: qsTr("Import a disk image")
            subtitle: qsTr("VMware, Hyper-V, VirtualBox and QEMU images are converted automatically")
        }

        // Drop / pick zone
        Card {
            Layout.fillWidth: true
            implicitHeight: 130
            color: dropArea.containsDrag ? Theme.accentSubtle : Theme.surfaceAlt
            border.color: dropArea.containsDrag ? Theme.accent : Theme.border

            DropArea {
                id: dropArea
                anchors.fill: parent
                onDropped: (drop) => {
                    if (drop.hasUrls && drop.urls.length > 0) {
                        importer.sourcePath = drop.urls[0].toString().replace("file://", "");
                        importer.detected = App.importer.detectFormat(importer.sourcePath);
                        drop.accept();
                    }
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.space2
                Text { text: "⤓"; font.pixelSize: 34; color: Theme.textDim; Layout.alignment: Qt.AlignHCenter }
                Text {
                    text: importer.sourcePath ? importer.sourcePath : qsTr("Drop an image here, or")
                    color: Theme.text; font.pixelSize: Theme.fontSm; Layout.alignment: Qt.AlignHCenter
                    elide: Text.ElideMiddle; Layout.maximumWidth: importer.width - 100
                }
                AppButton { text: qsTr("Browse…"); variant: "ghost"; Layout.alignment: Qt.AlignHCenter; onClicked: fileDialog.open() }
            }
        }

        // Detected format
        RowLayout {
            visible: importer.detected.length > 0
            spacing: Theme.space2
            Rectangle { width: 8; height: 8; radius: 4; color: Theme.running }
            Text { text: qsTr("Detected: .%1").arg(importer.detected); color: Theme.textDim; font.pixelSize: Theme.fontSm }
        }

        AppTextField {
            id: nameField; Layout.fillWidth: true
            placeholderText: qsTr("New VM name")
        }

        // Progress
        ColumnLayout {
            Layout.fillWidth: true
            visible: App.importer.busy
            spacing: Theme.space2
            Text { text: App.importer.statusText; color: Theme.textDim; font.pixelSize: Theme.fontSm }
            ProgressBar {
                Layout.fillWidth: true
                indeterminate: App.importer.progress < 0
                value: App.importer.progress < 0 ? 0 : App.importer.progress
            }
        }

        Item { Layout.fillHeight: true }

        Text {
            visible: !App.importer.available
            text: qsTr("⚠ qemu-img not found. Install qemu (brew install qemu) to enable format conversion.")
            color: Theme.paused; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: importer.close() }
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Import")
                variant: "primary"
                enabled: importer.sourcePath.length > 0 && nameField.text.trim().length > 0 && !App.importer.busy
                onClicked: App.importImage(App.currentConnectionId, importer.sourcePath, {
                    name: nameField.text.trim(),
                    diskFormat: "qcow2",
                })
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Select disk image")
        nameFilters: [
            "Disk images (*.qcow2 *.raw *.img *.vmdk *.vhdx *.vhd *.vdi *.qed *.ova *.ovf)",
            "All files (*)"
        ]
        onAccepted: {
            importer.sourcePath = selectedFile.toString().replace("file://", "");
            importer.detected = App.importer.detectFormat(importer.sourcePath);
        }
    }
}
