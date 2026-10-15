import QtQuick
import "theme"
import "components"
import "pages"
import "overlays"

Window {
    id: window
    x: App.mockMode ? 60 : App.targetRect.x
    y: App.mockMode ? 40 : App.targetRect.y
    width: App.mockMode ? 1280 : Math.max(900, App.targetRect.width)
    height: App.mockMode ? 760 : Math.max(620, App.targetRect.height)
    minimumWidth: 900; minimumHeight: 620
    visible: App.mockMode || App.targetVisible; color: "transparent"; title: "Thedorius"
    flags: Qt.FramelessWindowHint | Qt.Window | (App.menuVisible ? 0 : Qt.WindowTransparentForInput)
    Connections { target: App; function onMenuVisibleChanged() { if (App.menuVisible) window.requestActivate() } }

    BackgroundLayer { anchors.fill: parent; visible: App.menuVisible; opacity: App.menuVisible ? 1 : 0; Behavior on opacity { NumberAnimation { duration: 180 } } }
    Rectangle { anchors.fill: parent; visible: App.menuVisible; color: "#5c000000" }

    Item {
        id: workspace
        width: selectedPanel.opened ? 1142 : 824; height: 620
        anchors.centerIn: parent
        visible: App.menuVisible

        GlassSurface {
            id: shell
            x: 0; width: 824; height: 620; surfaceColor: Theme.window; surfaceRadius: Theme.shellRadius; clip: true
            Rectangle { width: Theme.sidebarWidth; height: parent.height; color: Theme.sidebar }
            Sidebar {
                width: Theme.sidebarWidth; height: parent.height
                currentCategory: App.activeCategory; currentPage: App.activePage
                onCategorySelected: category => App.setCategory(category)
                onPageSelected: page => App.setPage(page)
            }
            Rectangle { x: Theme.sidebarWidth; width: 1; height: parent.height; color: Theme.separator }
            Item {
                x: Theme.sidebarWidth + 1; width: parent.width - x; height: parent.height
                Loader {
                    anchors.fill: parent
                    sourceComponent: App.activePage === "friends" ? friendsPage : App.activePage === "configs" ? configsPage : App.activePage === "hud" ? hudPage : App.activePage === "launch" ? launchPage : modulesPage
                }
            }
        }
        ModuleSettingsPanel { id: selectedPanel; x: 838; y: 0; moduleData: App.selectedModule; onCloseRequested: App.closeSettings() }
        SearchPopover { id: search; x: 824 - width - 12; y: 11; onCloseRequested: opened = false }
    }

    Component { id: modulesPage; ModulesPage { onSearchRequested: search.opened = true } }
    Component { id: friendsPage; FriendsPage {} }
    Component { id: configsPage; ConfigsPage {} }
    Component { id: hudPage; HudPage {} }
    Component { id: launchPage; LaunchPage {} }

    WatermarkOverlay { x: 18; y: 18; playerName: App.playerName; ping: App.ping; visible: !App.menuVisible }
    ArrayListOverlay { anchors.right: parent.right; anchors.rightMargin: 12; y: 18; modules: App.enabledModuleNames; visible: !App.menuVisible }
    Toast { anchors.right: parent.right; anchors.rightMargin: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 18; message: App.toastText; success: App.toastSuccess }

    Shortcut { sequence: "Insert"; onActivated: App.menuVisible = !App.menuVisible }
    Shortcut { sequence: "Ctrl+K"; enabled: App.menuVisible; onActivated: search.opened = true }
    Shortcut { sequence: "Escape"; enabled: App.menuVisible; onActivated: { if (search.opened) search.opened = false; else if (App.selectedModuleId.length) App.closeSettings(); else App.menuVisible = false } }
    Component.onCompleted: {
        if (App.initialState === "settings") App.selectModule("triggerbot")
        else if (App.initialState === "search") search.opened = true
        else if (App.initialState === "visuals") App.setCategory("Visuals")
        else if (App.initialState === "configs") App.setPage("configs")
        else if (App.initialState === "friends") App.setPage("friends")
        else if (App.initialState === "hud") App.setPage("hud")
        else if (App.initialState === "closed") App.menuVisible = false
    }
}
