import QtQuick
import "../theme"

Item {
    id: root
    property string label: ""
    property real value: 0
    property real minimum: 0
    property real maximum: 1
    property string unit: ""
    property bool integral: false
    signal edited(real value)
    implicitHeight: 49

    function normalized() { return Math.max(0, Math.min(1, (value - minimum) / Math.max(0.000001, maximum - minimum))) }
    function updateFromX(px) {
        let next = minimum + Math.max(0, Math.min(1, px / track.width)) * (maximum - minimum)
        if (integral) next = Math.round(next)
        value = next
        edited(next)
    }

    Text {
        x: 0; y: 1; text: root.label
        color: Theme.secondaryText; font.family: Theme.fontFamily
        font.pixelSize: 12; font.weight: Font.DemiBold
    }
    Text {
        anchors.right: parent.right; y: 1
        text: (root.integral ? Math.round(root.value) : Number(root.value).toFixed(root.maximum <= 2 ? 2 : 1)) + (root.unit.length ? " " + root.unit : "")
        color: Theme.primaryText; font.family: Theme.fontFamily
        font.pixelSize: 12; font.weight: Font.DemiBold
    }
    Rectangle {
        id: track
        x: 0; y: 31; width: parent.width; height: 3; radius: 2
        color: "#302d36"
        Rectangle { width: track.width * root.normalized(); height: parent.height; radius: 2; color: Theme.accent }
        Rectangle {
            x: track.width * root.normalized() - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: drag.pressed ? 11 : 8; height: width; radius: width / 2
            color: "#f4f1f7"
            Behavior on width { NumberAnimation { duration: 90 } }
        }
        MouseArea {
            id: drag; anchors.fill: parent; anchors.margins: -8
            cursorShape: Qt.PointingHandCursor
            onPressed: root.updateFromX(mouse.x)
            onPositionChanged: if (pressed) root.updateFromX(mouse.x)
        }
    }
}
