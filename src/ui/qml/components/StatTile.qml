import QtQuick
import VMManager

// Compact metric tile: big value + label, used on the dashboard header and the
// VM detail page.
Column {
    property string label: ""
    property string value: ""
    property color valueColor: Theme.text
    spacing: 2

    Text {
        text: value
        color: valueColor
        font.pixelSize: Theme.fontXl
        font.weight: Font.DemiBold
    }
    Text {
        text: label
        color: Theme.textDim
        font.pixelSize: Theme.fontXs
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.5
    }
}
