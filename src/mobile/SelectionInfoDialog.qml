// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickLink as BL
import BrickStore as BS


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8

    padding: 0

    required property BS.Document document

    property bool single: false
    property bool none: false


    function updateInfos() {
        let selected = document.selectedLots.length
        single = (selected === 1)
        none = (selected === 0)

        if (single) {
            root.title = ''
            infoText.text = ''

            let lot = document.selectedLots[0]
            info.lot = lot

            priceGuide.item = lot.item
            priceGuide.color = lot.color

            appearsIn.items = [ lot.item ]
            appearsIn.colors = [ lot.color ]

            headerText.text = BL.BrickLink.itemHtmlDescription(lot.item, lot.color, Style.accentColor)
            headerText.visible = true
        } else {
            root.title = (selected === 0) ? qsTr("Document statistics")
                                          : qsTr("Multiple lots selected")
            headerText.visible = false
            headerText.text = ''

            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()

            info.lot = BL.BrickLink.noLot

            priceGuide.item = BL.BrickLink.noItem
            priceGuide.color = BL.BrickLink.noColor

            let items = []
            let colors = []

            document.selectedLots.forEach(function(lot) {
                if (!lot.item.isNull && !lot.color.isNull) {
                    items.push(lot.item)
                    colors.push(lot.color)
                }
            })

            appearsIn.items = items
            appearsIn.colors = colors
        }
        if (!tabBar.currentItem.enabled)
            tabBar.currentIndex = 0
    }

    function clearInfos() {
        info.lot = BL.BrickLink.noLot

        priceGuide.item = BL.BrickLink.noItem
        priceGuide.color = BL.BrickLink.noColor

        appearsIn.items = []
        appearsIn.colors = []
    }

    onAboutToShow: { updateInfos() }
    onClosed: { clearInfos() }

    footer: TabBar {
        id: tabBar

        currentIndex: swipeView.currentIndex

        TabButton { text: qsTr("Info"); }
        TabButton { text: qsTr("Price Guide"); enabled: root.single ? 1 : 0 }
        TabButton { text: qsTr("Appears In"); enabled: !root.none }
    }

    ColumnLayout {
        anchors.fill: parent
        Label {
            id: headerText
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            visible: false
            Layout.fillWidth: true
        }
        SwipeView {
            id: swipeView
            interactive: false
            currentIndex: tabBar.currentIndex
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                clip: true
                currentIndex: root.single ? 1 : 0

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
                visible: root.single

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

            AppearsInWidget {
                id: appearsIn
                document: root.document
            }
        }
    }
}
