import QtQuick
import "../theme"

Rectangle {
    id: root
    property var configData: ({})
    property bool selected: false
    signal selectedRequested()
    signal favouriteRequested()
    width: parent ? parent.width : 260
    height: 76
    radius: Theme.cardRadius
    color: selected ? Theme.cardSelected : (hover.containsMouse ? Theme.cardHover : Theme.card)
    border.width: selected ? 1 : 0
    border.color: Theme.accent
    Behavior on color { ColorAnimation { duration: Theme.hoverDuration } }

    Rectangle {
        x: 10; y: 11; width: 34; height: 34; radius: 9
        color: Qt.rgba(0.61, 0.27, 1, 0.14)
        Text { anchors.centerIn: parent; text: configData.name ? configData.name.charAt(0).toUpperCase() : "C"; color: Theme.accentHover; font.family: Theme.fontFamily; font.pixelSize: 14; font.bold: true }
    }
    Text { x: 54; y: 10; width: parent.width - 88; text: configData.name || "Config"; color: Theme.primaryText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
    Text { x: 54; y: 30; width: parent.width - 88; text: (configData.author || "Unknown") + "  /  " + (configData.modules || 0) + " modules"; color: Theme.mutedText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 9 }
    Text { x: 11; y: 56; text: (configData.downloads || 0) + " loads"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 11; y: 8
        text: configData.favourite ? "*" : "+"; color: configData.favourite ? Theme.accentHover : Theme.mutedText
        font.pixelSize: 16; font.bold: true
        MouseArea { anchors.fill: parent; anchors.margins: -8; cursorShape: Qt.PointingHandCursor; onClicked: root.favouriteRequested() }
    }
    MouseArea { id: hover; anchors.fill: parent; anchors.rightMargin: 34; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selectedRequested() }
}
