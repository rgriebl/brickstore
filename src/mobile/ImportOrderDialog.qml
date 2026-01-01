// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import Qt5Compat.GraphicalEffects
import BrickStore as BS
import BrickLink as BL


FullscreenDialog {
    id: root
    title: qsTr("Import Order")

    property int updateLastNDays: 60

    property var goBackFunction
    onBackClicked: goBackFunction()

    BS.ExtraConfig {
        category: "ImportOrderDialog"

        property alias updateLastNDays: root.updateLastNDays
        property alias receivedOrPlaced: receivedOrPlaced.currentIndex
    }

    toolButtons: ToolButton {
        icon.name: "view-refresh"
        enabled: BL.BrickLink.orders.updateStatus !== BL.BrickLink.Updating
        onClicked: updateDaysDialog.open()

        InputDialog {
            id: updateDaysDialog
            mode: "int"
            text: qsTr("Synchronize the orders of the last") + " n " + qsTr("days")
            intValue: root.updateLastNDays
            intMinimum: 1
            intMaximum: 180

            onAccepted: {
                root.updateLastNDays = intValue
                BL.BrickLink.orders.startUpdate(root.updateLastNDays)
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

            ScrollIndicator.vertical: ScrollIndicator { }
            FlashScrollIndicators { id: flashScroller; target: table }

            model: BS.SortFilterProxyModel {
                id: sortFilterModel
                sourceModel: BL.BrickLink.orders
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

                required property BL.Order order
                required property int index

                GridLayout {
                    id: layout
                    x: delegate.xspacing
                    y: delegate.xspacing / 2
                    width: parent.width - 2 * delegate.xspacing
                    columnSpacing: delegate.xspacing
                    rowSpacing: delegate.xspacing / 2
                    columns: 3

                    Image {
                        id: flag
                        Layout.rowSpan: 3
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                        asynchronous: true
                        source: "qrc:/assets/flags/" + delegate.order.countryCode
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
                        property string name: delegate.order.address.split("\n",1)[0]
                        text: delegate.order.otherParty + (name === '' ? '' : ' (' + name + ')')
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        Layout.fillWidth: true
                    }
                    Label {
                        text: delegate.order.date.toLocaleDateString(Locale.ShortFormat)
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: "#" + delegate.order.id
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: delegate.order.statusAsString()
                        font.bold: true
                        color: Qt.hsla(delegate.order.status/ BL.BrickLink.OrderStatus.Count, .8, .5)
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: qsTr("%1 items (%2 lots)")
                        .arg(Number(delegate.order.itemCount).toLocaleString())
                        .arg(Number(delegate.order.lotCount).toLocaleString())
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: BS.Currency.format(delegate.order.grandTotal, delegate.order.currencyCode, 2)
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
                    anchors.rightMargin: delegate.xspacing / 2
                    anchors.leftMargin: anchors.rightMargin

                }
                onClicked: {
                    root.goBackFunction()
                    BS.BrickStore.importBrickLinkOrder(delegate.order)
                }
            }
        }
    }

    footer: TabBar {
        id: receivedOrPlaced

        TabButton { text: qsTr("Received") }
        TabButton { text: qsTr("Placed") }

        onCurrentIndexChanged: {
            table.forceLayout()
            flashScroller.flash()
        }
    }

    Component.onCompleted: {
        flashScroller.flash()
    }
}
