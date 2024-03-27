// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickLink as BL
import BrickStore as BS


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8

    //padding: 0

    required property BS.Document document

    property int selectionSize: 0
    property var simpleLots: []

    function updateInfos() {
        selectionSize = document.selectedLots.length

        if (selectionSize === 1) {
            root.title = ''
            infoText.text = ''

            let lot = document.selectedLots[0]
            info.lot = lot

            priceGuide.item = lot.item
            priceGuide.color = lot.color

            simpleLots = [ { item: lot.item, color: lot.color, quantity: lot.quantity } ]

            headerText.text = //"<center>"
                    BL.BrickLink.itemHtmlDescription(lot.item, lot.color, Style.accentColor)
                    //+ "</center>"
            headerText.visible = true
        } else {
            root.title = (selectionSize === 0) ? qsTr("Document statistics")
                                               : qsTr("Multiple lots selected")
            headerText.visible = false
            headerText.text = ''

            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()

            info.lot = BL.BrickLink.noLot

            priceGuide.item = BL.BrickLink.noItem
            priceGuide.color = BL.BrickLink.noColor

            let sl = []
            document.selectedLots.forEach(function(lot) {
                if (!lot.item.isNull && !lot.color.isNull)
                    sl.push({ item: lot.item, color: lot.color, quantity: lot.quantity })
            })
            simpleLots = sl
        }
        if (!tabBar.currentItem.visible)
            tabBar.currentIndex = 0
    }

    function clearInfos() {
        info.lot = BL.BrickLink.noLot

        priceGuide.item = BL.BrickLink.noItem
        priceGuide.color = BL.BrickLink.noColor

        simpleLots = []
    }

    onAboutToShow: { updateInfos() }
    onClosed: { clearInfos() }


    ColumnLayout {
        anchors.fill: parent
        Label {
            id: headerText
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            visible: false
            Layout.fillWidth: true
            Layout.topMargin: 4
        }
        TabBar {
            id: tabBar

            background: Item { }

            Layout.fillWidth: true
            visible: tabRepeater.model.length > 1

            property var tabs: [
                { label: qsTr("Info"),        index: 0, icon: "help-about" },
                { label: qsTr("Price Guide"), index: 1, icon: "bricklink-priceguide" },
                { label: qsTr("Appears In"),  index: 2, icon: "bootstrap-box-arrow-in-right" },
                { label: qsTr("Consists Of"), index: 3, icon: "bootstrap-box-arrow-right" },
                { label: qsTr("Can Build"),   index: 4, icon: "bootstrap-bricks" },
                { label: qsTr("Related"),     index: 5, icon: "bootstrap-share-fill" }
            ]

            Repeater {
                id: tabRepeater
                model: {
                    switch (root.selectionSize) {
                    case  0: return tabBar.tabs.slice(0, 1)
                    case  1: return tabBar.tabs
                    default: return tabBar.tabs.filter((_, idx) => idx !== 1)
                    }
                }
                TabButton {
                    icon.name: modelData.icon
                    text: Style.smallSize ? undefined : modelData.label
                    property int swipeIndex: modelData.index
                    property bool current: swipeView.currentIndex === swipeIndex
                    onCurrentChanged: if (current) TabBar.tabBar.currentIndex = TabBar.index
                }
            }
        }

        SwipeView {
            id: swipeView
            interactive: false
            currentIndex: tabBar.currentItem?.swipeIndex ?? 0
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                clip: true
                currentIndex: (root.selectionSize === 1) ? 1 : 0

                Label {
                    id: infoText
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    leftPadding: 8
                }

                InfoWidget {
                    id: info
                    document: root.document
                }
            }

            ScrollableLayout {
                id: pgScroller
                visible: root.selectionSize === 1

                SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

                ColumnLayout {
                    width: pgScroller.width
                    PriceGuideWidget {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        id: priceGuide
                        document: root.document
                    }
                }
            }

            InventoryWidget {
                id: appearsIn
                mode: BL.InventoryModel.Mode.AppearsIn
                simpleLots: root.simpleLots
                visible: root.selectionSize > 0
            }
            InventoryWidget {
                id: consistsOf
                mode: BL.InventoryModel.Mode.ConsistsOf
                simpleLots: root.simpleLots
                visible: root.selectionSize > 0
            }
            InventoryWidget {
                id: canBuild
                mode: BL.InventoryModel.Mode.CanBuild
                simpleLots: root.simpleLots
                visible: root.selectionSize > 0
            }
            InventoryWidget {
                id: related
                mode: BL.InventoryModel.Mode.Relationships
                simpleLots: root.simpleLots
                visible: root.selectionSize > 0
            }
        }
    }
}
