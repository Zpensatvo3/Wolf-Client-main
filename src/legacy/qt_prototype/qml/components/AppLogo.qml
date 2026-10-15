import QtQuick
import "../theme"

Item {
    id: root
    property real logoSize: 48
    implicitWidth: logoSize
    implicitHeight: logoSize

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#24000000"
        border.width: 1
        border.color: "#24ffffff"
    }
    Image {
        anchors.fill: parent
        anchors.margins: 3
        source: "../assets/logo/20-thedorius-logo.png"
        sourceClipRect: Qt.rect(285, 205, 690, 750)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }
}
