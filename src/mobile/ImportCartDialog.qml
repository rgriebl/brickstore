import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore
import "./uihelpers" as UIHelpers

Page {
    id: root
    title: qsTr("Import BrickLink Shopping Cart")

    property var goBackFunction

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-previous"
                onClicked: goBackFunction()
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                icon.name: "view-refresh"
                onClicked: BrickLink.carts.startUpdate()
                enabled: BrickLink.carts.updateStatus !== BrickLink.Updating
            }
        }
        Label {
            anchors.centerIn: parent
            scale: 1.3
            text: root.title
            elide: Label.ElideLeft
            horizontalAlignment: Qt.AlignHCenter
        }
    }

    ListView {
        anchors.fill: parent
        clip: true

        ScrollIndicator.vertical: ScrollIndicator { }

        model: BrickLink.carts
        delegate: ItemDelegate {
            width: ListView.view.width
            text: model.date + " " + model.store
            onClicked: {
                BrickLink.carts.startFetchLots(model.cart)
                // initBrickLink in application.cpp reacts on fetchLotsFinished() and opens the documents
                goBackFunction()
            }
        }
    }
    Component.onCompleted: {
        Qt.callLater(function() { BrickLink.carts.startUpdate() })
    }
}
