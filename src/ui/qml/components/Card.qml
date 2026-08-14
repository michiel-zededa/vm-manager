import QtQuick
import QtQuick.Effects
import VMManager

// Elevated surface used everywhere as a container. Optional hover lift for
// clickable cards.
Rectangle {
    id: card
    property bool interactive: false
    property bool hovered: false

    color: Theme.surface
    radius: Theme.radius
    border.width: 1
    border.color: interactive && hovered ? Theme.accent : Theme.border

    Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easing } }
    scale: interactive && hovered ? 1.01 : 1.0

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Theme.shadow
        shadowVerticalOffset: 2
        shadowBlur: 0.6
    }
}
