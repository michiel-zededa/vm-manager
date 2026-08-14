import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtCore
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
    function navigate(page) { currentPage = page; }

    // ---- Persisted preferences (theme) ------------------------------------
    Settings {
        id: prefs
        property int themeMode: 0           // 0 system, 1 light, 2 dark
    }
    Component.onCompleted: {
        Theme.mode = prefs.themeMode;
        // First-run: if we're on the real backend but tooling is missing, help.
        const dep = App.dependencyStatus();
        if (!dep.usingMock && (!dep.qemu || !dep.libvirt))
            depDialog.open();
    }
    function setTheme(mode) { Theme.mode = mode; prefs.themeMode = mode; }

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
                    onClicked: win.navigate("dashboard")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Storage"); glyph: "▤"
                    selected: win.currentPage === "storage"
                    onClicked: win.navigate("storage")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Networks"); glyph: "⇄"
                    selected: win.currentPage === "networks"
                    onClicked: win.navigate("networks")
                }
                NavItem {
                    Layout.fillWidth: true; text: qsTr("Snapshots"); glyph: "◷"
                    selected: win.currentPage === "snapshots"
                    onClicked: win.navigate("snapshots")
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

                // Theme switcher — System / Light / Dark
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    Repeater {
                        model: [{ m: 0, t: qsTr("Auto") }, { m: 1, t: qsTr("Light") }, { m: 2, t: qsTr("Dark") }]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            height: 26
                            radius: Theme.radiusSm
                            color: Theme.mode === modelData.m ? Theme.accentSubtle
                                   : hover.hovered ? Theme.surfaceAlt : "transparent"
                            border.width: 1
                            border.color: Theme.mode === modelData.m ? Theme.accentBorder : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.t
                                color: Theme.mode === modelData.m ? Theme.accent : Theme.textDim
                                font.pixelSize: Theme.fontXs
                            }
                            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: win.setTheme(modelData.m) }
                        }
                    }
                }

                // Backend indicator — click to see why (dependency check).
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: Theme.radiusSm
                    color: App.usingMockBackend ? Theme.accentSubtle : "transparent"
                    visible: App.usingMockBackend
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("● Demo mode — check tools")
                        color: Theme.accent; font.pixelSize: Theme.fontXs
                    }
                    TapHandler { onTapped: depDialog.open() }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
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
    Component { id: detailComp;    VmDetailView { vm: win.selectedVm; onBack: win.navigate("dashboard") } }
    Component { id: storageComp;   StorageView {} }
    Component { id: networksComp;  NetworkView {} }
    Component { id: snapshotsComp; SnapshotsView {} }

    // ===== Dialogs =========================================================
    CreateWizard { id: createWizard }
    ImportView { id: importDialog }
    ConnectionDialog { id: connectDialog }
    DependencyDialog { id: depDialog }

    Toast { id: toast; anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: Theme.space4 }
}
