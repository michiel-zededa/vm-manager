import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VMManager

// A labelled slider with an editable numeric field and optional unit selector.
// `value` is always kept in `baseUnit`; the field shows it in the currently
// selected display unit. Example (memory):
//   LabeledSlider { label: "Memory"; from: 256; to: 131072; stepSize: 256
//                   value: 2048; unitOptions: [{name:"MiB",factor:1},
//                                               {name:"GiB",factor:1024}] }
ColumnLayout {
    id: root
    spacing: Theme.space2
    Layout.fillWidth: true

    property string label: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property string unit: ""                 // static unit when no unitOptions
    property var unitOptions: []             // [{name, factor}] for a unit picker
    property int decimals: 0

    // Which unit is selected; defaults to the largest that keeps value >= 1.
    property int unitIndex: 0
    readonly property real factor: unitOptions.length > 0
                                   ? unitOptions[unitIndex].factor : 1
    readonly property string unitName: unitOptions.length > 0
                                       ? unitOptions[unitIndex].name : unit

    function _fmt(v) {
        var d = v / factor
        return decimals > 0 ? d.toFixed(decimals) : Math.round(d).toString()
    }
    function _commit(text) {
        var n = parseFloat(text)
        if (isNaN(n)) { field.text = _fmt(value); return }
        var base = n * factor
        base = Math.max(from, Math.min(to, base))
        base = Math.round(base / stepSize) * stepSize
        value = base
        field.text = _fmt(value)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space3
        Text {
            text: root.label
            color: Theme.textDim
            font.pixelSize: Theme.fontMd
            Layout.preferredWidth: 96
        }
        Item { Layout.fillWidth: true }
        AppTextField {
            id: field
            Layout.preferredWidth: 88
            implicitHeight: 32
            horizontalAlignment: Text.AlignRight
            text: root._fmt(root.value)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onEditingFinished: root._commit(text)
            Connections {
                target: root
                function onValueChanged() { if (!field.activeFocus) field.text = root._fmt(root.value) }
                function onUnitIndexChanged() { field.text = root._fmt(root.value) }
            }
        }
        AppComboBox {
            visible: root.unitOptions.length > 0
            Layout.preferredWidth: 78
            implicitHeight: 32
            model: root.unitOptions.map(function(u){ return u.name })
            currentIndex: root.unitIndex
            onActivated: root.unitIndex = currentIndex
        }
        Text {
            visible: root.unitOptions.length === 0 && root.unit !== ""
            text: root.unit
            color: Theme.textFaint
            font.pixelSize: Theme.fontSm
            Layout.preferredWidth: 44
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        stepSize: root.stepSize
        value: root.value
        onMoved: root.value = value

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            radius: 3
            color: Theme.surfaceHover
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 3
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 20; height: 20; radius: 10
            color: "#FFFFFF"
            border.width: 2
            border.color: Theme.accent
            scale: slider.pressed ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: Theme.durFast } }
        }
    }
}
