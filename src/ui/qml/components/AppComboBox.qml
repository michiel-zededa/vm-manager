import QtQuick
import QtQuick.Controls.Basic
import VMManager

// Themed dropdown. The stock Basic ComboBox renders as a bright white box that
// is unreadable on the dark theme — this restyles field, popup and delegates.
ComboBox {
    id: control
    implicitHeight: 38
    font.pixelSize: Theme.fontMd
    leftPadding: Theme.space3
    rightPadding: Theme.space5

    contentItem: Text {
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: Theme.space1
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled ? Theme.field : Theme.surfaceAlt
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                     : control.hovered ? Theme.borderStrong : Theme.fieldBorder
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    indicator: Text {
        x: control.width - width - Theme.space3
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        color: Theme.textDim
        font.pixelSize: Theme.fontSm
    }

    delegate: ItemDelegate {
        width: ListView.view.width
        height: 34
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        contentItem: Text {
            text: modelData !== undefined ? modelData
                  : (control.textRole ? model[control.textRole] : model.display)
            color: Theme.text
            font.pixelSize: Theme.fontMd
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: highlighted ? Theme.accentSubtle : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: Theme.space1
        implicitHeight: Math.min(contentItem.implicitHeight + Theme.space2, 280)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
        }
    }
}
