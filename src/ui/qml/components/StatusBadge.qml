import QtQuick
import VMManager

// Pill showing a VM's run state with a colored dot.
Row {
    id: badge
    property int state: 0
    property string label: ""
    spacing: Theme.space2

    Rectangle {
        width: 8; height: 8; radius: 4
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.stateColor(badge.state)

        // Gentle pulse while running.
        SequentialAnimation on opacity {
            running: badge.state === 1
            loops: Animation.Infinite
            NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0;  duration: 900; easing.type: Easing.InOutSine }
        }
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: badge.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSm
    }
}
