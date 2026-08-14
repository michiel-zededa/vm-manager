import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed spin box (numeric stepper) readable in both themes.
SpinBox {
    id: control
    implicitHeight: 36
    font.pixelSize: Theme.fontMd
    editable: true

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: Theme.text
        selectionColor: Theme.selection
        selectedTextColor: Theme.accentText
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }
    background: Rectangle {
        implicitWidth: 120
        radius: Theme.radiusSm
        color: Theme.field
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.fieldBorder
    }
    up.indicator: Rectangle {
        x: control.width - width; height: control.height; width: 28
        radius: Theme.radiusSm
        color: control.up.pressed ? Theme.surfaceHover : "transparent"
        Text { anchors.centerIn: parent; text: "+"; color: Theme.textDim; font.pixelSize: Theme.fontLg }
    }
    down.indicator: Rectangle {
        height: control.height; width: 28
        radius: Theme.radiusSm
        color: control.down.pressed ? Theme.surfaceHover : "transparent"
        Text { anchors.centerIn: parent; text: "−"; color: Theme.textDim; font.pixelSize: Theme.fontLg }
    }
}
