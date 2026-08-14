import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Compact square button that shows a glyph (we use text glyphs to stay
// dependency-free; phase 1 can swap in an icon font / SVG set).
Button {
    id: control
    property string glyph: "•"
    property color glyphColor: Theme.text
    property string tooltip: ""

    implicitWidth: 34
    implicitHeight: 34
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    ToolTip.visible: tooltip.length > 0 && hovered
    ToolTip.text: tooltip
    ToolTip.delay: 400

    contentItem: Text {
        text: control.glyph
        color: control.enabled ? control.glyphColor : Theme.textFaint
        font.pixelSize: Theme.fontLg
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.radiusSm
        color: control.down ? Theme.surfaceHover
                            : control.hovered ? Theme.surfaceAlt : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }
}
