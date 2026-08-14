pragma Singleton
import QtQuick

// Single source of truth for the visual language. Every color, space, radius,
// font size and motion timing used in the app comes from here — no magic
// numbers in views.
//
// `mode` selects the palette: 0 = follow the OS, 1 = light, 2 = dark. When
// following the OS, `dark` tracks Application.styleHints.colorScheme live, so
// the whole app re-themes the moment the system appearance changes.
QtObject {
    id: theme

    // 0 = system, 1 = light, 2 = dark. Persisted in Main.qml via QtCore.Settings.
    property int mode: 0
    readonly property bool dark: mode === 1 ? false
                               : mode === 2 ? true
                               : Application.styleHints.colorScheme === Qt.Dark

    // ---- Brand -------------------------------------------------------------
    readonly property color accent:        dark ? "#6366F1" : "#4F46E5"
    readonly property color accentHover:   dark ? "#7C7DF5" : "#6366F1"
    readonly property color accentPressed: dark ? "#4F46E5" : "#4338CA"
    readonly property color accentText:    "#FFFFFF"
    readonly property color accentSubtle:  dark ? "#1E2140" : "#EEF0FF"
    readonly property color accentBorder:  dark ? "#3B3F73" : "#C7CBFF"

    // ---- Semantic / status -------------------------------------------------
    readonly property color running: dark ? "#22C55E" : "#16A34A"
    readonly property color paused:  dark ? "#F59E0B" : "#D97706"
    readonly property color stopped: dark ? "#64748B" : "#94A3B8"
    readonly property color danger:  dark ? "#EF4444" : "#DC2626"
    readonly property color dangerSubtle: dark ? "#3A1D22" : "#FEE2E2"
    readonly property color info:    dark ? "#38BDF8" : "#0284C7"
    readonly property color warning: dark ? "#F59E0B" : "#B45309"
    readonly property color success: dark ? "#22C55E" : "#16A34A"

    // ---- Surfaces ----------------------------------------------------------
    readonly property color bg:          dark ? "#0F1117" : "#F4F5F8"
    readonly property color surface:     dark ? "#171A21" : "#FFFFFF"
    readonly property color surfaceAlt:  dark ? "#1E222B" : "#EDEFF3"
    readonly property color surfaceHover:dark ? "#252A35" : "#E2E6EE"
    readonly property color sidebar:     dark ? "#12141A" : "#FAFBFC"
    readonly property color border:      dark ? "#2A2F3A" : "#D6DBE4"
    readonly property color borderStrong:dark ? "#3A4150" : "#C2C9D6"
    readonly property color overlay:     dark ? "#000000CC" : "#1A1D2480"

    // ---- Inputs (fields, dropdowns) ----------------------------------------
    readonly property color field:        dark ? "#1E222B" : "#FFFFFF"
    readonly property color fieldBorder:  dark ? "#333A47" : "#CBD2DD"
    readonly property color fieldFocus:   accent
    readonly property color placeholder:  dark ? "#828B9C" : "#8A93A3"
    readonly property color selection:    accent

    // ---- Text --------------------------------------------------------------
    readonly property color text:      dark ? "#E6E9EF" : "#161A22"
    readonly property color textDim:   dark ? "#A2ABBA" : "#4A5262"
    readonly property color textFaint: dark ? "#727C8E" : "#727C8C"
    readonly property color textOnAccent: "#FFFFFF"

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
