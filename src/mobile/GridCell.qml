import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Control {
    id: root

    // we expect to be inside a TableView.delegate for a Document
    required property int documentField
    required property int row
    required property int column
    required property bool selected
    required property var textAlignment
    required property var display

    property string text
    property bool alternate: row & 1
    property color tint: "transparent"
    property int textLeftPadding: 0
    property int cellPadding: 2

    function delaySetText(delay) {
        if (delay && ((row + column) % delay !== 0) && this && delaySetText) {
            Qt.callLater(delaySetText, delay - 1)
        } else if (loaderText.status === Loader.Ready) {
            loaderText.item.text = Qt.binding(() => root.text)
        }
    }

    TableView.onPooled: {
        if (loaderText.status === Loader.Ready)
            loaderText.item.text = ""
        //console.log("POOLED")
    }
    TableView.onReused: {
        if (loaderText.status === Loader.Ready)
            Qt.callLater(delaySetText, 6)
        //console.log("REUSED")
    }
    //Component.onCompleted: console.log("NEW")

    Loader {
        id: loaderText
        anchors.fill: parent
        asynchronous: true
//        active: root.text !== ""
        sourceComponent: Component {

            Label {
                id: label
                text: root.text

                horizontalAlignment: textAlignment & 7

                clip: true
                leftPadding: root.cellPadding + root.textLeftPadding
                rightPadding: root.cellPadding + 1
                bottomPadding: 1
                topPadding: 0
                verticalAlignment: Text.AlignVCenter
                fontSizeMode: Text.Fit
                maximumLineCount: 2
                elide: Text.ElideRight
                wrapMode: Text.Wrap

                color: selected ? Material.primaryHighlightedTextColor : Material.foreground
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: -1
        color: {
            let c = selected ? Material.accentColor : Material.background
            if (alternate)
                c = (Material.theme === Material.Dark) ? Qt.lighter(c, 1.2) : Qt.darker(c, 1.1)
            return Qt.tint(c, tint)
        }
    }

    Rectangle {
        visible: root.row !== 0
        z: 1
        antialiasing: true
        height: 1 / Screen.devicePixelRatio
        color: Material.hintTextColor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
    }
    Rectangle {
        z: 1
        antialiasing: true
        width: 1 / Screen.devicePixelRatio
        color: Material.hintTextColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }
}
