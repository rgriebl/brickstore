import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Control {
    id: root

    // we expect to be inside a TableView.delegate
    required property int row       // from TableView
    required property int column    // from TableView
    required property bool selected // from TableView
    required property var textAlignment
    required property var display

    property string text
    property bool alternate: row & 1
    property color tint: "transparent"
    property int textLeftPadding: 0
    property int cellPadding: 2
    property bool asynchronous: true

    function delaySetText(delay) {
        if (delay && ((row + column) % delay !== 0) && this && delaySetText) {
            Qt.callLater(delaySetText, delay - 1)
        } else if (loaderText.status === Loader.Ready) {
            loaderText.item.text = Qt.binding(() => root.text)
        }
    }

    TableView.onPooled: {
        if ((loaderText.status === Loader.Ready) && root.asynchronous)
            loaderText.item.text = ""
        //console.log("POOLED")
    }
    TableView.onReused: {
        if ((loaderText.status === Loader.Ready) && root.asynchronous)
            Qt.callLater(delaySetText, 6)
        //console.log("REUSED")
    }
    //Component.onCompleted: console.log("NEW")

    implicitWidth: loaderText.implicitWidth
    implicitHeight: loaderText.implicitHeight

    Loader {
        id: loaderText
        anchors.fill: parent
        asynchronous: root.asynchronous
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

    // we cannot handle taps directly in the TableView thanks to QTBUG-101386
    TapHandler {
        onDoubleTapped: parent.TableView.view.showMenu(parent.row, parent.column)
        onTapped: parent.TableView.view.toggleSelection(parent.row, parent.column)
    }
}
