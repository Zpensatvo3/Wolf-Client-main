import QtQuick
import "../theme"

GlassSurface {
    id: root
    property bool opened: false
    signal closeRequested()
    width: opened ? 330 : 48
    height: opened ? Math.min(286, 72 + resultColumn.height) : 42
    opacity: opened ? 1 : 0
    scale: opened ? 1 : 0.88
    visible: opacity > 0.01
    surfaceColor: "#f418161d"
    surfaceRadius: 13
    clip: true
    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 150 } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

    TextInput {
        id: input
        x: 14; y: 12; width: parent.width - 44; height: 28
        color: Theme.primaryText; selectionColor: Theme.accent
        font.family: Theme.fontFamily; font.pixelSize: 13
        clip: true; focus: root.opened
        onTextChanged: App.searchText = text
        Keys.onEscapePressed: root.closeRequested()
        Text { anchors.fill: parent; text: "Search modules"; visible: !input.text.length; color: Theme.mutedText; font: input.font }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 15; y: 14; text: "x"; color: Theme.mutedText; font.pixelSize: 14; font.bold: true }
    MouseArea { x: parent.width - 38; y: 5; width: 34; height: 34; onClicked: root.closeRequested() }
    Rectangle { x: 12; y: 50; width: parent.width - 24; height: 1; color: Theme.separator }

    Flickable {
        x: 8; y: 57; width: parent.width - 16; height: parent.height - 64
        clip: true; contentHeight: resultColumn.height; boundsBehavior: Flickable.StopAtBounds
        Column {
            id: resultColumn; width: parent.width; spacing: 3
            Repeater {
                model: App.allModules
                Rectangle {
                    required property var modelData
                    width: resultColumn.width; height: matches ? 42 : 0; visible: matches; radius: 7
                    property bool matches: !input.text.length || (modelData.name + " " + modelData.description + " " + modelData.category).toLowerCase().includes(input.text.toLowerCase())
                    color: resultMouse.containsMouse ? "#1affffff" : "transparent"
                    Text { x: 9; y: 6; text: modelData.name; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                    Text { x: 9; y: 23; width: parent.width - 18; text: modelData.category + "  /  " + modelData.description; elide: Text.ElideRight; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
                    MouseArea { id: resultMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { App.setCategory(modelData.category); App.selectModule(modelData.id); root.closeRequested() } }
                }
            }
        }
    }
}
