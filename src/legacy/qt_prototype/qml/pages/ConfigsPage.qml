import QtQuick
import "../theme"
import "../components"

Item {
    id: root
    property int selectedId: -1
    property var selectedConfig: ({})
    property string tab: "Public"
    property bool publishMode: false
    property int editingId: 0
    property int pendingDelete: 0
    TopBar { id: bar; width: parent.width; title: "Configs"; subtitle: "Browse and apply community setups" }
    Rectangle {
        x: parent.width - 104; y: 10; width: 91; height: 32; radius: 8; color: Theme.accent
        Text { anchors.centerIn: parent; text: root.publishMode ? "Close" : "Publish"; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { root.publishMode = !root.publishMode; root.editingId = 0; if (root.publishMode) { configName.text = ""; configDescription.text = "" } } }
    }
    Row {
        x: 14; y: Theme.topBarHeight + 8; width: parent.width - 28; height: parent.height - y - 14; spacing: 8
        GlassSurface {
            width: parent.width * 0.43; height: parent.height; surfaceColor: "#d8111016"; surfaceRadius: 12
            Row {
                x: 10; y: 10; spacing: 4
                Repeater { model: ["My", "Favourite", "Public"]; Rectangle { required property string modelData; width: 69; height: 27; radius: 7; color: root.tab === modelData ? Theme.accent : "#1affffff"; Text { anchors.centerIn: parent; text: modelData; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.tab = modelData } } }
            }
            Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 17; text: "refresh"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9; MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: App.refreshConfigs() } }
            Rectangle { x: 10; y: 47; width: parent.width - 20; height: 34; radius: 8; color: "#242129"; TextInput { id: configSearch; x: 10; width: parent.width - 20; height: parent.height; verticalAlignment: TextInput.AlignVCenter; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }; Text { anchors.fill: parent; anchors.leftMargin: 10; verticalAlignment: Text.AlignVCenter; visible: !configSearch.text.length; text: "Search configs"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 } }
            ListView {
                x: 10; y: 89; width: parent.width - 20; height: parent.height - 99; spacing: 5; clip: true; model: App.configs
                delegate: ConfigCard {
                    required property var modelData
                    visible: (!configSearch.text.length || modelData.name.toLowerCase().includes(configSearch.text.toLowerCase())) && (root.tab === "Public" || (root.tab === "My" && modelData.mine) || (root.tab === "Favourite" && modelData.favourite))
                    height: visible ? 76 : 0; configData: modelData; selected: root.selectedId === modelData.id
                    onSelectedRequested: { root.selectedId = modelData.id; root.selectedConfig = modelData }
                    onFavouriteRequested: App.toggleFavourite(modelData.id)
                }
            }
        }
        GlassSurface {
            width: parent.width - parent.children[0].width - parent.spacing; height: parent.height; surfaceColor: "#d8111016"; surfaceRadius: 12
            Item {
                anchors.fill: parent; visible: root.selectedId < 0 && !root.publishMode
                Text { anchors.centerIn: parent; text: "Select a config\nChoose an entry to inspect or load it."; horizontalAlignment: Text.AlignHCenter; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 12; lineHeight: 1.6 }
            }
            Item {
                anchors.fill: parent; visible: root.selectedId >= 0 && !root.publishMode
                Text { x: 18; y: 18; width: parent.width - 36; text: root.selectedConfig.name || "Config"; color: Theme.primaryText; elide: Text.ElideRight; font.family: Theme.fontFamily; font.pixelSize: 18; font.bold: true }
                Text { x: 18; y: 47; text: "by " + (root.selectedConfig.author || "Unknown"); color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
                Rectangle { x: 18; y: 76; width: parent.width - 36; height: 1; color: Theme.separator }
                Text { x: 18; y: 94; text: (root.selectedConfig.modules || 0) + " modules"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                Text { x: 18; y: 118; text: (root.selectedConfig.downloads || 0) + " community loads"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                Text { x: 18; y: 151; width: parent.width - 36; wrapMode: Text.WordWrap; text: "A public Thedorius setup. Module states, keybinds and typed settings are applied together."; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11; lineHeight: 1.4 }
                Row {
                    x: 18; y: parent.height - 55; spacing: 7
                    Rectangle { width: 92; height: 34; radius: 8; color: Theme.accent; Text { anchors.centerIn: parent; text: "Load"; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.loadConfig(root.selectedId) } }
                    Rectangle { width: 92; height: 34; radius: 8; color: "#242129"; Text { anchors.centerIn: parent; text: root.selectedConfig.favourite ? "Unfavourite" : "Favourite"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 10 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.toggleFavourite(root.selectedId) } }
                    Rectangle { visible: root.selectedConfig.mine || false; width: 58; height: 34; radius: 8; color: "#242129"; Text { anchors.centerIn: parent; text: "Edit"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 10 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { root.editingId = root.selectedId; configName.text = root.selectedConfig.name || ""; configDescription.text = root.selectedConfig.description || ""; root.publishMode = true } } }
                    Rectangle { visible: root.selectedConfig.mine || false; width: 62; height: 34; radius: 8; color: Qt.rgba(0.88, 0.36, 0.44, 0.14); Text { anchors.centerIn: parent; text: "Delete"; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: 10 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.pendingDelete = root.selectedId } }
                }
            }
            Item {
                anchors.fill: parent; visible: root.publishMode
                Text { x: 18; y: 18; text: root.editingId > 0 ? "Edit config" : "Publish current setup"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 17; font.bold: true }
                Text { x: 18; y: 46; text: "Module states, keybinds and typed settings are included."; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
                Rectangle { x: 18; y: 76; width: parent.width - 36; height: 38; radius: 8; color: "#242129"; TextInput { id: configName; x: 11; width: parent.width - 22; height: parent.height; verticalAlignment: TextInput.AlignVCenter; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }; Text { anchors.fill: parent; anchors.leftMargin: 11; verticalAlignment: Text.AlignVCenter; visible: !configName.text.length; text: "Config name"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 } }
                Rectangle { x: 18; y: 124; width: parent.width - 36; height: 112; radius: 8; color: "#242129"; TextEdit { id: configDescription; x: 11; y: 9; width: parent.width - 22; height: parent.height - 18; color: Theme.primaryText; selectionColor: Theme.accent; wrapMode: TextEdit.Wrap; font.family: Theme.fontFamily; font.pixelSize: 11 }; Text { x: 11; y: 9; visible: !configDescription.text.length; text: "Short description"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 11 } }
                Rectangle { x: 18; y: 250; width: 124; height: 34; radius: 8; color: configName.text.trim().length ? Theme.accent : "#302d36"; Text { anchors.centerIn: parent; text: root.editingId > 0 ? "Save changes" : "Publish publicly"; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }; MouseArea { anchors.fill: parent; enabled: configName.text.trim().length > 0; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { App.publishConfig(root.editingId, configName.text, configDescription.text, "Thessaryn"); root.publishMode = false; root.editingId = 0 } } }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; visible: root.pendingDelete > 0; color: "#82000000"; radius: 12
        Rectangle {
            width: 300; height: 148; radius: 13; anchors.centerIn: parent; color: "#fa19171e"; border.width: 1; border.color: Theme.border
            Text { x: 18; y: 18; text: "Delete this config?"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 15; font.bold: true }
            Text { x: 18; y: 50; width: parent.width - 36; text: "This removes the public entry. The action cannot be undone."; wrapMode: Text.WordWrap; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
            Row { anchors.right: parent.right; anchors.rightMargin: 16; anchors.bottom: parent.bottom; anchors.bottomMargin: 15; spacing: 7
                Rectangle { width: 76; height: 32; radius: 8; color: "#242129"; Text { anchors.centerIn: parent; text: "Cancel"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 10 }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.pendingDelete = 0 } }
                Rectangle { width: 76; height: 32; radius: 8; color: Theme.danger; Text { anchors.centerIn: parent; text: "Delete"; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }; MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { App.deleteConfig(root.pendingDelete); root.pendingDelete = 0; root.selectedId = -1; root.selectedConfig = ({}) } } }
            }
        }
    }
}
