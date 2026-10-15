import QtQuick
import "../theme"

Rectangle {
    id: root
    property string message: ""
    property bool success: true
    width: Math.min(360, Math.max(210, label.implicitWidth + 48))
    height: 42
    radius: 11
    color: "#f41a181f"
    border.width: 1
    border.color: success ? Qt.rgba(0.61, 0.27, 1, 0.55) : Qt.rgba(0.88, 0.36, 0.44, 0.65)
    opacity: message.length ? 1 : 0
    y: message.length ? 0 : 10
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: 180 } }
    Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Rectangle { x: 13; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: success ? Theme.accentHover : Theme.danger }
    Text { id: label; x: 29; anchors.verticalCenter: parent.verticalCenter; text: root.message; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
}
