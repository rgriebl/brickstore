// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS
import BrickLink as BL


FullscreenDialog {
    id: root
    title: qsTr("Part out")

    property alias currentItem: itemList.currentItem

    property var goBackFunction
    onBackClicked: {
        if (pages.currentIndex === 0)
            root.goBackFunction()
        else if (pages.currentIndex > 0)
            pages.currentIndex = pages.currentIndex - 1
    }

    BS.ExtraConfig {
        category: "ImportInventoryDialog"

        property alias itemType: ittTabs.currentIndex
        property alias itemZoom: itemList.zoom
        property alias itemFilterText: itemList.filterText

        property alias importQuantity: importWidget.quantity
        property alias importCondition: importWidget.condition
        property alias importExtraParts: importWidget.extraParts
        property alias importPartOutTraits: importWidget.partOutTraits
    }

    toolButtons: ToolButton {
        property bool lastPage: pages.currentIndex === (pages.count - 1)
        text: lastPage ? qsTr("Import") : qsTr("Next")
        icon.name: lastPage ? "brick-1x1" : ""
        enabled: (pages.currentIndex !== 1) || (!root.currentItem.isNull)
        onClicked: {
            if (lastPage && !root.currentItem.isNull) {
                root.goBackFunction()
                BS.BrickStore.importPartInventory(root.currentItem, BL.BrickLink.noColor,
                                                  importWidget.quantity, importWidget.condition,
                                                  importWidget.extraParts, importWidget.partOutTraits)
            } else if (visible) {
                pages.currentIndex = pages.currentIndex + 1
            }
        }
    }

    SwipeView {
        id: pages
        interactive: false
        anchors.fill: parent

        Pane {
            padding: 0

            ColumnLayout {
                anchors.fill: parent

                TabBar {
                    Layout.fillWidth: true
                    id: ittTabs
                    position: TabBar.Header

                    Repeater {
                        model: BL.ItemTypeModel {
                            filterWithoutInventory: true

                            // get "Set" to the left side
                            Component.onCompleted: { sort(0, Qt.DescendingOrder) }
                        }
                        delegate: TabButton {
                            required property var itemTypePointer
                            required property string name
                            text: name
                        }
                    }

                    currentIndex: 0
                    onCurrentItemChanged: updateFilters()

                    function updateFilters() {
                        catList.model.filterItemType = currentItem.itemTypePointer
                        itemList.model.filterItemType = currentItem.itemTypePointer
                    }

                    // the sort comes after the first setCurrentIndex, so we have to re-do the
                    // filterItemType settings
                    Component.onCompleted: Qt.callLater(ittTabs.updateFilters)
                }

                ListView {
                    id: catList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollIndicator.vertical: ScrollIndicator { }

                    model: BL.CategoryModel {
                        filterWithoutInventory: true
                        Component.onCompleted: { sort(0, Qt.AscendingOrder) }
                    }
                    ButtonGroup { id: catListGroup }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        required property int index
                        required property string name
                        required property var categoryPointer
                        text: name
                        highlighted: (itemList.model.filterCategory === categoryPointer)

                        onClicked: {
                            itemList.model.filterCategory = categoryPointer
                            pages.currentIndex = 1
                        }
                    }
                }
            }
        }

        Pane {
            padding: 0
            ItemList {
                id: itemList
                anchors.fill: parent
                filterWithoutInventory: true

                onItemSelected: pages.currentIndex = 2
            }
        }

        ImportInventoryWidget {
            id: importWidget
            padding: 0
            currentItem: root.currentItem
        }
    }
}
