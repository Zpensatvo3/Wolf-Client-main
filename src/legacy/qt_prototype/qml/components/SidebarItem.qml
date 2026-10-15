import QtQuick
import "../theme"

Item {
    id: root
    property string label: ""
    property string iconText: ""
    property bool selected: false
    signal clicked()
    implicitWidth: 164
    implicitHeight: 38

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.selected ? Theme.accent : (mouse.containsMouse ? "#16ffffff" : "transparent")
        Behavior on color { ColorAnimation { duration: Theme.normalDuration } }
    }

    Text {
        x: 13; anchors.verticalCenter: parent.verticalCenter
        text: root.iconText
        color: root.selected ? "white" : Theme.secondaryText
        font.family: Theme.iconFontFamily
        font.pixelSize: 17
        font.weight: Font.DemiBold
    }
    Text {
        x: 43; anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.selected ? "white" : Theme.secondaryText
        font.family: Theme.fontFamily
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
