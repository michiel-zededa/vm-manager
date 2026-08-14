import QtQuick
import VMManager

Column {
    property string title: ""
    property string subtitle: ""
    spacing: Theme.space1

    Text {
        text: title
        color: Theme.text
        font.pixelSize: Theme.fontXl
        font.weight: Font.DemiBold
    }
    Text {
        visible: subtitle.length > 0
        text: subtitle
        color: Theme.textDim
        font.pixelSize: Theme.fontSm
    }
}
