import QtQuick
import "../theme"
import "../components"

Item {
    id: root
    signal searchRequested()
    TopBar {
        id: bar; width: parent.width
        title: App.activeCategory
        subtitle: App.visibleModules.length + " modules available"
        onSearchRequested: root.searchRequested()
    }
    ModuleGrid {
        x: 14; y: Theme.topBarHeight + 6
        width: parent.width - 28; height: parent.height - y - 14
        modules: App.visibleModules
        selectedId: App.selectedModuleId
        onModuleSelected: moduleId => App.selectModule(moduleId)
    }
}
