import QtQuick

Flickable {
    id: root
    property var modules: []
    property string selectedId: ""
    signal moduleSelected(string id)
    clip: true
    contentHeight: grid.height
    boundsBehavior: Flickable.StopAtBounds

    Grid {
        id: grid
        width: parent.width
        columns: 2
        columnSpacing: 8
        rowSpacing: 8
        Repeater {
            model: root.modules
            ModuleCard {
                required property var modelData
                width: (grid.width - grid.columnSpacing) / 2
                moduleData: modelData
                selected: modelData.id === root.selectedId
                onClicked: root.moduleSelected(modelData.id)
            }
        }
    }
}
