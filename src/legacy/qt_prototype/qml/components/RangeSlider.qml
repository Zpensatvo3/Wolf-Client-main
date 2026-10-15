import QtQuick
import "../theme"

Item {
    id: root
    property string label: ""
    property real low: 0
    property real high: 1
    property real minimum: 0
    property real maximum: 1
    property string unit: ""
    property bool integral: false
    signal edited(real low, real high)
    implicitHeight: 49

    function position(value) { return (value - minimum) / Math.max(0.000001, maximum - minimum) }
    function valueAt(px) {
        let value = minimum + Math.max(0, Math.min(1, px / track.width)) * (maximum - minimum)
        return integral ? Math.round(value) : value
    }
    function updateFromMouse(px) {
        let next = valueAt(px)
        if (drag.editLow) low = Math.min(next, high)
        else high = Math.max(next, low)
        edited(low, high)
    }

    Text { x: 0; y: 1; text: root.label; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
    Text {
        anchors.right: parent.right; y: 1
        text: (integral ? Math.round(low) : Number(low).toFixed(1)) + " - " +
              (integral ? Math.round(high) : Number(high).toFixed(1)) + (unit.length ? " " + unit : "")
        color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold
    }
    Rectangle {
        id: track; x: 0; y: 31; width: parent.width; height: 3; radius: 2; color: "#302d36"
        Rectangle {
            x: track.width * root.position(root.low)
            width: track.width * (root.position(root.high) - root.position(root.low))
            height: parent.height; radius: 2; color: Theme.accent
        }
        Rectangle { x: track.width * root.position(root.low) - 4; y: -3; width: 9; height: 9; radius: 5; color: Theme.primaryText }
        Rectangle { x: track.width * root.position(root.high) - 4; y: -3; width: 9; height: 9; radius: 5; color: Theme.primaryText }
        MouseArea {
            id: drag
            anchors.fill: parent; anchors.margins: -8; cursorShape: Qt.PointingHandCursor
            property bool editLow: true
            onPressed: mouse => {
                editLow = Math.abs(mouse.x - track.width * root.position(root.low)) <= Math.abs(mouse.x - track.width * root.position(root.high))
                root.updateFromMouse(mouse.x)
            }
            onPositionChanged: mouse => { if (pressed) root.updateFromMouse(mouse.x) }
        }
    }
}
