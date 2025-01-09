// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import Qt5Compat.GraphicalEffects
import BrickLink as BL
import BrickStore as BS


FullscreenDialog {
    id: root
    title: qsTr("Import Cart")

    property var goBackFunction
    onBackClicked: goBackFunction()

    BS.ExtraConfig {
        category: "ImportCartDialog"

        property alias domesticOrInternational: domesticOrInternational.currentIndex
    }

    toolButtons: ToolButton {
        icon.name: "view-refresh"
        onClicked: BL.BrickLink.carts.startUpdate()
        enabled: BL.BrickLink.carts.updateStatus !== BL.BrickLink.UpdateStatus.Updating
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
                sourceModel: BL.BrickLink.carts
                sortOrder: Qt.DescendingOrder
                sortColumn: 0
                filterSyntax: BS.SortFilterProxyModel.FixedString
                filterString: domesticOrInternational.currentIndex ? "false" : "true"

                Component.onCompleted: {
                    filterRoleName = "domestic"
                    sortRoleName = "lastUpdated"
                }
            }

            delegate: ItemDelegate {
                id: delegate
                property int xspacing: 16
                width: ListView.view.width
                height: layout.height + xspacing
                visible: cart

                required property BL.Cart cart
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
                        Layout.rowSpan: 2
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                        asynchronous: true
                        source: delegate.cart ? "qrc:/assets/flags/" + delegate.cart.countryCode : ''
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
                        text: delegate.cart?.storeName + ' (' + delegate.cart?.sellerName + ')'
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        Layout.fillWidth: true
                    }
                    Label {
                        text: delegate.cart?.lastUpdated.toLocaleDateString(Locale.ShortFormat) ?? ''
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: qsTr("%1 items (%2 lots)")
                        .arg(Number(delegate.cart?.itemCount).toLocaleString())
                        .arg(Number(delegate.cart?.lotCount).toLocaleString())
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: BS.Currency.format(delegate.cart?.total, delegate.cart?.currencyCode, 2)
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
                    BL.BrickLink.carts.startFetchLots(delegate.cart)
                    root.goBackFunction()
                }
            }
        }
    }
    footer: TabBar {
        id: domesticOrInternational

        TabButton { text: qsTr("Domestic") }
        TabButton { text: qsTr("International") }

        onCurrentIndexChanged: {
            // table.forceLayout()
            flashScroller.flash(table)
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() { BL.BrickLink.carts.startUpdate() })
        flashScroller.flash(table)
    }
}
