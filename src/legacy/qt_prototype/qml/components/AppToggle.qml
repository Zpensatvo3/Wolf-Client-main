import QtQuick
import "../theme"

Item {
    id: root
    property bool checked: false
    property bool enabled: true
    signal toggled(bool value)
    implicitWidth: 38
    implicitHeight: 22
    opacity: enabled ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : "#34313b"
        Behavior on color { ColorAnimation { duration: Theme.normalDuration } }
    }
    Rectangle {
        width: 16; height: 16; radius: 8
        y: 3; x: root.checked ? parent.width - width - 3 : 3
        color: "#f5f2f7"
        Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    }
    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
