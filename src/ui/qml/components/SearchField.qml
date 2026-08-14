import QtQuick
import QtQuick.Controls.Basic
import VMManager

TextField {
    id: field
    placeholderText: qsTr("Search…")
    implicitHeight: 36
    leftPadding: 34
    rightPadding: Theme.space3
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    font.pixelSize: Theme.fontMd
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.surfaceAlt
        border.width: 1
        border.color: field.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    Text {
        text: "⌕"   // magnifier
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        color: Theme.textFaint
        font.pixelSize: Theme.fontLg
    }
}
