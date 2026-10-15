import QtQuick
import "../theme"

Rectangle {
    id: root
    property color surfaceColor: Theme.card
    property real surfaceRadius: Theme.cardRadius
    property bool outlined: true
    color: surfaceColor
    radius: surfaceRadius
    border.width: outlined ? 1 : 0
    border.color: Theme.border
}
