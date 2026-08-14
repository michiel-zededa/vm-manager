import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed single-line text input with a readable placeholder in both themes.
TextField {
    id: control
    color: Theme.text
    placeholderTextColor: Theme.placeholder
    selectionColor: Theme.selection
    selectedTextColor: Theme.accentText
    font.pixelSize: Theme.fontMd
    padding: Theme.space3
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    implicitHeight: 38
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled ? Theme.field : Theme.surfaceAlt
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                     : control.hovered ? Theme.borderStrong : Theme.fieldBorder
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }
}
