import QtQuick

Item {
    anchors.fill: parent

    Image {
        anchors.fill: parent
        source: "../assets/backgrounds/21-main-background.png"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#80040508"
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#16000000" }
            GradientStop { position: 0.55; color: "#05000000" }
            GradientStop { position: 1.0; color: "#78000000" }
        }
    }

    Repeater {
        model: 9
        Rectangle {
            required property int index
            width: 2 + index % 3
            height: width
            radius: width / 2
            color: "#20d7bfef"
            x: (index * 173 + drift) % Math.max(1, parent.width)
            y: (index * 97 + 60) % Math.max(1, parent.height)
            property real drift: 0
            NumberAnimation on drift {
                from: 0; to: 120; duration: 36000 + index * 2700
                loops: Animation.Infinite
            }
        }
    }
}
