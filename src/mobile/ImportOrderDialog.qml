import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import BrickStore as BS
import "./uihelpers" as UIHelpers


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
            Label {
                Layout.fillWidth: true
                font.pointSize: root.font.pointSize * 1.3
                minimumPointSize: font.pointSize / 2
                fontSizeMode: Text.Fit
                text: root.title
                elide: Label.ElideLeft
                horizontalAlignment: Qt.AlignLeft
            }
            ToolButton {
                icon.name: "view-refresh"
                enabled: BS.BrickLink.orders.updateStatus !== BS.BrickLink.Updating
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
                        BS.BrickLink.orders.startUpdate(updateLastNDays)
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: table
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true

            ScrollIndicator.vertical: ScrollIndicator { active: true }

            model: BS.SortFilterProxyModel {
                id: sortFilterModel
                sourceModel: BS.BrickLink.orders
                sortOrder: Qt.DescendingOrder
                sortColumn: 0
                filterSyntax: BS.SortFilterProxyModel.FixedString
                filterString: receivedOrPlaced.currentIndex ? "Placed" : "Received"

                Component.onCompleted: {
                    filterRoleName = "type"
                    sortRoleName = "date"
                }
            }

            delegate: ItemDelegate {
                id: delegate
                property int xspacing: 16
                width: ListView.view.width
                height: layout.height + xspacing

                required property BS.Order order
                required property int index

                GridLayout {
                    id: layout
                    x: xspacing
                    y: xspacing / 2
                    width: parent.width - 2 * xspacing
                    columnSpacing: xspacing
                    rowSpacing: xspacing / 2
                    columns: 3

                    Image {
                        id: flag
                        Layout.rowSpan: 3
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                        asynchronous: true
                        source: "qrc:/assets/flags/" + order.countryCode
                        fillMode: Image.PreserveAspectFit
                        sourceSize.height: fm.height * .75
                        sourceSize.width: fm.height * 1.5
                        FontMetrics {
                            id: fm
                            font: delegate.font
                        }
                        RectangularGlow {
                            z: -1
                            anchors.fill: parent
                            color: label.color
                            cornerRadius: 4
                            glowRadius: 4
                            spread: 0
                        }
                    }

                    Label {
                        id: label
                        property string name: order.address.split("\n",1)[0]
                        text: order.otherParty + (name === '' ? '' : ' (' + name + ')')
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        Layout.fillWidth: true
                    }
                    Label {
                        text: order.date.toLocaleDateString(Locale.ShortFormat)
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: "#" + order.id
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: order.statusAsString()
                        font.bold: true
                        color: Qt.hsla(order.status/ BS.BrickLink.OrderStatus.Count, .8, .5)
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: qsTr("%1 items (%2 lots)")
                        .arg(Number(order.itemCount).toLocaleString())
                        .arg(Number(order.lotCount).toLocaleString())
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: BS.Currency.format(order.grandTotal, order.currencyCode, 2)
                        Layout.alignment: Qt.AlignRight
                    }
                }
                Rectangle {
                    z: 1
                    antialiasing: true
                    height: 1 / Screen.devicePixelRatio
                    color: "grey"
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.rightMargin: parent.xspacing / 2
                    anchors.leftMargin: anchors.rightMargin

                }
                onClicked: {
                    BS.BrickStore.importBrickLinkOrder(order)
                    root.goBackFunction()
                }
            }
        }
    }

    footer: TabBar {
        id: receivedOrPlaced

        TabButton { text: qsTr("Received") }
        TabButton { text: qsTr("Placed") }

        onCurrentIndexChanged: table.forceLayout()
    }
}
