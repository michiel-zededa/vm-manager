import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import VMManager

// Guided VM creation. Simple by default (name + OS + go); an Advanced toggle
// exposes full hardware control. UTM/RPi-Imager inspired.
Popup {
    id: wizard
    anchors.centerIn: parent
    width: 640
    height: 560
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    background: Card {}

    property int step: 0
    property bool advanced: false
    readonly property int lastStep: 2

    function reset() { step = 0; nameField.text = ""; }
    onOpened: reset()

    // OS gallery entries (a curated manifest ships in phase 2; static for now).
    property var osCatalog: [
        { name: "Ubuntu 24.04 LTS", variant: "ubuntu24.04", glyph: "🟠" },
        { name: "Fedora 40",        variant: "fedora40",    glyph: "🎩" },
        { name: "Debian 12",        variant: "debian12",    glyph: "🌀" },
        { name: "Windows 11",       variant: "win11",       glyph: "🪟" },
        { name: "Alpine 3.20",      variant: "alpinelinux3.20", glyph: "🏔" },
        { name: "Other / custom",   variant: "",            glyph: "💿" },
    ]
    property string chosenVariant: "ubuntu24.04"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4

        // Header + step indicator
        RowLayout {
            Layout.fillWidth: true
            SectionHeader { title: qsTr("Create a virtual machine"); subtitle: qsTr("on %1").arg(App.currentConnectionId) }
            Item { Layout.fillWidth: true }
            RowLayout {
                spacing: Theme.space2
                Repeater {
                    model: 3
                    delegate: Rectangle {
                        width: index === wizard.step ? 22 : 8; height: 8; radius: 4
                        color: index <= wizard.step ? Theme.accent : Theme.surfaceAlt
                        Behavior on width { NumberAnimation { duration: Theme.durFast } }
                        Behavior on color { ColorAnimation { duration: Theme.durFast } }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: wizard.step

            // ---- Step 1: OS ----
            ColumnLayout {
                spacing: Theme.space3
                Text { text: qsTr("Choose an operating system"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    Repeater {
                        model: wizard.osCatalog
                        delegate: Card {
                            width: 180; implicitHeight: 64
                            interactive: true
                            property bool sel: wizard.chosenVariant === modelData.variant
                            border.color: sel ? Theme.accent : Theme.border
                            HoverHandler { id: oh }
                            hovered: oh.hovered
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: wizard.chosenVariant = modelData.variant }
                            RowLayout {
                                anchors.fill: parent; anchors.margins: Theme.space3; spacing: Theme.space3
                                Text { text: modelData.glyph; font.pixelSize: 22 }
                                Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space2
                    TextField {
                        id: isoField; Layout.fillWidth: true; color: Theme.text
                        placeholderText: qsTr("Install media (ISO) — optional")
                        background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceAlt; border.color: Theme.border; border.width: 1 }
                    }
                    AppButton { text: qsTr("Browse…"); variant: "ghost"; onClicked: isoDialog.open() }
                }
                Text {
                    text: qsTr("A curated, downloadable OS gallery (with checksum verification) arrives in phase 2.")
                    color: Theme.textFaint; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }

            // ---- Step 2: Identity + resources ----
            ColumnLayout {
                spacing: Theme.space4
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Name & resources"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                    Item { Layout.fillWidth: true }
                    Switch { id: advSwitch; text: qsTr("Advanced"); checked: wizard.advanced; onToggled: wizard.advanced = checked }
                }
                TextField {
                    id: nameField; Layout.fillWidth: true; color: Theme.text
                    placeholderText: qsTr("VM name (e.g. ubuntu-dev)")
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceAlt; border.color: nameField.activeFocus ? Theme.accent : Theme.border; border.width: 1 }
                }
                GridLayout {
                    columns: 2; columnSpacing: Theme.space5; rowSpacing: Theme.space3; Layout.fillWidth: true
                    Text { text: qsTr("vCPUs: %1").arg(cpuSlider.value.toFixed(0)); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    Slider { id: cpuSlider; Layout.fillWidth: true; from: 1; to: 16; value: 2; stepSize: 1; snapMode: Slider.SnapAlways }
                    Text { text: qsTr("Memory: %1 MiB").arg(memSlider.value.toFixed(0)); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    Slider { id: memSlider; Layout.fillWidth: true; from: 512; to: 32768; value: 2048; stepSize: 512; snapMode: Slider.SnapAlways }
                    Text { text: qsTr("Disk: %1 GiB").arg(diskSlider.value.toFixed(0)); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    Slider { id: diskSlider; Layout.fillWidth: true; from: 5; to: 500; value: 20; stepSize: 5; snapMode: Slider.SnapAlways }
                }
                // Advanced-only options
                GridLayout {
                    visible: wizard.advanced
                    columns: 2; columnSpacing: Theme.space5; rowSpacing: Theme.space3; Layout.fillWidth: true
                    Text { text: qsTr("Firmware"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    ComboBox { id: fwCombo; Layout.fillWidth: true; model: ["bios", "uefi"] }
                    Text { text: qsTr("Network"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    ComboBox { id: netCombo; Layout.fillWidth: true; editable: true; model: ["default", "isolated", "bridge"]; }
                    Text { text: qsTr("Disk format"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    ComboBox { id: fmtCombo; Layout.fillWidth: true; model: ["qcow2", "raw"] }
                }
            }

            // ---- Step 3: Review ----
            ColumnLayout {
                spacing: Theme.space3
                Text { text: qsTr("Review"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                Card {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space2
                        Repeater {
                            model: [
                                { k: qsTr("Name"), v: nameField.text || qsTr("(unnamed)") },
                                { k: qsTr("OS"), v: wizard.chosenVariant || qsTr("Custom") },
                                { k: qsTr("Install media"), v: isoField.text || qsTr("None") },
                                { k: qsTr("vCPUs"), v: cpuSlider.value.toFixed(0) },
                                { k: qsTr("Memory"), v: memSlider.value.toFixed(0) + " MiB" },
                                { k: qsTr("Disk"), v: diskSlider.value.toFixed(0) + " GiB" },
                                { k: qsTr("Firmware"), v: wizard.advanced ? fwCombo.currentText : "bios" },
                                { k: qsTr("Network"), v: wizard.advanced ? netCombo.currentText : "default" },
                            ]
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text { text: modelData.k; color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.preferredWidth: 140 }
                                Text { text: modelData.v; color: Theme.text; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }

        // Footer
        RowLayout {
            Layout.fillWidth: true
            AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: wizard.close() }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Back"); variant: "ghost"; visible: wizard.step > 0; onClicked: wizard.step-- }
            AppButton {
                text: wizard.step === wizard.lastStep ? qsTr("Create VM") : qsTr("Next")
                variant: "primary"
                enabled: wizard.step !== 1 || nameField.text.trim().length > 0
                onClicked: {
                    if (wizard.step < wizard.lastStep) { wizard.step++; return; }
                    App.createVm(App.currentConnectionId, {
                        name: nameField.text.trim(),
                        osVariant: wizard.chosenVariant,
                        vcpus: Math.round(cpuSlider.value),
                        memoryMiB: Math.round(memSlider.value),
                        diskGiB: Math.round(diskSlider.value),
                        installMediaPath: isoField.text.trim(),
                        networkName: wizard.advanced ? netCombo.currentText : "default",
                        firmware: wizard.advanced ? fwCombo.currentText : "bios",
                        diskFormat: wizard.advanced ? fmtCombo.currentText : "qcow2",
                    });
                    wizard.close();
                }
            }
        }
    }

    FileDialog {
        id: isoDialog
        title: qsTr("Select install ISO")
        nameFilters: ["Disc images (*.iso *.img)", "All files (*)"]
        onAccepted: isoField.text = selectedFile.toString().replace("file://", "")
    }
}
