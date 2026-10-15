import QtQuick
import "../theme"
import "../components"

Item {
    id: root
    property string playerFilter: ""
    TopBar { id: bar; width: parent.width; title: "Friends"; subtitle: "Trusted players for this session" }
    Row {
        x: 14; y: Theme.topBarHeight + 8; width: parent.width - 28; height: parent.height - y - 14; spacing: 8
        GlassSurface {
            width: parent.width * 0.57; height: parent.height; surfaceColor: "#d8111016"; surfaceRadius: 12
            Text { x: 12; y: 11; text: "Server players"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
            Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 11; text: App.serverPlayers.length + " online"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
            Rectangle {
                x: 10; y: 36; width: parent.width - 20; height: 34; radius: 8; color: "#242129"
                TextInput { id: playerSearch; x: 10; width: parent.width - 20; height: parent.height; verticalAlignment: TextInput.AlignVCenter; color: Theme.primaryText; selectionColor: Theme.accent; font.family: Theme.fontFamily; font.pixelSize: 11; onTextChanged: root.playerFilter = text }
                Text { anchors.fill: parent; anchors.leftMargin: 10; verticalAlignment: Text.AlignVCenter; visible: !playerSearch.text.length; text: "Search players"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 }
            }
            ListView {
                x: 10; y: 78; width: parent.width - 20; height: parent.height - 88; spacing: 4; clip: true
                model: App.serverPlayers
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: visible ? 42 : 0; visible: !root.playerFilter.length || modelData.name.toLowerCase().includes(root.playerFilter.toLowerCase())
                    radius: 8; color: rowMouse.containsMouse ? Theme.cardHover : Theme.card
                    Text { x: 11; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 92; text: modelData.name; color: Theme.primaryText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Text { anchors.right: action.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.local ? "YOU" : (modelData.friend ? "FRIEND" : "ONLINE"); color: modelData.friend ? Theme.accentHover : Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 8 }
                    Rectangle { id: action; anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; width: 27; height: 27; radius: 7; color: "#1cffffff"; Text { anchors.centerIn: parent; text: modelData.friend ? "-" : "+"; color: Theme.primaryText; font.pixelSize: 15 }; MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: !modelData.local; onClicked: modelData.friend ? App.removeFriend(modelData.name) : App.addFriend(modelData.name) } }
                }
            }
        }
        GlassSurface {
            width: parent.width - parent.children[0].width - parent.spacing; height: parent.height; surfaceColor: "#d8111016"; surfaceRadius: 12
            Text { x: 12; y: 11; text: "Friends"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
            Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 11; text: App.friends.length; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
            Rectangle {
                x: 10; y: 36; width: parent.width - 20; height: 34; radius: 8; color: "#242129"
                TextInput { id: friendInput; x: 10; width: parent.width - 48; height: parent.height; verticalAlignment: TextInput.AlignVCenter; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                Text { anchors.fill: friendInput; verticalAlignment: Text.AlignVCenter; visible: !friendInput.text.length; text: "Player name"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                Rectangle { anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; width: 28; height: 27; radius: 7; color: Theme.accent; Text { anchors.centerIn: parent; text: "+"; color: "white"; font.pixelSize: 16 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { App.addFriend(friendInput.text); friendInput.clear() } } }
            }
            ListView {
                x: 10; y: 78; width: parent.width - 20; height: parent.height - 88; spacing: 4; clip: true; model: App.friends
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 42; radius: 8; color: Theme.card
                    Rectangle { x: 10; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: modelData.online ? "#62d39b" : Theme.mutedText }
                    Text { x: 27; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 70; text: modelData.name; color: Theme.primaryText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Rectangle { anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; width: 27; height: 27; radius: 7; color: "#1cffffff"; Text { anchors.centerIn: parent; text: "-"; color: Theme.danger; font.pixelSize: 15 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.removeFriend(modelData.name) } }
                }
            }
        }
    }
}
