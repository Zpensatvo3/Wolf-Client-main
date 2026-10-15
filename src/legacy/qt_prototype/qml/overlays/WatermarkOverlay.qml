import QtQuick
import "../theme"

Rectangle {
    id: root
    property string playerName: "Thessaryn"
    property int ping: 84
    width: 258; height: 42; radius: 12; color: "#e80b0b10"; border.width: 1; border.color: "#18ffffff"
    Row {
        anchors.centerIn: parent; spacing: 11
        Text { text: "Thedorius"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 14; font.bold: true }
        Rectangle { width: 1; height: 18; color: "#36ffffff" }
        Text { text: "person  " + root.playerName; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold }
        Rectangle { width: 1; height: 18; color: "#36ffffff" }
        Text { text: "signal  " + root.ping + " ms"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold }
    }
}
