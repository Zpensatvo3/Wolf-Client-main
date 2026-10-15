import QtQuick
import "../theme"
import "../components"

Item {
    TopBar { id: bar; width: parent.width; title: "Hud"; subtitle: "Preview the lightweight game overlay" }
    HudPreviewPanel { x: 14; y: Theme.topBarHeight + 8; width: parent.width - 28; height: parent.height - y - 14 }
}
