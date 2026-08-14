import QtQuick
import QtQuick.Layouts
import VMManager

// Transient notification banner. Driven by App.notify via show().
Item {
    id: root
    width: card.width
    height: card.height
    z: 1000
    property int queueLimit: 1

    function levelColor(level) {
        switch (level) {
        case 1: return Theme.running;   // success
        case 2: return Theme.paused;    // warning
        case 3: return Theme.danger;    // error
        default: return Theme.info;     // info
        }
    }

    function show(level, title, message) {
        accent.color = levelColor(level);
        titleText.text = title;
        bodyText.text = message;
        card.opacity = 0; card.y = -12;
        showAnim.restart();
        hideTimer.restart();
    }

    Card {
        id: card
        width: Math.max(280, Math.min(440, contentRow.implicitWidth + Theme.space5))
        height: 60
        opacity: 0

        RowLayout {
            id: contentRow
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space3
            Rectangle { id: accent; width: 4; Layout.fillHeight: true; radius: 2 }
            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                Text { id: titleText; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { id: bodyText; color: Theme.textDim; font.pixelSize: Theme.fontSm; elide: Text.ElideRight; Layout.fillWidth: true }
            }
        }

        MouseArea { anchors.fill: parent; onClicked: hideAnim.restart() }
    }

    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: card; property: "opacity"; to: 1; duration: Theme.durMed }
        NumberAnimation { target: card; property: "y"; to: 0; duration: Theme.durMed; easing.type: Theme.easing }
    }
    NumberAnimation { id: hideAnim; target: card; property: "opacity"; to: 0; duration: Theme.durMed }
    Timer { id: hideTimer; interval: 3600; onTriggered: hideAnim.restart() }
}
