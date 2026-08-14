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
    property bool allHosts: false          // dashboard scope: all hosts vs current
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
                    Image {
                        width: 32; height: 32
                        sourceSize.width: 64; sourceSize.height: 64
                        source: "qrc:/icons/app.png"
                        smooth: true
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
                // "All hosts" aggregate scope (dashboard shows every host's VMs).
                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    radius: Theme.radiusSm
                    visible: App.connections.count > 1
                    color: win.allHosts && win.currentPage === "dashboard" ? Theme.accentSubtle
                           : allHover.hovered ? Theme.surfaceAlt : "transparent"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: Theme.space3; anchors.rightMargin: Theme.space3
                        spacing: Theme.space3
                        Text { text: "▦"; color: Theme.textDim; font.pixelSize: Theme.fontSm }
                        Text { text: qsTr("All hosts"); color: Theme.text; font.pixelSize: Theme.fontSm; Layout.fillWidth: true }
                        Text { text: App.vms.count; color: Theme.textDim; font.pixelSize: Theme.fontXs }
                    }
                    HoverHandler { id: allHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { win.allHosts = true; win.navigate("dashboard"); } }
                }
                ListView {
                    id: hostList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.space1
                    model: App.connections
                    delegate: ItemDelegate {
                        id: hostDel
                        required property string id
                        required property string displayName
                        required property string uri
                        required property bool connected
                        required property bool isLocal
                        required property int activeVms
                        required property int totalVms
                        required property string lastError
                        width: hostList.width
                        height: 48
                        hoverEnabled: true
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        onClicked: { win.allHosts = false; App.currentConnectionId = hostDel.id }
                        ToolTip.visible: hostDel.lastError.length > 0 && hovered
                        ToolTip.text: hostDel.lastError
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: (!win.allHosts && App.currentConnectionId === hostDel.id) ? Theme.accentSubtle
                                   : parent.hovered ? Theme.surfaceAlt : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: Theme.space3
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: hostDel.connected ? Theme.running
                                       : hostDel.lastError.length > 0 ? Theme.danger : Theme.stopped
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Text {
                                    text: hostDel.displayName; color: Theme.text
                                    font.pixelSize: Theme.fontSm; elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: hostDel.isLocal ? qsTr("local") : hostDel.uri
                                    color: Theme.textFaint; font.pixelSize: Theme.fontXs
                                    elide: Text.ElideMiddle; Layout.fillWidth: true
                                }
                            }
                            Text {
                                visible: !hostDel.hovered
                                text: hostDel.activeVms + "/" + hostDel.totalVms
                                color: Theme.textDim; font.pixelSize: Theme.fontXs
                            }
                            IconButton {
                                visible: hostDel.hovered
                                implicitWidth: 26; implicitHeight: 26
                                glyph: "⋯"; tip: qsTr("Manage")
                                onClicked: hostMenu.open()
                                Menu {
                                    id: hostMenu
                                    background: Rectangle { implicitWidth: 180; radius: Theme.radiusSm
                                        color: Theme.surface; border.width: 1; border.color: Theme.border }
                                    delegate: MenuItem {
                                        id: hmi
                                        contentItem: Text { text: hmi.text; color: hmi.text === qsTr("Remove") ? Theme.danger : Theme.text
                                            font.pixelSize: Theme.fontMd; verticalAlignment: Text.AlignVCenter; leftPadding: Theme.space2 }
                                        background: Rectangle { color: hmi.highlighted ? Theme.accentSubtle : "transparent"; radius: Theme.radiusSm }
                                    }
                                    MenuItem { text: hostDel.connected ? qsTr("Disconnect") : qsTr("Connect")
                                        onTriggered: hostDel.connected ? App.disconnectConnection(hostDel.id)
                                                                       : App.connectConnection(hostDel.id) }
                                    MenuItem { text: qsTr("Reconnect"); onTriggered: { App.disconnectConnection(hostDel.id); App.connectConnection(hostDel.id); } }
                                    MenuSeparator {}
                                    MenuItem { text: qsTr("Remove"); onTriggered: App.removeConnection(hostDel.id) }
                                }
                            }
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
                        text: App.demoMode ? qsTr("● Demo mode (sample data)")
                                           : qsTr("● libvirt unavailable — check tools")
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

    Component { id: dashboardComp; DashboardView { allHosts: win.allHosts; onOpenVm: (vm) => win.openDetail(vm) } }
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
