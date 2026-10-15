import QtQuick
import "../theme"

GlassSurface {
    id: root
    property var moduleData: ({})
    property bool opened: moduleData && moduleData.id !== undefined
    signal closeRequested()
    width: Theme.settingsWidth
    height: opened ? Math.min(548, 102 + settingsColumn.height) : 56
    opacity: opened ? 1 : 0
    scale: opened ? 1 : 0.97
    visible: opacity > 0.01
    surfaceColor: Theme.settingsPanel
    surfaceRadius: Theme.panelRadius
    clip: true
    Behavior on height { NumberAnimation { duration: Theme.panelDuration; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: Theme.normalDuration } }
    Behavior on scale { NumberAnimation { duration: Theme.normalDuration; easing.type: Easing.OutCubic } }

    Text {
        x: 15; y: 15; width: parent.width - 58
        text: root.opened ? root.moduleData.name : ""
        color: Theme.primaryText; elide: Text.ElideRight
        font.family: Theme.fontFamily; font.pixelSize: 16; font.bold: true
    }
    Rectangle {
        width: 28; height: 28; radius: 14; anchors.right: parent.right; anchors.rightMargin: 11; y: 10
        color: closeMouse.containsMouse ? "#20ffffff" : "#12ffffff"
        Text { anchors.centerIn: parent; text: "x"; color: Theme.secondaryText; font.pixelSize: 15; font.bold: true }
        MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.closeRequested() }
    }
    Rectangle { x: 15; y: 50; width: parent.width - 30; height: 1; color: Theme.separator }

    Flickable {
        x: 15; y: 60; width: parent.width - 30; height: Math.min(contentHeight, root.height - 72)
        contentHeight: settingsColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: settingsColumn
            width: parent.width
            spacing: 6

            Item {
                width: parent.width; height: 36
                Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Enabled"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                AppToggle {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    checked: root.opened && root.moduleData.enabled
                    onToggled: value => App.setModuleEnabled(root.moduleData.id, value)
                }
            }
            Item {
                width: parent.width; height: 34
                Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Keybind"; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12 }
                Rectangle {
                    width: 78; height: 27; radius: 7; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    color: "#25222b"; border.width: 1; border.color: Theme.border
                    Text { anchors.centerIn: parent; text: root.opened ? root.moduleData.keybind : "NONE"; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: if (root.opened) App.beginKeybind(root.moduleData.id) }
                }
            }

            Repeater {
                model: root.opened ? root.moduleData.settings : []
                Item {
                    id: settingRoot
                    required property var modelData
                    property bool expanded: false
                    width: settingsColumn.width
                    height: {
                        if (modelData.type === "float" || modelData.type === "int") return 52
                        if (modelData.type === "range") return 52
                        if (modelData.type === "enum") return 36 + (expanded ? modelData.options.length * 29 + 5 : 0)
                        if (modelData.type === "color") return 36 + (expanded ? 298 : 0)
                        return 36
                    }
                    clip: true
                    Behavior on height { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

                    Item {
                        anchors.fill: parent
                        visible: modelData.type === "bool"
                        Text { anchors.left: parent.left; anchors.verticalCenter: parent.top; anchors.verticalCenterOffset: 18; text: modelData.label; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12 }
                        AppToggle { anchors.right: parent.right; y: 7; checked: !!modelData.value; onToggled: value => App.setSetting(root.moduleData.id, modelData.id, value) }
                    }
                    AppSlider {
                        width: parent.width
                        visible: modelData.type === "float" || modelData.type === "int"
                        label: modelData.label; value: Number(modelData.value); minimum: modelData.minimum; maximum: modelData.maximum
                        unit: modelData.unit; integral: modelData.type === "int"
                        onEdited: value => App.setSetting(root.moduleData.id, modelData.id, value)
                    }
                    RangeSlider {
                        width: parent.width
                        visible: modelData.type === "range"
                        label: modelData.label; low: modelData.value && modelData.value.length ? modelData.value[0] : 0
                        high: modelData.value && modelData.value.length > 1 ? modelData.value[1] : 1
                        minimum: modelData.minimum; maximum: modelData.maximum; unit: modelData.unit
                        integral: modelData.minimum % 1 === 0 && modelData.maximum % 1 === 0
                        onEdited: (low, high) => App.setSetting(root.moduleData.id, modelData.id, [low, high])
                    }
                    Column {
                        width: parent.width; spacing: 2
                        visible: modelData.type === "enum"
                        Rectangle {
                            width: parent.width; height: 34; radius: 8; color: "#211f27"; border.width: 1; border.color: settingRoot.expanded ? "#509b45ff" : Theme.border
                            Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12 }
                            Text { anchors.right: parent.right; anchors.rightMargin: 25; anchors.verticalCenter: parent.verticalCenter; text: modelData.options[Number(modelData.value)] || ""; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                            Text { anchors.right: parent.right; anchors.rightMargin: 9; anchors.verticalCenter: parent.verticalCenter; text: settingRoot.expanded ? "^" : "v"; color: Theme.accentHover; font.family: Theme.fontFamily; font.pixelSize: 10; font.bold: true }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: settingRoot.expanded = !settingRoot.expanded }
                        }
                        Repeater {
                            model: settingRoot.expanded ? modelData.options : []
                            Rectangle {
                                required property var modelData
                                required property int index
                                width: settingRoot.width; height: 27; radius: 6
                                color: index === Number(settingRoot.modelData.value) ? "#459b45ff" : (optionMouse.containsMouse ? "#18ffffff" : "transparent")
                                Text { x: 9; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 11 }
                                MouseArea { id: optionMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { App.setSetting(root.moduleData.id, settingRoot.modelData.id, index); settingRoot.expanded = false } }
                            }
                        }
                    }
                    Column {
                        width: parent.width; spacing: 5
                        visible: modelData.type === "color"
                        Rectangle {
                            width: parent.width; height: 34; radius: 8; color: "#211f27"; border.width: 1; border.color: Theme.border
                            Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12 }
                            Rectangle { width: 18; height: 18; radius: 5; anchors.right: parent.right; anchors.rightMargin: 9; anchors.verticalCenter: parent.verticalCenter; color: modelData.value }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: settingRoot.expanded = !settingRoot.expanded }
                        }
                        ColorEditor {
                            width: parent.width
                            visible: settingRoot.expanded
                            selectedColor: modelData.value
                            onColorEdited: value => App.setSetting(root.moduleData.id, modelData.id, value.toString())
                        }
                    }
                    Rectangle {
                        anchors.fill: parent; visible: modelData.type === "action"; radius: 8; color: "#211f27"; border.width: 1; border.color: Theme.border
                        Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.secondaryText; font.family: Theme.fontFamily; font.pixelSize: 12 }
                        Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: Theme.primaryText; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.beginSettingKey(root.moduleData.id, modelData.id) }
                    }
                }
            }
        }
    }
}
