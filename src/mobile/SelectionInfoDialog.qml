// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickLink as BL
import BrickStore as BS


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8

    property BS.DocumentStatistics statistics
    property var selection // can be LotList or simple lots

    property var simpleLots: []

    function updateInfos() {
        let item = BL.BrickLink.noItem
        let color = BL.BrickLink.noColor

        if (selection.length === 1) {
            item = selection[0].item
            color = selection[0].color

            root.title = ''
            infoText.text = ''
            headerText.text = BL.BrickLink.itemHtmlDescription(item, color, Style.accentColor)
            headerText.visible = true
        } else {
            root.title = (selection.length === 0) ? qsTr("Document statistics")
                                                  : qsTr("Multiple lots selected")
            infoText.text = statistics.asHtmlTable()
            headerText.visible = false
            headerText.text = ''
        }

        info.item = priceGuide.item = item
        info.color = priceGuide.color = color

        let sl = []
        selection.forEach(function(lot) {
            sl.push({ item: lot.item, color: lot.color, quantity: lot.quantity })
        })
        simpleLots = sl

        // if (!tabBar.currentItem.visible)
        //     tabBar.currentIndex = 0
    }

    function clearInfos() {
        info.item = priceGuide.item = BL.BrickLink.noItem
        info.color = priceGuide.color = BL.BrickLink.noColor

        simpleLots = []
    }

    onAboutToShow: { Qt.callLater(updateInfos) }
    onStatisticsChanged: { Qt.callLater(updateInfos) }
    onSelectionChanged: { Qt.callLater(updateInfos) }
    onClosed: { clearInfos() }


    ColumnLayout {
        anchors.fill: parent
        TextEdit {
            id: headerText
            readOnly: true
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            visible: false
            Layout.fillWidth: true
            Layout.topMargin: 4
            font.pixelSize: root.font.pixelSize * 1.2
            color: Style.textColor
            selectionColor: Style.accentColor
            selectedTextColor: Style.primaryHighlightedTextColor
        }
        Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: -root.leftPadding
            Layout.rightMargin: -root.rightPadding

            padding: 0
            ColumnLayout {
                anchors.fill: parent
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
                            switch (root.simpleLots.length) {
                            case  0: return tabBar.tabs.slice(0, 1)
                            case  1: return tabBar.tabs
                            default: return tabBar.tabs.filter((_, idx) => idx !== 1)
                            }
                        }
                        TabButton {
                            required property var modelData
                            icon.name: modelData.icon
                            text: Style.smallSize ? undefined : modelData.label
                            property int swipeIndex: modelData.index
                            // property bool current: swipeView.currentIndex === swipeIndex
                            // onCurrentChanged: if (current) TabBar.tabBar.currentIndex = TabBar.index
                            Component.onCompleted: {
                                // otherwise the currentIndex increases on every Repeater instantiation
                                TabBar.tabBar.currentIndex = 0
                            }
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
                        currentIndex: (root.simpleLots.length === 1) ? 1 : 0

                        Label {
                            id: infoText
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            leftPadding: 8
                        }

                        InfoWidget { id: info }
                    }

                    ScrollableLayout {
                        id: pgScroller
                        visible: root.simpleLots.length === 1

                        SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

                        ColumnLayout {
                            width: pgScroller.width
                            PriceGuideWidget {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: 16
                                Layout.rightMargin: 16
                                id: priceGuide
                                currencyCode: root.statistics?.currencyCode ?? 'USD'
                            }
                        }
                    }

                    InventoryWidget {
                        id: appearsIn
                        mode: BL.InventoryModel.Mode.AppearsIn
                        simpleLots: root.simpleLots
                        visible: root.simpleLots.length > 0
                    }
                    InventoryWidget {
                        id: consistsOf
                        mode: BL.InventoryModel.Mode.ConsistsOf
                        simpleLots: root.simpleLots
                        visible: root.simpleLots.length > 0
                    }
                    InventoryWidget {
                        id: canBuild
                        mode: BL.InventoryModel.Mode.CanBuild
                        simpleLots: root.simpleLots
                        visible: root.simpleLots.length > 0
                    }
                    InventoryWidget {
                        id: related
                        mode: BL.InventoryModel.Mode.Relationships
                        simpleLots: root.simpleLots
                        visible: root.simpleLots.length > 0
                    }
                }
            }
        }
    }
}
