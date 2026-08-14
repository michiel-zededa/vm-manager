import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import VMManager

// Add-a-host dialog with friendly, separate fields (host / user / port /
// key / password) that assemble a libvirt connection URI. Local sockets and
// remote qemu+ssh:// are both supported.
Dialog {
    id: dlg
    anchors.centerIn: Overlay.overlay
    modal: true
    width: 520
    padding: Theme.space5
    background: Card {}

    property int mode: 1                  // 0 = local, 1 = remote (ssh)
    property string localScope: "session" // session | system

    // Password auth requires the libssh2 transport (OpenSSH can't take a
    // password without a TTY); keys/agent use the standard ssh transport.
    function usePassword() {
        return mode === 1 && pwField.text.trim().length > 0 && keyField.text.trim().length === 0;
    }
    function buildUri() {
        if (mode === 0)
            return "qemu:///" + localScope;
        var transport = usePassword() ? "libssh2" : "ssh";
        var base = App.buildConnectionUri(transport, hostField.text.trim(),
                                          userField.text.trim(),
                                          parseInt(portField.text) || 0,
                                          pathField.text.trim());
        var q = [];
        if (usePassword()) {
            q.push("sshauth=password");
            if (noVerify.checked) q.push("known_hosts_verify=ignore");
        } else {
            if (keyField.text.trim().length > 0)
                q.push("keyfile=" + encodeURIComponent(keyField.text.trim()));
            if (noVerify.checked) q.push("no_verify=1");
        }
        return q.length ? base + "?" + q.join("&") : base;
    }

    onOpened: {
        hostField.text = ""; userField.text = ""; portField.text = "";
        keyField.text = ""; pwField.text = ""; nameField.text = "";
        pathField.text = "system"; mode = 1; localScope = "session";
    }

    contentItem: ColumnLayout {
        spacing: Theme.space4

        Text { text: qsTr("Connect a host"); color: Theme.text
            font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }

        // Local vs remote segmented control
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.space1
            Repeater {
                model: [{ m: 0, t: qsTr("This machine") }, { m: 1, t: qsTr("Remote (SSH)") }]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true; height: 34; radius: Theme.radiusSm
                    color: dlg.mode === modelData.m ? Theme.accentSubtle : Theme.surfaceAlt
                    border.width: 1; border.color: dlg.mode === modelData.m ? Theme.accent : Theme.border
                    Text { anchors.centerIn: parent; text: modelData.t
                        color: dlg.mode === modelData.m ? Theme.accent : Theme.textDim; font.pixelSize: Theme.fontSm }
                    TapHandler { onTapped: dlg.mode = modelData.m }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        // ---- Local options ----
        RowLayout {
            visible: dlg.mode === 0
            Layout.fillWidth: true; spacing: Theme.space3
            Text { text: qsTr("Scope"); color: Theme.textDim; font.pixelSize: Theme.fontMd; Layout.preferredWidth: 90 }
            AppComboBox {
                Layout.fillWidth: true
                model: [qsTr("User session (qemu:///session)"), qsTr("System (qemu:///system)")]
                onActivated: dlg.localScope = currentIndex === 1 ? "system" : "session"
            }
        }

        // ---- Remote options ----
        GridLayout {
            visible: dlg.mode === 1
            columns: 2; columnSpacing: Theme.space3; rowSpacing: Theme.space3; Layout.fillWidth: true

            Text { text: qsTr("Host"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            AppTextField { id: hostField; Layout.fillWidth: true; placeholderText: qsTr("hostname or IP") }

            Text { text: qsTr("Username"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            AppTextField { id: userField; Layout.fillWidth: true; placeholderText: qsTr("e.g. root") }

            Text { text: qsTr("Port"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            AppTextField { id: portField; Layout.fillWidth: true; placeholderText: qsTr("22 (default)")
                inputMethodHints: Qt.ImhDigitsOnly }

            Text { text: qsTr("libvirt path"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            AppTextField { id: pathField; Layout.fillWidth: true; text: "system"
                placeholderText: qsTr("system") }

            Text { text: qsTr("SSH key"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.space2
                AppTextField { id: keyField; Layout.fillWidth: true
                    placeholderText: qsTr("~/.ssh/id_ed25519 (optional, uses agent if blank)") }
                AppButton { text: qsTr("Browse…"); variant: "ghost"; onClicked: keyPicker.open() }
            }

            Text { text: qsTr("Password"); color: Theme.textDim; font.pixelSize: Theme.fontMd }
            AppTextField { id: pwField; Layout.fillWidth: true; echoMode: TextInput.Password
                placeholderText: qsTr("only if not using a key") }

            Item {}
            AppCheckBox { id: noVerify; text: qsTr("Skip host key verification") }
        }

        // First-connection host-key hint (the usual cause of "Host key
        // verification failed", whether using a key or a password).
        Text {
            visible: dlg.mode === 1 && !noVerify.checked
            text: qsTr("First time connecting to a host? Tick “Skip host key verification” — "
                     + "otherwise the connection fails with “Host key verification failed” until the "
                     + "host is in ~/.ssh/known_hosts.")
            color: Theme.warning; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        // Password storage note (honest about scope)
        Text {
            visible: dlg.mode === 1 && pwField.text.length > 0
            text: qsTr("Passwords are held only for this session and stored in the OS keychain — "
                     + "SSH keys or an agent are recommended for unattended reconnects.")
            color: Theme.textFaint; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }

        AppTextField { id: nameField; Layout.fillWidth: true; placeholderText: qsTr("Display name (optional)") }

        // Live URI preview
        Rectangle {
            Layout.fillWidth: true; radius: Theme.radiusSm; color: Theme.surfaceAlt
            implicitHeight: uriPreview.implicitHeight + Theme.space3
            Text { id: uriPreview; anchors.fill: parent; anchors.margins: Theme.space2
                text: dlg.buildUri(); color: Theme.textDim; font.family: Theme.monoFamily
                font.pixelSize: Theme.fontXs; wrapMode: Text.WrapAnywhere; verticalAlignment: Text.AlignVCenter }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: dlg.close() }
            AppButton {
                text: qsTr("Connect"); variant: "primary"
                enabled: dlg.mode === 0 || hostField.text.trim().length > 0
                onClicked: {
                    var uri = dlg.buildUri();
                    App.addConnection(uri, nameField.text.trim() || uri,
                                      userField.text.trim(),
                                      dlg.usePassword() ? pwField.text : "");
                    dlg.close();
                }
            }
        }
    }

    FileDialog {
        id: keyPicker
        title: qsTr("Select SSH private key")
        onAccepted: keyField.text = selectedFile.toString().replace("file://", "")
    }
}
