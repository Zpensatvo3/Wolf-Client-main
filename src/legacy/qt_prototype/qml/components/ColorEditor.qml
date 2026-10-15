import QtQuick
import "../theme"

GlassSurface {
    id: root
    property color selectedColor: Theme.accent
    signal colorEdited(color value)
    width: 244; height: 290
    surfaceColor: "#f21a171e"
    surfaceRadius: 13

    Rectangle {
        id: field; x: 12; y: 14; width: parent.width - 24; height: 126; radius: 9
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "white" }
            GradientStop { position: 1; color: root.selectedColor }
        }
        Rectangle {
            anchors.fill: parent; radius: parent.radius
            gradient: Gradient { GradientStop { position: 0; color: "transparent" }; GradientStop { position: 1; color: "black" } }
        }
        MouseArea {
            anchors.fill: parent
            onPressed: positionChanged(mouse)
            onPositionChanged: mouse => {
                if (!pressed) return
                root.selectedColor = Qt.hsla(root.selectedColor.hslHue,
                    Math.max(0, Math.min(1, mouse.x / width)),
                    Math.max(0, Math.min(1, 1 - mouse.y / height)), 1)
                root.colorEdited(root.selectedColor)
            }
        }
    }
    Text { x: 12; y: 149; text: "Hue"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
    Rectangle {
        id: hue; x: 12; y: 165; width: parent.width - 24; height: 14; radius: 7
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "red" }
            GradientStop { position: 0.17; color: "yellow" }
            GradientStop { position: 0.34; color: "lime" }
            GradientStop { position: 0.51; color: "cyan" }
            GradientStop { position: 0.68; color: "blue" }
            GradientStop { position: 0.85; color: "magenta" }
            GradientStop { position: 1.0; color: "red" }
        }
        MouseArea {
            anchors.fill: parent
            onPressed: positionChanged(mouse)
            onPositionChanged: mouse => {
                if (!pressed) return
                root.selectedColor = Qt.hsla(Math.max(0, Math.min(1, mouse.x / width)),
                    root.selectedColor.hslSaturation, root.selectedColor.hslLightness, 1)
                root.colorEdited(root.selectedColor)
            }
        }
    }
    Text { x: 12; y: 195; text: "Palette"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
    Row {
        x: 12; y: 214; spacing: 8
        Repeater {
            model: ["#9b45ff", "#ef3c57", "#35cf86", "#3893ef", "#ef4f9b", "#18c8d2", "#ff8c2e"]
            Rectangle {
                required property var modelData
                width: 21; height: 21; radius: 6; color: modelData
                MouseArea { anchors.fill: parent; onClicked: { root.selectedColor = modelData; root.colorEdited(root.selectedColor) } }
            }
        }
    }
    Rectangle { x: 12; y: 252; width: parent.width - 24; height: 1; color: Theme.separator }
    Text { x: 12; y: 267; text: "Solid color"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
}
