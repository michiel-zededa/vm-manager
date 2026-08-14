import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Primary action button with variants. Usage:
//   AppButton { text: "Start"; variant: "primary"; onClicked: ... }
Button {
    id: control
    property string variant: "primary"   // primary | ghost | danger | subtle
    property color accentColor: variant === "danger" ? Theme.danger : Theme.accent

    implicitHeight: 36
    padding: Theme.space3
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    font.pixelSize: Theme.fontMd
    font.weight: Font.Medium
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: {
            if (control.variant === "primary" || control.variant === "danger")
                return Theme.accentText;
            return control.enabled ? Theme.text : Theme.textFaint;
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        border.width: control.variant === "ghost" ? 1 : 0
        border.color: Theme.border
        color: {
            if (!control.enabled) return Theme.surfaceAlt;
            if (control.variant === "ghost") return control.hovered ? Theme.surfaceHover : "transparent";
            if (control.variant === "subtle") return control.down ? Theme.surfaceHover
                                                     : control.hovered ? Theme.surfaceAlt : Theme.surface;
            // primary / danger
            return control.down ? Qt.darker(control.accentColor, 1.15)
                                : control.hovered ? Qt.lighter(control.accentColor, 1.1)
                                                  : control.accentColor;
        }
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }
}
