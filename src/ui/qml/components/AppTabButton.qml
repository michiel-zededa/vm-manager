import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed tab button with an accent underline for the checked tab.
TabButton {
    id: control
    implicitHeight: 40
    font.pixelSize: Theme.fontMd
    padding: Theme.space3

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? Theme.text : Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered && !control.checked ? Theme.surfaceAlt : "transparent"
        radius: Theme.radiusSm
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - Theme.space3
            height: 2
            radius: 1
            color: control.checked ? Theme.accent : "transparent"
        }
    }
}
