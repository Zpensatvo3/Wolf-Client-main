pragma Singleton
import QtQuick

QtObject {
    readonly property color root: "#07070b"
    readonly property color window: "#f0141219"
    readonly property color sidebar: "#f719171e"
    readonly property color mainSurface: "#ed111016"
    readonly property color card: "#e81a181f"
    readonly property color cardHover: "#f0222029"
    readonly property color cardSelected: "#f0292137"
    readonly property color settingsPanel: "#f217111f"
    readonly property color primaryText: "#f4f1f7"
    readonly property color secondaryText: "#c3bdc9"
    readonly property color mutedText: "#85808e"
    readonly property color accent: "#9b45ff"
    readonly property color accentHover: "#ac61ff"
    readonly property color danger: "#e05d70"
    readonly property color border: "#14ffffff"
    readonly property color separator: "#12ffffff"
    readonly property string fontFamily: "Google Sans"
    readonly property string iconFontFamily: "Font Awesome 6 Free"
    readonly property int shellRadius: 20
    readonly property int panelRadius: 15
    readonly property int cardRadius: 9
    readonly property int sidebarWidth: 204
    readonly property int topBarHeight: 54
    readonly property int settingsWidth: 304
    readonly property int hoverDuration: 100
    readonly property int normalDuration: 160
    readonly property int panelDuration: 220
}
