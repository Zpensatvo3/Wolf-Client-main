import QtQuick
import "../theme"

GlassSurface {
    id: root
    width: 250; height: 420
    surfaceColor: "#ed17131e"
    surfaceRadius: 16
    Text { x: 16; y: 15; text: "HUD PREVIEW"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
    Rectangle {
        x: 18; y: 56; width: parent.width - 36; height: 278; radius: 12; color: "#20000000"; border.width: 1; border.color: Theme.border
        Text { anchors.centerIn: parent; text: "Thedorius\nHUD"; horizontalAlignment: Text.AlignHCenter; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 18; font.bold: true }
    }
    Text { x: 18; y: 354; text: "Watermark  /  ArrayList"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
    Text { x: 18; y: 377; width: parent.width - 36; text: "Live positions and theme colors are mirrored from the game."; wrapMode: Text.WordWrap; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
}
