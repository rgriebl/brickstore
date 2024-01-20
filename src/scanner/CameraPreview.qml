import QtQuick
import QtMultimedia

VideoOutput {
    id: root
    focus: true

    implicitWidth: 320
    implicitHeight: 180

    property bool active: false
    signal clicked()

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "#80000000"
        opacity: root.active ? 0 : 1

        Image {
            id: play
            anchors.centerIn: parent
            source: "./media-playback-start"
            property int size: Math.min(root.width, root.height) / 3
            sourceSize.width: size
            sourceSize.height: size
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: if (root.active) root.clicked()
    }

    Keys.onSpacePressed: if (root.active) root.clicked()
}
