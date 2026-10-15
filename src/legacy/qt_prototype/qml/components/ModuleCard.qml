import QtQuick
import "../theme"

Rectangle {
    id: root
    property var moduleData
    property bool selected: false
    signal clicked()
    implicitHeight: 49
    radius: Theme.cardRadius
    color: selected ? Theme.cardSelected : (mouse.containsMouse ? Theme.cardHover : Theme.card)
    border.width: selected ? 1 : 0
    border.color: selected ? "#679b45ff" : "transparent"
    clip: true
    Behavior on color { ColorAnimation { duration: Theme.hoverDuration } }

    Rectangle {
        x: -parent.width; y: 0; width: parent.width; height: parent.height
        color: "#169b45ff"
        NumberAnimation on x {
            running: mouse.containsMouse
            from: -root.width; to: root.width
            duration: 480
        }
    }
    Text {
        x: 12; y: 8; width: parent.width - 30
        text: moduleData ? moduleData.name : ""
        elide: Text.ElideRight
        color: Theme.primaryText
        font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold
    }
    Text {
        x: 12; y: 27; width: parent.width - 30
        text: moduleData ? moduleData.description : ""
        elide: Text.ElideRight
        color: Theme.mutedText
        font.family: Theme.fontFamily; font.pixelSize: 10
    }
    Rectangle {
        width: 5; height: 5; radius: 3
        anchors.right: parent.right; anchors.rightMargin: 11
        anchors.verticalCenter: parent.verticalCenter
        color: moduleData && moduleData.enabled ? Theme.accentHover : "#50505a"
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
