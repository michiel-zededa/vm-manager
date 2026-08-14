import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// Home screen: a gallery of VMs for the host selected in the sidebar (or all
// hosts), a host-resource summary, and live search. The sidebar is the single
// place to pick/manage hosts; "" as the current connection means "all hosts".
Item {
    id: root
    signal openVm(var vm)

    property string query: ""
    property bool allHosts: false
    readonly property string scope: allHosts ? "" : App.currentConnectionId  // "" = all hosts
    property var host: ({})

    function refreshHost() { host = scope.length ? App.connections.hostMap(scope) : ({}) }
    Connections { target: App; function onCurrentConnectionChanged() { root.refreshHost() } }
    Connections { target: App.connections; function onDataChanged() { root.refreshHost() }
                  function onCountChanged() { root.refreshHost() } }
    Component.onCompleted: refreshHost()

    function matches(name, os, title, connId) {
        if (scope.length > 0 && connId !== scope) return false;
        if (query.length === 0) return true;
        const q = query.toLowerCase();
        return (name || "").toLowerCase().indexOf(q) >= 0
            || (os || "").toLowerCase().indexOf(q) >= 0
            || (title || "").toLowerCase().indexOf(q) >= 0;
    }
    function fmtGiB(kib) {
        const g = kib / 1048576;
        return (g >= 100 ? g.toFixed(0) : g.toFixed(1)) + " GiB";
    }
    function fmtBytesG(b) {
        const g = b / 1073741824;
        return (g >= 100 ? g.toFixed(0) : g.toFixed(1)) + " GiB";
    }
    function fmtRate(bps) {
        if (bps >= 1048576) return (bps / 1048576).toFixed(1) + " MB/s";
        if (bps >= 1024) return (bps / 1024).toFixed(0) + " KB/s";
        return Math.round(bps) + " B/s";
    }

    // Live host network throughput (sum of VM rx+tx), sampled for the graph.
    property real netBps: 0
    property real netMax: 1
    Timer {
        interval: 2000; repeat: true
        running: root.scope.length > 0 && root.host.connected === true
        onTriggered: {
            root.netBps = App.vms.totalNetBps(root.scope);
            root.netMax = Math.max(root.netMax * 0.9, root.netBps, 65536);
        }
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
                subtitle: root.scope.length ? qsTr("on %1").arg(App.connections.displayNameFor(root.scope))
                                            : qsTr("across all connected hosts")
            }
            Item { Layout.fillWidth: true }
            SearchField {
                Layout.preferredWidth: 280
                onTextChanged: root.query = text
            }
        }

        // ---- Host resource summary ----
        // A specific host shows its live resources; "all hosts" shows aggregates.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space4

            // Host resource card (only when a single host is selected)
            Card {
                visible: root.scope.length > 0 && root.host.connected === true
                Layout.fillWidth: true
                implicitHeight: 130
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space4
                    spacing: Theme.space3
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        Rectangle { width: 8; height: 8; radius: 4; color: Theme.running }
                        Text { text: root.host.displayName || ""; color: Theme.text
                            font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                        Text { text: (root.host.hypervisor || "") + (root.host.hostArch ? " · " + root.host.hostArch : "")
                            color: Theme.textDim; font.pixelSize: Theme.fontXs }
                        Item { Layout.fillWidth: true }
                        Text { text: (root.host.activeVms || 0) + "/" + (root.host.totalVms || 0) + " " + qsTr("running")
                            color: Theme.textDim; font.pixelSize: Theme.fontSm }
                    }

                    // CPU · Memory · Disk · Network
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.space5

                        // -- CPU --
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.space1
                            RowLayout { Layout.fillWidth: true
                                Text { text: qsTr("CPU"); color: Theme.textDim; font.pixelSize: Theme.fontXs; Layout.fillWidth: true }
                                Text { text: Math.round(root.host.hostCpuPercent || 0) + "%  ·  " + (root.host.hostCpus || 0) + " cores"
                                    color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                            Rectangle { Layout.fillWidth: true; height: 8; radius: 4; color: Theme.surfaceAlt
                                property real frac: Math.min(1, (root.host.hostCpuPercent || 0) / 100)
                                Rectangle { height: parent.height; radius: 4; width: parent.width * parent.frac
                                    color: parent.frac > 0.85 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.durMed } } } }
                        }
                        // -- Memory --
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.space1
                            RowLayout { Layout.fillWidth: true
                                Text { text: qsTr("Memory"); color: Theme.textDim; font.pixelSize: Theme.fontXs; Layout.fillWidth: true }
                                Text { text: root.fmtGiB(root.host.hostMemUsedKiB || 0) + " / " + root.fmtGiB(root.host.hostMemoryKiB || 0)
                                    color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                            Rectangle { Layout.fillWidth: true; height: 8; radius: 4; color: Theme.surfaceAlt
                                property real frac: (root.host.hostMemoryKiB > 0) ? Math.min(1, root.host.hostMemUsedKiB / root.host.hostMemoryKiB) : 0
                                Rectangle { height: parent.height; radius: 4; width: parent.width * parent.frac
                                    color: parent.frac > 0.85 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.durMed } } } }
                        }
                        // -- Disk (pools) --
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.space1
                            RowLayout { Layout.fillWidth: true
                                Text { text: qsTr("Disk"); color: Theme.textDim; font.pixelSize: Theme.fontXs; Layout.fillWidth: true }
                                Text { text: root.fmtBytesG(root.host.storageAllocationBytes || 0) + " / " + root.fmtBytesG(root.host.storageCapacityBytes || 0)
                                    color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                            Rectangle { Layout.fillWidth: true; height: 8; radius: 4; color: Theme.surfaceAlt
                                property real frac: (root.host.storageCapacityBytes > 0) ? Math.min(1, root.host.storageAllocationBytes / root.host.storageCapacityBytes) : 0
                                Rectangle { height: parent.height; radius: 4; width: parent.width * parent.frac
                                    color: parent.frac > 0.85 ? Theme.danger : Theme.accent
                                    Behavior on width { NumberAnimation { duration: Theme.durMed } } } }
                        }
                        // -- Network graph --
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.space1
                            RowLayout { Layout.fillWidth: true
                                Text { text: qsTr("Network"); color: Theme.textDim; font.pixelSize: Theme.fontXs; Layout.fillWidth: true }
                                Text { text: root.fmtRate(root.netBps); color: Theme.textDim; font.pixelSize: Theme.fontXs } }
                            Sparkline {
                                Layout.fillWidth: true; Layout.preferredHeight: 26
                                value: root.netBps; maxValue: root.netMax
                                lineColor: Theme.info
                                fillColor: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.18)
                            }
                        }
                    }
                }
            }

            // Aggregate tiles (always useful; primary view for "all hosts")
            Card {
                Layout.preferredWidth: 150; implicitHeight: 96
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: root.scope.length ? qsTr("VMs here") : qsTr("Total VMs")
                        value: root.vmCount() } }
            }
            Card {
                Layout.preferredWidth: 150; implicitHeight: 96
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: qsTr("Running"); value: App.vms.runningCount; valueColor: Theme.running } }
            }
            Card {
                visible: root.scope.length === 0
                Layout.preferredWidth: 150; implicitHeight: 96
                RowLayout { anchors.fill: parent; anchors.margins: Theme.space4
                    StatTile { label: qsTr("Hosts"); value: App.connections.count } }
            }
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

    // Count of VMs in the current scope (all, or a single host).
    function vmCount() {
        if (scope.length === 0) return App.vms.count;
        var n = 0;
        for (var i = 0; i < App.vms.count; ++i)
            if (App.vms.data(App.vms.index(i, 0), Qt.UserRole + 2) === scope) n++;
        return n;
    }

    EmptyState {
        anchors.centerIn: parent
        visible: App.vms.count === 0
        glyph: "🖥"
        title: qsTr("No virtual machines yet")
        body: qsTr("Create your first VM, or import an existing disk image from VMware, Hyper-V or VirtualBox.")
        action: AppButton { text: qsTr("＋ Create VM"); variant: "primary" }
    }
}
