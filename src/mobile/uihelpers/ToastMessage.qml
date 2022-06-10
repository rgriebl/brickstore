import QtQuick
import QtQuick.Controls

Popup {
    id: root
    property string message
    property int timeout

    closePolicy: Popup.NoAutoClose

    anchors.centerIn: Overlay.overlay

    leftMargin: Overlay.overlay ? Overlay.overlay.width / 5 : 0
    rightMargin: Overlay.overlay ? Overlay.overlay.width / 5 : 0
    bottomMargin: Overlay.overlay ? Overlay.overlay.height / 5 : 0
    topMargin: Overlay.overlay ? Overlay.overlay.height * 4 / 5 : 0

    leftPadding: fm.height + 4
    topPadding: 4
    bottomPadding: 4
    rightPadding: fm.height + 4

    contentItem: Label {
        id: label
        text: root.message
        wrapMode: Text.Wrap
        font.bold: true

        FontMetrics {
            id: fm
            font: label.font
        }
    }

    background: Rectangle {
        radius: height / 2
        color: "gray"
        MouseArea {
            z: 1000
            anchors.fill: parent
            onClicked: { console.log("WTF?", root.close()) }
        }
    }

    Timer {
        interval: root.timeout
        running: true
        onTriggered: root.close()
    }
}
