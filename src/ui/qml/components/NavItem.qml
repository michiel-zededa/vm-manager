import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Sidebar navigation row.
ItemDelegate {
    id: item
    property string glyph: ""
    property bool selected: false
    property int badgeCount: -1

    implicitHeight: 40
    hoverEnabled: true
    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: Theme.radiusSm
        color: item.selected ? Theme.accentSubtle
                             : item.hovered ? Theme.surfaceAlt : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    contentItem: Row {
        spacing: Theme.space3
        leftPadding: Theme.space3
        rightPadding: Theme.space3
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: item.glyph
            font.pixelSize: Theme.fontLg
            color: item.selected ? Theme.accent : Theme.textDim
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: item.text
            font.pixelSize: Theme.fontMd
            font.weight: item.selected ? Font.DemiBold : Font.Normal
            color: item.selected ? Theme.text : Theme.textDim
            width: item.width - 90
            elide: Text.ElideRight
        }
    }

    // Optional count chip on the right.
    Rectangle {
        visible: item.badgeCount >= 0
        anchors.right: parent.right
        anchors.rightMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        height: 18
        width: Math.max(18, countText.implicitWidth + 10)
        radius: 9
        color: Theme.surfaceAlt
        Text {
            id: countText
            anchors.centerIn: parent
            text: item.badgeCount
            font.pixelSize: Theme.fontXs
            color: Theme.textDim
        }
    }
}
