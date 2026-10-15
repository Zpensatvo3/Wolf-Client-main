import QtQuick
import "../theme"

Item {
    id: root
    property string title: "Combat"
    property string subtitle: "Combat assistance and timing modules"
    signal searchRequested()
    height: Theme.topBarHeight
    Text { x: 18; anchors.verticalCenter: parent.verticalCenter; text: root.title; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 16; font.bold: true }
    Rectangle { x: 112; anchors.verticalCenter: parent.verticalCenter; width: 3; height: 3; radius: 2; color: Theme.mutedText }
    Text { x: 125; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 195; text: root.subtitle; elide: Text.ElideRight; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 }
    Rectangle {
        width: 36; height: 36; radius: 10; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
        color: searchMouse.containsMouse ? "#1cffffff" : "transparent"
        Text { anchors.centerIn: parent; text: "\uf002"; color: Theme.secondaryText; font.family: Theme.iconFontFamily; font.pixelSize: 13 }
        MouseArea { id: searchMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.searchRequested() }
    }
}
