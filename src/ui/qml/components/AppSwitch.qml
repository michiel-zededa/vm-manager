import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed on/off switch with an optional trailing label.
Switch {
    id: control
    font.pixelSize: Theme.fontMd
    spacing: Theme.space2

    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 24
        radius: height / 2
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        color: control.checked ? Theme.accent : Theme.surfaceHover
        border.width: control.checked ? 0 : 1
        border.color: Theme.fieldBorder
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 18; height: 18; radius: 9
            color: "#FFFFFF"
            Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing } }
        }
    }

    contentItem: Text {
        text: control.text
        color: Theme.textDim
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
