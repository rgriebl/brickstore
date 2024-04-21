import QtQuick
import QtQuick.Controls
import QtMultimedia

VideoOutput {
    id: root
    focus: true

    implicitWidth: 320
    implicitHeight: 180

    property bool active: true
    property int progress: 0
    signal clicked()

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "#80000000"
        opacity: root.active ? 0 : 1

        Image {
            id: play
            anchors.centerIn: parent
            source: "media-playback-start.svg"
            property int size: Math.min(root.width, root.height) / 3
            sourceSize.width: size
            sourceSize.height: size
        }
    }

    ProgressBar {
        id: progress
        anchors.centerIn: parent
        width: parent.width * 0.8
        visible: root.active && root.progress
        value: root.progress / 100
        onValueChanged: { console.log("V",value,root.progress)}
    }

    MouseArea {
        anchors.fill: parent
        onClicked: if (root.active && !root.progress) root.clicked()
    }

    Keys.onSpacePressed: if (root.active && !root.progress) root.clicked()
}
