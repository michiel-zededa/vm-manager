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
    property bool allHosts: true

    function matches(name, os, title, connId) {
        if (!allHosts && connId !== App.currentConnectionId) return false;
        if (query.length === 0) return true;
        const q = query.toLowerCase();
        return (name || "").toLowerCase().indexOf(q) >= 0
            || (os || "").toLowerCase().indexOf(q) >= 0
            || (title || "").toLowerCase().indexOf(q) >= 0;
    }

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
            AppButton {
                text: root.allHosts ? qsTr("All hosts") : qsTr("This host")
                variant: "ghost"
                onClicked: root.allHosts = !root.allHosts
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
