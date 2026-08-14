import QtQuick
import VMManager

// Friendly zero-state with an optional call to action.
Column {
    id: empty
    property string glyph: "🗂"
    property string title: ""
    property string body: ""
    property Component action: null
    spacing: Theme.space3
    width: parent ? parent.width : 320

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: empty.glyph
        font.pixelSize: 48
        opacity: 0.7
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: empty.title
        color: Theme.text
        font.pixelSize: Theme.fontLg
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: empty.body
        color: Theme.textDim
        font.pixelSize: Theme.fontSm
        width: Math.min(420, empty.width)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }
    Loader {
        id: actionLoader
        anchors.horizontalCenter: parent.horizontalCenter
        sourceComponent: empty.action
    }
}
