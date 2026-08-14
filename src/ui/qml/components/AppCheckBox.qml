import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed checkbox with a readable label in both themes.
CheckBox {
    id: control
    font.pixelSize: Theme.fontMd
    spacing: Theme.space3

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: Theme.radiusSm - 2
        color: control.checked ? Theme.accent : Theme.field
        border.width: 1
        border.color: control.checked ? Theme.accent : Theme.fieldBorder
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: "#FFFFFF"
            font.pixelSize: 13
            font.bold: true
        }
    }

    contentItem: Text {
        text: control.text
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        leftPadding: control.indicator.width + control.spacing
    }
}
