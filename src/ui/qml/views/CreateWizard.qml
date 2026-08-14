import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import VMManager

// Guided VM creation. Step 1 picks an OS family (Windows / Linux / Custom);
// step 2 refines it (Linux → distro + version, Windows → edition, Custom →
// ISO); step 3 sets name + resources; step 4 reviews and creates.
Popup {
    id: wizard
    anchors.centerIn: parent
    width: 680
    height: 620
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    background: Card {}

    property int step: 0
    property bool advanced: false
    readonly property int lastStep: 3

    // Selections
    property string osFamily: ""             // "windows" | "linux" | "custom"
    property string linuxDistro: "ubuntu"
    property string linuxVersion: "24.04 LTS"
    property string winEdition: "Windows 11"

    property var linuxDistros: [
        { id: "ubuntu",   name: "Ubuntu",       glyph: "🟠", versions: ["24.04 LTS", "23.10", "22.04 LTS"] },
        { id: "debian",   name: "Debian",       glyph: "🌀", versions: ["12 (Bookworm)", "11 (Bullseye)"] },
        { id: "fedora",   name: "Fedora",       glyph: "🎩", versions: ["40", "39"] },
        { id: "alpine",   name: "Alpine",       glyph: "🏔", versions: ["3.20", "3.19"] },
        { id: "rocky",    name: "Rocky Linux",  glyph: "⛰", versions: ["9", "8"] },
        { id: "opensuse", name: "openSUSE",     glyph: "🦎", versions: ["Leap 15.6", "Tumbleweed"] },
        { id: "arch",     name: "Arch Linux",   glyph: "🐧", versions: ["rolling"] },
    ]
    property var winEditions: ["Windows 11", "Windows 10", "Windows Server 2022", "Windows Server 2019"]

    function currentDistro() {
        for (var i = 0; i < linuxDistros.length; ++i)
            if (linuxDistros[i].id === linuxDistro) return linuxDistros[i];
        return linuxDistros[0];
    }
    // A friendly OS label + a best-effort libosinfo-ish variant id.
    function osLabel() {
        if (osFamily === "windows") return winEdition;
        if (osFamily === "linux")   return currentDistro().name + " " + linuxVersion;
        return qsTr("Custom / other");
    }
    function osVariant() {
        if (osFamily === "windows")
            return winEdition.toLowerCase().replace(/ /g, "").replace("windows", "win");
        if (osFamily === "linux")
            return linuxDistro + linuxVersion.replace(/[^0-9.]/g, "");
        return "";
    }

    function reset() {
        step = 0; osFamily = ""; nameField.text = ""; isoField.text = "";
        cpu.value = 2; mem.value = 2048; disk.value = 20; advanced = false;
        App.loadNetworks(App.currentConnectionId);
        App.loadStorage(App.currentConnectionId);
    }
    onOpened: reset()

    // Populate advanced dropdowns from the live host.
    ListModel { id: netModel }
    ListModel { id: poolModel }
    Connections {
        target: App
        function onNetworksLoaded(connId, nets) {
            netModel.clear();
            for (var i = 0; i < nets.length; ++i) netModel.append({ text: nets[i].name });
            if (nets.length === 0) netModel.append({ text: "default" });
        }
        function onStorageLoaded(connId, pools) {
            poolModel.clear();
            for (var i = 0; i < pools.length; ++i) poolModel.append({ text: pools[i].name });
            if (pools.length === 0) poolModel.append({ text: "default" });
        }
    }

    function canAdvance() {
        if (step === 0) return osFamily !== "";
        if (step === 1) return osFamily !== "custom" || isoField.text.trim() !== "";
        if (step === 2) return nameField.text.trim() !== "";
        return true;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4

        RowLayout {
            Layout.fillWidth: true
            SectionHeader { title: qsTr("Create a virtual machine")
                subtitle: qsTr("on %1").arg(App.currentConnectionId) }
            Item { Layout.fillWidth: true }
            RowLayout {
                spacing: Theme.space2
                Repeater {
                    model: 4
                    delegate: Rectangle {
                        required property int index
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

            // ================= Step 1: OS family =================
            ColumnLayout {
                spacing: Theme.space4
                Text { text: qsTr("What kind of operating system?"); color: Theme.text
                    font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space3
                    Repeater {
                        model: [
                            { id: "windows", name: qsTr("Windows"), glyph: "🪟", sub: qsTr("Windows 10/11, Server") },
                            { id: "linux",   name: qsTr("Linux"),   glyph: "🐧", sub: qsTr("Ubuntu, Fedora, Debian…") },
                            { id: "custom",  name: qsTr("Custom"),  glyph: "💿", sub: qsTr("Boot from any ISO") },
                        ]
                        delegate: Card {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 150
                            interactive: true
                            readonly property bool sel: wizard.osFamily === modelData.id
                            border.color: sel ? Theme.accent : Theme.border
                            border.width: sel ? 2 : 1
                            HoverHandler { id: fh }
                            hovered: fh.hovered
                            TapHandler { onTapped: wizard.osFamily = modelData.id }
                            ColumnLayout {
                                anchors.centerIn: parent; spacing: Theme.space2; width: parent.width - Theme.space4
                                Text { text: modelData.glyph; font.pixelSize: 40; Layout.alignment: Qt.AlignHCenter }
                                Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontLg
                                    font.weight: Font.DemiBold; Layout.alignment: Qt.AlignHCenter }
                                Text { text: modelData.sub; color: Theme.textDim; font.pixelSize: Theme.fontXs
                                    Layout.alignment: Qt.AlignHCenter; horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            // ================= Step 2: refine =================
            ColumnLayout {
                spacing: Theme.space4

                // ---- Linux: distro + version ----
                ColumnLayout {
                    visible: wizard.osFamily === "linux"
                    Layout.fillWidth: true; spacing: Theme.space3
                    Text { text: qsTr("Choose a distribution"); color: Theme.text
                        font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                    Flow {
                        Layout.fillWidth: true; spacing: Theme.space2
                        Repeater {
                            model: wizard.linuxDistros
                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool sel: wizard.linuxDistro === modelData.id
                                width: 150; height: 46; radius: Theme.radiusSm
                                color: sel ? Theme.accentSubtle : Theme.surfaceAlt
                                border.width: 1; border.color: sel ? Theme.accent : Theme.border
                                TapHandler { onTapped: { wizard.linuxDistro = modelData.id;
                                    wizard.linuxVersion = modelData.versions[0]; } }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: Theme.space2; spacing: Theme.space2
                                    Text { text: modelData.glyph; font.pixelSize: 20 }
                                    Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontSm
                                        Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: Theme.space3; Layout.topMargin: Theme.space2
                        Text { text: qsTr("Version"); color: Theme.textDim; font.pixelSize: Theme.fontMd
                            Layout.preferredWidth: 80 }
                        AppComboBox {
                            Layout.fillWidth: true
                            model: wizard.currentDistro().versions
                            Component.onCompleted: currentIndex = 0
                            onActivated: wizard.linuxVersion = currentText
                            Connections { target: wizard
                                function onLinuxDistroChanged() { /* model reset picks index 0 */ } }
                        }
                    }
                }

                // ---- Windows: edition ----
                ColumnLayout {
                    visible: wizard.osFamily === "windows"
                    Layout.fillWidth: true; spacing: Theme.space3
                    Text { text: qsTr("Choose a Windows edition"); color: Theme.text
                        font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                    Repeater {
                        model: wizard.winEditions
                        delegate: Rectangle {
                            required property string modelData
                            Layout.fillWidth: true; height: 44; radius: Theme.radiusSm
                            readonly property bool sel: wizard.winEdition === modelData
                            color: sel ? Theme.accentSubtle : Theme.surfaceAlt
                            border.width: 1; border.color: sel ? Theme.accent : Theme.border
                            TapHandler { onTapped: wizard.winEdition = modelData }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            RowLayout {
                                anchors.fill: parent; anchors.margins: Theme.space3; spacing: Theme.space3
                                Text { text: "🪟"; font.pixelSize: 18 }
                                Text { text: modelData; color: Theme.text; font.pixelSize: Theme.fontMd; Layout.fillWidth: true }
                                Text { text: sel ? "✓" : ""; color: Theme.accent; font.pixelSize: Theme.fontMd }
                            }
                        }
                    }
                }

                // ---- Install media (all families; required for custom) ----
                ColumnLayout {
                    Layout.fillWidth: true; spacing: Theme.space2; Layout.topMargin: Theme.space2
                    Text {
                        text: wizard.osFamily === "custom" ? qsTr("Install media (ISO)")
                                                           : qsTr("Install media (ISO) — optional")
                        color: Theme.textDim; font.pixelSize: Theme.fontSm
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: Theme.space2
                        AppTextField { id: isoField; Layout.fillWidth: true
                            placeholderText: qsTr("/path/to/installer.iso") }
                        AppButton { text: qsTr("Browse…"); variant: "ghost"; onClicked: isoDialog.open() }
                    }
                    Text {
                        visible: wizard.osFamily === "linux"
                        text: qsTr("Leave blank to attach a downloadable image later (OS gallery, phase 2).")
                        color: Theme.textFaint; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
            }

            // ================= Step 3: name + resources =================
            ColumnLayout {
                spacing: Theme.space4
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Name & resources"); color: Theme.text
                        font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                    Item { Layout.fillWidth: true }
                    AppSwitch { text: qsTr("Advanced"); checked: wizard.advanced; onToggled: wizard.advanced = checked }
                }
                AppTextField { id: nameField; Layout.fillWidth: true
                    placeholderText: qsTr("VM name (e.g. ubuntu-dev)") }

                LabeledSlider { id: cpu;  label: qsTr("vCPUs");  from: 1; to: 32; stepSize: 1; value: 2; unit: "cores" }
                LabeledSlider { id: mem;  label: qsTr("Memory"); from: 256; to: 131072; stepSize: 256; value: 2048
                    unitOptions: [{name: "MiB", factor: 1}, {name: "GiB", factor: 1024}]; unitIndex: 1 }
                LabeledSlider { id: disk; label: qsTr("Disk");   from: 1; to: 4096; stepSize: 1; value: 20
                    unitOptions: [{name: "GiB", factor: 1}, {name: "TiB", factor: 1024}] }

                GridLayout {
                    visible: wizard.advanced
                    columns: 2; columnSpacing: Theme.space5; rowSpacing: Theme.space3; Layout.fillWidth: true
                    Text { text: qsTr("Firmware"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    AppComboBox { id: fwCombo; Layout.fillWidth: true; model: ["bios", "uefi"] }
                    Text { text: qsTr("Network"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    AppComboBox { id: netCombo; Layout.fillWidth: true; textRole: "text"; model: netModel }
                    Text { text: qsTr("Storage pool"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    AppComboBox { id: poolCombo; Layout.fillWidth: true; textRole: "text"; model: poolModel }
                    Text { text: qsTr("Disk format"); color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    AppComboBox { id: fmtCombo; Layout.fillWidth: true; model: ["qcow2", "raw"] }
                }
            }

            // ================= Step 4: review =================
            ColumnLayout {
                spacing: Theme.space3
                Text { text: qsTr("Review"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                Card {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: Theme.space4; spacing: Theme.space2
                        Repeater {
                            model: [
                                { k: qsTr("Name"),    v: nameField.text || qsTr("(unnamed)") },
                                { k: qsTr("OS"),      v: wizard.osLabel() },
                                { k: qsTr("Install media"), v: isoField.text || qsTr("None") },
                                { k: qsTr("vCPUs"),   v: Math.round(cpu.value).toString() },
                                { k: qsTr("Memory"),  v: Math.round(mem.value) + " MiB" },
                                { k: qsTr("Disk"),    v: Math.round(disk.value) + " GiB" },
                                { k: qsTr("Firmware"),v: wizard.advanced ? fwCombo.currentText : "bios" },
                                { k: qsTr("Network"), v: wizard.advanced ? netCombo.currentText : "default" },
                                { k: qsTr("Pool"),    v: wizard.advanced ? poolCombo.currentText : "default" },
                            ]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text { text: modelData.k; color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.preferredWidth: 150 }
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
                enabled: wizard.canAdvance()
                onClicked: {
                    if (wizard.step < wizard.lastStep) { wizard.step++; return; }
                    App.createVm(App.currentConnectionId, {
                        name: nameField.text.trim(),
                        osVariant: wizard.osVariant(),
                        vcpus: Math.round(cpu.value),
                        memoryMiB: Math.round(mem.value),
                        diskGiB: Math.round(disk.value),
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
