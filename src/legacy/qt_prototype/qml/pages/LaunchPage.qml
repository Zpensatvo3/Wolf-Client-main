import QtQuick
import "../theme"
import "../components"

Item {
    TopBar { id: bar; width: parent.width; title: "Launch"; subtitle: "Attach to Fabric 1.21.1" }
    GlassSurface {
        x: 14; y: Theme.topBarHeight + 8; width: parent.width - 28; height: parent.height - y - 14; surfaceRadius: 12
        Text { x: 16; y: 15; text: "Detected Minecraft sessions"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold }
        Text { anchors.right: parent.right; anchors.rightMargin: 16; y: 15; text: "Scan"; color: Theme.accentHover; font.family: Theme.fontFamily; font.pixelSize: 10; MouseArea { anchors.fill: parent; anchors.margins: -7; cursorShape: Qt.PointingHandCursor; onClicked: App.scanProcesses() } }
        ListView {
            x: 14; y: 49; width: parent.width - 28; height: parent.height - 63; spacing: 5; clip: true; model: App.processes
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width; height: 54; radius: 9; color: Theme.card
                Text { x: 13; y: 9; width: parent.width - 120; text: modelData.title || "Minecraft"; color: Theme.primaryText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                Text { x: 13; y: 31; text: "PID " + modelData.pid + "  /  Fabric 1.21.1"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
                Rectangle { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: 82; height: 32; radius: 8; color: modelData.compatible ? Theme.accent : "#242129"; Text { anchors.centerIn: parent; text: modelData.compatible ? "Attach" : "Unsupported"; color: modelData.compatible ? "white" : Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }; MouseArea { anchors.fill: parent; enabled: modelData.compatible; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: App.attachToProcess(modelData.pid) } }
            }
        }
    }
    Row {
        x: 28; y: parent.height - 62; spacing: 6
        Repeater {
            model: App.launchers
            Rectangle {
                required property var modelData
                width: 126; height: 32; radius: 8; color: "#e8242129"; border.width: 1; border.color: Theme.border
                Text { anchors.centerIn: parent; text: modelData.name; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.launchGame(modelData.id, "") }
            }
        }
    }
    Component.onCompleted: App.scanProcesses()
}
