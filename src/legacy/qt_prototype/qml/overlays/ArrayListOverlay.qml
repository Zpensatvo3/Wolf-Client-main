import QtQuick
import "../theme"

Item {
    id: root
    property var modules: []
    property var sortedModules: modules ? modules.slice().sort((left, right) => right.length - left.length) : []
    property color accentA: "#9b45ff"
    property color accentB: "#4e8cff"
    width: 260; height: list.height

    SequentialAnimation on accentA {
        loops: Animation.Infinite
        ColorAnimation { to: "#4e8cff"; duration: 4200 }
        ColorAnimation { to: "#d354ff"; duration: 4200 }
        ColorAnimation { to: "#9b45ff"; duration: 4200 }
    }
    SequentialAnimation on accentB {
        loops: Animation.Infinite
        ColorAnimation { to: "#42d8c8"; duration: 5100 }
        ColorAnimation { to: "#705cff"; duration: 5100 }
        ColorAnimation { to: "#4e8cff"; duration: 5100 }
    }

    Column {
        id: list; anchors.right: parent.right; spacing: -2
        Repeater {
            model: root.sortedModules
            Rectangle {
                id: row
                required property string modelData
                required property int index
                anchors.right: parent.right
                width: nameText.implicitWidth + 26; height: 33; radius: 9
                color: "#ed090a0f"
                border.width: 1; border.color: "#12ffffff"
                opacity: 1
                Behavior on width { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                Text {
                    id: nameText; anchors.centerIn: parent; text: modelData
                    color: Qt.rgba(root.accentA.r * (1 - index / Math.max(1, root.sortedModules.length)) + root.accentB.r * index / Math.max(1, root.sortedModules.length),
                                   root.accentA.g * (1 - index / Math.max(1, root.sortedModules.length)) + root.accentB.g * index / Math.max(1, root.sortedModules.length),
                                   root.accentA.b * (1 - index / Math.max(1, root.sortedModules.length)) + root.accentB.b * index / Math.max(1, root.sortedModules.length), 1)
                    font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold
                }
            }
        }
    }
}
