import QtQuick
import "../theme"

Item {
    id: root
    property string currentCategory: "Combat"
    property string currentPage: "modules"
    signal categorySelected(string category)
    signal pageSelected(string page)
    width: Theme.sidebarWidth

    AppLogo { x: 18; y: 17; logoSize: 48 }
    Text { x: 76; y: 20; text: "Thedorius"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 16; font.bold: true }
    Text { x: 76; y: 42; text: "Version 0.3"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 10 }
    Text { x: 20; y: 87; text: "FEATURES"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }

    Column {
        x: 20; y: 104; spacing: 4
        Repeater {
            model: App.categories
            SidebarItem {
                required property var modelData
                label: modelData.label
                iconText: modelData.id === "Combat" ? "\uf05b" : modelData.id === "Movement" ? "\uf70c" : modelData.id === "Player" ? "\uf007" : modelData.id === "Visuals" ? "\uf06e" : "\uf1de"
                selected: root.currentPage === "modules" && root.currentCategory === modelData.id
                onClicked: root.categorySelected(modelData.id)
            }
        }
    }

    Text { x: 20; y: 314; text: "OTHER"; color: Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }
    Column {
        x: 20; y: 331; spacing: 4
        SidebarItem { label: "Friends"; iconText: "\uf0c0"; selected: root.currentPage === "friends"; onClicked: root.pageSelected("friends") }
        SidebarItem { label: "Configs"; iconText: "\uf07c"; selected: root.currentPage === "configs"; onClicked: root.pageSelected("configs") }
        SidebarItem { label: "Hud"; iconText: "\uf390"; selected: root.currentPage === "hud"; onClicked: root.pageSelected("hud") }
        SidebarItem { label: "Launch"; iconText: "\uf135"; selected: root.currentPage === "launch"; onClicked: root.pageSelected("launch") }
    }

    Item {
        x: 20; width: 164; height: 54; anchors.bottom: parent.bottom; anchors.bottomMargin: 17
        Rectangle { anchors.fill: parent; radius: 10; color: "#16000000" }
        Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 34; height: 34; radius: 10; color: "#20ffffff"; Text { anchors.centerIn: parent; text: "T"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 14; font.bold: true } }
        Text { x: 52; y: 11; text: "Thessaryn"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
        Text { x: 52; y: 30; text: App.connected ? "SESSION ACTIVE" : "OFFLINE"; color: App.connected ? Theme.accentHover : Theme.mutedText; font.family: Theme.fontFamily; font.pixelSize: 9 }
    }
}
