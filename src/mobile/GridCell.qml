import QtQuick
import QtQuick.Controls
import BrickStore as BS

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

                color: selected ? Style.accentTextColor : Style.textColor
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: -1
        color: {
            let c = selected ? Style.accentColor : Style.backgroundColor
            if (alternate)
                c = BS.Utililty.contrastColor(c, 0.1)
            return c
        }
    }

    Rectangle {
        visible: root.row !== 0
        z: 1
        antialiasing: true
        height: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
    }
    Rectangle {
        z: 1
        antialiasing: true
        width: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }

    // we cannot handle taps directly in the TableView thanks to QTBUG-101386
    MouseArea {
        anchors.fill: parent
        onClicked: parent.TableView.view.toggleSelection(parent.row, parent.column)
        onDoubleClicked: {
            parent.TableView.view.toggleSelection(parent.row, parent.column)
            parent.TableView.view.showMenu(parent.row, parent.column)
        }
    }
//  TapHandler {
//        grabPermissions: PointerHandler.TakeOverForbidden
//        onDoubleTapped: parent.TableView.view.showMenu(parent.row, parent.column)
//        onTapped:
//    }
}
