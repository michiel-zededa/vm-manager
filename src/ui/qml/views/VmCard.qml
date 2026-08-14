import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// One VM in the dashboard gallery. Shows identity, live state, a CPU sparkline
// and inline lifecycle actions. Emits open() when the body is clicked.
Card {
    id: vmCard
    implicitHeight: 168
    interactive: true
    signal open()

    property bool isRunning: state === 1
    property bool isPaused: state === 2

    function osGlyph(os) {
        const s = (os || "").toLowerCase();
        if (s.indexOf("win") >= 0) return "🪟";
        if (s.indexOf("ubuntu") >= 0) return "🟠";
        if (s.indexOf("fedora") >= 0) return "🎩";
        if (s.indexOf("debian") >= 0) return "🌀";
        if (s.indexOf("alpine") >= 0) return "🏔";
        return "🐧";
    }
    function gib(kib) { return (kib / 1048576).toFixed(kib >= 1048576 ? 0 : 1) + " GiB"; }

    HoverHandler { id: hh }
    hovered: hh.hovered

    MouseArea {
        anchors.fill: parent
        anchors.bottomMargin: 44   // leave the action bar clickable
        cursorShape: Qt.PointingHandCursor
        onClicked: vmCard.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        // Identity row
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            Rectangle {
                width: 40; height: 40; radius: Theme.radiusSm
                color: Theme.surfaceAlt
                Text { anchors.centerIn: parent; text: vmCard.osGlyph(osLabel); font.pixelSize: 20 }
            }
            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                RowLayout {
                    spacing: Theme.space2
                    Layout.fillWidth: true
                    Text {
                        text: name; color: Theme.text; font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Rectangle {
                        visible: isTemplate
                        height: 16; width: tmplText.implicitWidth + 10; radius: 8
                        color: Theme.accentSubtle
                        Text { id: tmplText; anchors.centerIn: parent; text: qsTr("TEMPLATE"); color: Theme.accent; font.pixelSize: 9; font.weight: Font.Bold }
                    }
                }
                Text { text: osLabel; color: Theme.textDim; font.pixelSize: Theme.fontXs; elide: Text.ElideRight; Layout.fillWidth: true }
            }
        }

        // Status + spec
        RowLayout {
            Layout.fillWidth: true
            StatusBadge { state: vmCard.state; label: stateText }
            Item { Layout.fillWidth: true }
            Text { text: vcpus + " vCPU · " + vmCard.gib(memoryMax); color: Theme.textFaint; font.pixelSize: Theme.fontXs }
        }

        // Live CPU sparkline (only meaningful while running)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Sparkline {
                anchors.fill: parent
                value: isRunning ? cpuPercent : 0
                maxValue: 100
                opacity: isRunning ? 1 : 0.25
            }
            Text {
                anchors.right: parent.right; anchors.top: parent.top
                text: isRunning ? Math.round(cpuPercent) + "% CPU" : qsTr("idle")
                color: Theme.textFaint; font.pixelSize: Theme.fontXs
            }
        }

        // Action bar
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            IconButton {
                visible: !vmCard.isRunning
                glyph: "▶"; glyphColor: Theme.running; tooltip: qsTr("Start")
                onClicked: App.startVm(connectionId, uuid)
            }
            IconButton {
                visible: vmCard.isRunning
                glyph: "⏸"; glyphColor: Theme.paused; tooltip: qsTr("Pause")
                onClicked: App.pauseVm(connectionId, uuid)
            }
            IconButton {
                visible: vmCard.isPaused
                glyph: "⏵"; glyphColor: Theme.running; tooltip: qsTr("Resume")
                onClicked: App.resumeVm(connectionId, uuid)
            }
            IconButton {
                visible: vmCard.isRunning || vmCard.isPaused
                glyph: "⏻"; glyphColor: Theme.textDim; tooltip: qsTr("Shut down")
                onClicked: App.shutdownVm(connectionId, uuid)
            }
            Item { Layout.fillWidth: true }
            IconButton { glyph: "›"; tooltip: qsTr("Details"); onClicked: vmCard.open() }
        }
    }
}
