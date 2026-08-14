import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// The home screen: a gallery of every VM on the selected host (or all hosts),
// with a summary header and live search.
Item {
    id: root
    signal openVm(var vm)

    property string query: ""
    property string filterHost: ""   // "" = all hosts, else a connectionId

    function matches(name, os, title, connId) {
        if (filterHost.length > 0 && connId !== filterHost) return false;
        if (query.length === 0) return true;
        const q = query.toLowerCase();
        return (name || "").toLowerCase().indexOf(q) >= 0
            || (os || "").toLowerCase().indexOf(q) >= 0
            || (title || "").toLowerCase().indexOf(q) >= 0;
    }
    function hostLabel() {
        if (filterHost.length === 0) return qsTr("All hosts");
        const n = App.connections.displayNameFor(filterHost);
        return n.length > 0 ? n : qsTr("This host");
    }

    // Selecting a connection in the sidebar filters the gallery to that host.
    Connections { target: App; function onCurrentConnectionChanged() { root.filterHost = App.currentConnectionId; } }
    Component.onCompleted: filterHost = App.currentConnectionId

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space5

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space4
            SectionHeader {
                title: qsTr("Virtual machines")
                subtitle: qsTr("Manage every VM across your connected hosts")
            }
            Item { Layout.fillWidth: true }
            SearchField {
                Layout.preferredWidth: 260
                onTextChanged: root.query = text
            }
            // Host filter dropdown: All hosts + each connected host.
            AppButton {
                id: hostBtn
                // reference connections.count so the label refreshes as hosts connect
                text: (App.connections.count, root.hostLabel()) + "  ▾"
                variant: "ghost"
                onClicked: hostMenu.open()
                Popup {
                    id: hostMenu
                    y: hostBtn.height + 4
                    x: hostBtn.width - width
                    width: 240
                    padding: Theme.space1
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surface
                        border.width: 1; border.color: Theme.border }
                    contentItem: ColumnLayout {
                        spacing: 0
                        // "All hosts" row
                        Rectangle {
                            Layout.fillWidth: true; height: 34; radius: Theme.radiusSm
                            color: allHover.hovered ? Theme.surfaceAlt : "transparent"
                            RowLayout { anchors.fill: parent; anchors.leftMargin: Theme.space3; anchors.rightMargin: Theme.space3
                                Text { text: qsTr("All hosts"); color: Theme.text; font.pixelSize: Theme.fontMd; Layout.fillWidth: true }
                                Text { text: root.filterHost === "" ? "✓" : ""; color: Theme.accent } }
                            HoverHandler { id: allHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: { root.filterHost = ""; hostMenu.close(); } }
                        }
                        Repeater {
                            model: App.connections
                            delegate: Rectangle {
                                required property string id
                                required property string displayName
                                required property bool connected
                                Layout.fillWidth: true; height: 34; radius: Theme.radiusSm
                                color: hostHover.hovered ? Theme.surfaceAlt : "transparent"
                                RowLayout { anchors.fill: parent; anchors.leftMargin: Theme.space3; anchors.rightMargin: Theme.space3; spacing: Theme.space2
                                    Rectangle { width: 7; height: 7; radius: 4; color: connected ? Theme.running : Theme.stopped }
                                    Text { text: displayName; color: Theme.text; font.pixelSize: Theme.fontMd; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: root.filterHost === id ? "✓" : ""; color: Theme.accent } }
                                HoverHandler { id: hostHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: { root.filterHost = id; App.currentConnectionId = id; hostMenu.close(); } }
                            }
                        }
                    }
                }
            }
        }

        // ---- Summary tiles ----
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space5
            Card {
                Layout.preferredWidth: 160; implicitHeight: 74
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: qsTr("Total VMs"); value: App.vms.count }
                }
            }
            Card {
                Layout.preferredWidth: 160; implicitHeight: 74
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: qsTr("Running"); value: App.vms.runningCount; valueColor: Theme.running }
                }
            }
            Card {
                Layout.preferredWidth: 160; implicitHeight: 74
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: qsTr("Hosts"); value: App.connections.count }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ---- Gallery ----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            Flow {
                width: parent.width
                spacing: Theme.space4

                Repeater {
                    model: App.vms
                    delegate: VmCard {
                        width: 300
                        visible: root.matches(name, osLabel, title, connectionId)
                        onOpen: root.openVm({
                            uuid: uuid, connectionId: connectionId, name: name,
                            osLabel: osLabel, title: title, state: state,
                            vcpus: vcpus, memoryMax: memoryMax, autostart: autostart,
                            isTemplate: isTemplate
                        })
                    }
                }
            }
        }
    }

    // Empty state when there are genuinely no VMs.
    EmptyState {
        anchors.centerIn: parent
        visible: App.vms.count === 0
        glyph: "🖥"
        title: qsTr("No virtual machines yet")
        body: qsTr("Create your first VM, or import an existing disk image from VMware, Hyper-V or VirtualBox.")
        action: AppButton { text: qsTr("＋ Create VM"); variant: "primary" }
    }
}
