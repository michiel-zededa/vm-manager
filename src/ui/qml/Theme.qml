pragma Singleton
import QtQuick

// Single source of truth for the visual language. Every color, space, radius,
// font size and motion timing used in the app comes from here — no magic
// numbers in views. Flip `dark` to retheme the whole app.
QtObject {
    id: theme

    // Follows the OS in a real build (phase 1 wires QStyleHints.colorScheme);
    // defaults to dark, which suits a dense management tool.
    property bool dark: true

    // ---- Brand -------------------------------------------------------------
    readonly property color accent:        "#6366F1"   // indigo 500
    readonly property color accentHover:   "#7C7DF5"
    readonly property color accentPressed: "#4F46E5"
    readonly property color accentText:    "#FFFFFF"
    readonly property color accentSubtle:  dark ? "#1E2140" : "#EEF0FF"

    // ---- Semantic / status -------------------------------------------------
    readonly property color running: "#22C55E"
    readonly property color paused:  "#F59E0B"
    readonly property color stopped: "#64748B"
    readonly property color danger:  "#EF4444"
    readonly property color info:    "#38BDF8"

    // ---- Surfaces ----------------------------------------------------------
    readonly property color bg:         dark ? "#0F1117" : "#F4F5F8"
    readonly property color surface:    dark ? "#171A21" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#1E222B" : "#EDEFF3"
    readonly property color surfaceHover: dark ? "#232833" : "#E4E7ED"
    readonly property color sidebar:    dark ? "#12141A" : "#FAFBFC"
    readonly property color border:     dark ? "#2A2F3A" : "#DEE2EA"
    readonly property color overlay:    dark ? "#000000CC" : "#1A1D2499"

    // ---- Text --------------------------------------------------------------
    readonly property color text:      dark ? "#E6E9EF" : "#1A1D24"
    readonly property color textDim:   dark ? "#9AA3B2" : "#5B6472"
    readonly property color textFaint: dark ? "#5C6575" : "#9AA1AD"

    // ---- Spacing (4pt scale) -----------------------------------------------
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int space7: 48

    // ---- Radius ------------------------------------------------------------
    readonly property int radiusSm: 6
    readonly property int radius:   10
    readonly property int radiusLg: 16
    readonly property int radiusPill: 999

    // ---- Typography --------------------------------------------------------
    readonly property string fontFamily: Qt.application.font.family
    readonly property string monoFamily: "monospace"
    readonly property int fontXs: 11
    readonly property int fontSm: 12
    readonly property int fontMd: 14
    readonly property int fontLg: 16
    readonly property int fontXl: 20
    readonly property int fontXxl: 28

    // ---- Motion ------------------------------------------------------------
    readonly property int durFast: 120
    readonly property int durMed:  200
    readonly property int durSlow: 320
    readonly property int easing:  Easing.OutCubic

    // ---- Elevation (shadow tuning) ----------------------------------------
    readonly property color shadow: dark ? "#00000066" : "#0F172A1A"

    function stateColor(state) {
        // Mirrors vmm::VmState: 0 none,1 running,2 paused,3 shutting,4 shutoff,5 crashed,6 suspended
        switch (state) {
        case 1: return running;
        case 2: case 6: return paused;
        case 3: return info;
        case 5: return danger;
        default: return stopped;
        }
    }
}
