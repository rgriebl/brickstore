import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Qt.labs.qmlmodels
import "./uihelpers" as UIHelpers
import BrickStore

Page {
    id: root
    title: qsTr("Import Order")

    property var goBackFunction

    property int updateLastNDays: 60

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
                enabled: BrickLink.orders.updateStatus !== BrickLink.Updating
                onClicked: updateDaysDialog.open()

                UIHelpers.InputDialog {
                    id: updateDaysDialog
                    mode: "int"
                    text: qsTr("Synchronize the orders of the last") + " n " + qsTr("days")
                    intValue: root.updateLastNDays
                    intMinimum: 1
                    intMaximum: 180

                    onAccepted: {
                        root.updateLastNDays = intValue
                        BrickLink.orders.startUpdate(updateLastNDays)
                    }
                }
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HorizontalHeaderView {
            id: header
            syncView: table
            clip: true
            reuseItems: false
            interactive: false

//            property bool sorted: document.model.sorted
//            property var sortColumns: document.model.sortColumns

//            ViewHeaderMenu {
//                id: headerMenu
//                document: root.document
//            }

            delegate: Control {
                FontMetrics { id: fm }

                implicitWidth: 20
                implicitHeight: fm.height * 1.5
                Text {
                    id: headerText
                    anchors.fill: parent
                    text: model.display
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: enabled ? root.Material.foreground : root.Material.hintTextColor
                    fontSizeMode: Text.HorizontalFit
                    minimumPointSize: font.pointSize / 4
                    elide: Text.ElideMiddle
                    clip: true
                    padding: 2
                }
                Rectangle {
                    z: 1
                    antialiasing: true
                    height: 1 / Screen.devicePixelRatio
                    color: Material.hintTextColor
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
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
        }

        TableView {
            id: table
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true

            ScrollIndicator.vertical: ScrollIndicator { }

            model: BrickLink.orders

            columnSpacing: 0
            rowSpacing: 0
            reuseItems: true

            contentWidth: width

            FontMetrics { id: fontMetrics; font: root.font }
            property int cellHeight: fontMetrics.height * 2 + 8
            property var cellWidths: [ 10, 0, 10, 10, 35, 10, 10, 15 ]

            columnWidthProvider: (c) => {
                                     return cellWidths[c] * table.contentWidth / 100
                                 }
            rowHeightProvider: (r) => {
                                   return ((receivedOrPlaced.currentIndex === 0)
                                           === (model.order(r).type === BrickLink.OrderType.Received)) ? cellHeight : 0
                               }


            delegate: DelegateChooser {
                id: chooser
                role: "column"

                DelegateChoice {
                    GridCell {
                        text: display
                    }
                }
            }

        }


//        delegate: ItemDelegate {
//            width: ListView.view.width
//            text: model.date + " " + model.otherParty
//            visible: (receivedOrPlaced.currentIndex === 0) === (model.type === BrickLink.Received)
//            onClicked: {
//                BrickStore.importBrickLinkOrder(model.order)
//                goBackFunction()
//            }
//        }
    }

    footer: TabBar {
        id: receivedOrPlaced

        TabButton { text: qsTr("Received") }
        TabButton { text: qsTr("Placed") }

        onCurrentIndexChanged: table.forceLayout()
    }
}
