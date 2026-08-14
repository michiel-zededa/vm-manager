import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("VM Manager")
    color: Theme.bg

    // ---- App-wide navigation state ----------------------------------------
    property string currentPage: "dashboard"
    property var selectedVm: null          // captured row map when opening detail

    function openDetail(vm) { selectedVm = vm; currentPage = "detail"; }
    function goto(page) { currentPage = page; }

    // ---- Toast notifications ----------------------------------------------
    Connections {
        target: App
        function onNotify(level, title, message) { toast.show(level, title, message); }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ===== Sidebar =====================================================
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 264
            color: Theme.sidebar
            border.width: 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space4
                spacing: Theme.space4

                // Brand
                RowLayout {
                    spacing: Theme.space3
                    Rectangle {
                        width: 32; height: 32; radius: Theme.radiusSm
                        gradient: Gradient {
                            GradientStop { position: 0; color: Theme.accent }
                            GradientStop { position: 1; color: "#8B5CF6" }
                        }
                        Text { anchors.centerIn: parent; text: "▣"; color: "white"; font.pixelSize: 18 }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text { text: qsTr("VM Manager"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
                        Text { text: "v" + App.appVersion; color: Theme.textFaint; font.pixelSize: Theme.fontXs }
                    }
                }

                // Primary actions
                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("＋  Create VM")
                    variant: "primary"
                    onClicked: createWizard.open()
                }
                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("⤓  Import image")
                    variant: "ghost"
                    onClicked: importDialog.open()
                }

                // Navigation
                Text {
                    text: qsTr("MANAGE"); color: Theme.textFaint
                    font.pixelSize: Theme.fontXs; font.letterSpacing: 1
                    Layout.topMargin: Theme.space2
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Dashboard"); glyph: "▦"
                    badgeCount: App.vms.count
                    selected: win.currentPage === "dashboard" || win.currentPage === "detail"
                    onClicked: win.goto("dashboard")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Storage"); glyph: "▤"
                    selected: win.currentPage === "storage"
                    onClicked: win.goto("storage")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Networks"); glyph: "⇄"
                    selected: win.currentPage === "networks"
                    onClicked: win.goto("networks")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Snapshots"); glyph: "◷"
                    selected: win.currentPage === "snapshots"
                    onClicked: win.goto("snapshots")
                }

                // Hosts
                Text {
                    text: qsTr("CONNECTIONS"); color: Theme.textFaint
                    font.pixelSize: Theme.fontXs; font.letterSpacing: 1
                    Layout.topMargin: Theme.space3
                }
                ListView {
                    id: hostList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.space1
                    model: App.connections
                    delegate: ItemDelegate {
                        width: hostList.width
                        height: 48
                        hoverEnabled: true
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        onClicked: App.currentConnectionId = id
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: App.currentConnectionId === id ? Theme.accentSubtle
                                   : parent.hovered ? Theme.surfaceAlt : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: Theme.space3
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: connected ? Theme.running : Theme.stopped
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Text {
                                    text: displayName; color: Theme.text
                                    font.pixelSize: Theme.fontSm; elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: isLocal ? qsTr("local") : uri
                                    color: Theme.textFaint; font.pixelSize: Theme.fontXs
                                    elide: Text.ElideMiddle; Layout.fillWidth: true
                                }
                            }
                            Text { text: activeVms + "/" + totalVms; color: Theme.textDim; font.pixelSize: Theme.fontXs }
                        }
                    }
                }

                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("＋ Add connection")
                    variant: "subtle"
                    onClicked: connectDialog.open()
                }

                // Backend indicator
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: Theme.radiusSm
                    color: App.usingMockBackend ? Theme.accentSubtle : "transparent"
                    visible: App.usingMockBackend
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("● Demo mode (mock backend)")
                        color: Theme.accent; font.pixelSize: Theme.fontXs
                    }
                }
            }
        }

        // Divider
        Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }

        // ===== Content =====================================================
        Loader {
            id: pageLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: {
                switch (win.currentPage) {
                case "detail":    return detailComp;
                case "storage":   return storageComp;
                case "networks":  return networksComp;
                case "snapshots": return snapshotsComp;
                default:          return dashboardComp;
                }
            }
        }
    }

    Component { id: dashboardComp; DashboardView { onOpenVm: (vm) => win.openDetail(vm) } }
    Component { id: detailComp;    VmDetailView { vm: win.selectedVm; onBack: win.goto("dashboard") } }
    Component { id: storageComp;   StorageView {} }
    Component { id: networksComp;  NetworkView {} }
    Component { id: snapshotsComp; SnapshotsView {} }

    // ===== Dialogs =========================================================
    CreateWizard { id: createWizard }
    ImportView { id: importDialog }

    // Add-connection dialog
    Dialog {
        id: connectDialog
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.space5
        background: Card { }

        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text { text: qsTr("Connect a host"); color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold }
            Text {
                text: qsTr("Local: qemu:///system · Remote: qemu+ssh://user@host/system")
                color: Theme.textDim; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
            TextField {
                id: uriField
                Layout.fillWidth: true
                placeholderText: "qemu+ssh://user@host/system"
                color: Theme.text
                background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceAlt; border.color: Theme.border; border.width: 1 }
            }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Display name (optional)")
                color: Theme.text
                background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceAlt; border.color: Theme.border; border.width: 1 }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: connectDialog.close() }
                AppButton {
                    text: qsTr("Connect"); variant: "primary"
                    enabled: uriField.text.trim().length > 0
                    onClicked: {
                        App.addConnection(uriField.text.trim(),
                                          nameField.text.trim() || uriField.text.trim());
                        uriField.clear(); nameField.clear(); connectDialog.close();
                    }
                }
            }
        }
    }

    Toast { id: toast; anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: Theme.space4 }
}
