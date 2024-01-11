// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS
import BrickLink as BL


FullscreenDialog {
    id: root
    title: qsTr("Part out")

    property BL.Item currentItem: BL.BrickLink.noItem

    property var goBackFunction
    onBackClicked: {
        if (pages.currentIndex === 0)
            root.goBackFunction()
        else if (pages.currentIndex > 0)
            pages.currentIndex = pages.currentIndex - 1
    }

    toolButtons: ToolButton {
        property bool lastPage: pages.currentIndex === (pages.count - 1)
        text: lastPage ? qsTr("Import") : qsTr("Next")
        icon.name: lastPage ? "brick-1x1" : ""
        enabled: (pages.currentIndex !== 1) || (!root.currentItem.isNull)
        onClicked: {
            if (lastPage && !root.currentItem.isNull) {
                BS.BrickStore.importPartInventory(root.currentItem, BL.BrickLink.noColor,
                                                  importWidget.quantity, importWidget.condition,
                                                  importWidget.extraParts, importWidget.partOutTraits)
                goBackFunction()
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
                    onCurrentItemChanged: {
                        catList.model.filterItemType = currentItem.itemTypePointer
                        itemListModel.filterItemType = currentItem.itemTypePointer
                    }

                    currentIndex: 0

                    // the sort comes after the first setCurrentIndex, so we have to re-do the
                    // filterItemType settings
                    Component.onCompleted: Qt.callLater(function() { ittTabs.onCurrentItemChanged() })
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

                    delegate: RadioDelegate {
                        width: ListView.view.width
                        required property int index
                        required property string name
                        required property var categoryPointer
                        text: name
                        highlighted: checked
                        ButtonGroup.group: catListGroup
                        checked: index === 0

                        onCheckedChanged: {
                            if (checked)
                                itemListModel.filterCategory = categoryPointer
                        }
                        onClicked: {
                            pages.currentIndex = 1
                        }
                    }
                }
            }
        }

        Pane {
            id: itemListPage
            padding: 0

            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    Layout.topMargin: 8
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8

                    TextField {
                        id: filter
                        Layout.fillWidth: true
                        placeholderText: qsTr("Filter")
                    }
                    SpinBox {
                        id: zoom
                        from: 50
                        to: 500
                        value: 100
                        stepSize: 25
                        textFromValue: function(value, locale) { return value + "%" }
                    }
                }
                GridView {
                    id: itemList

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    property real scaleFactor: zoom.value / 100

                    FontMetrics { id: fm; font: itemListPage.font }

                    property int labelHeight: fm.height + 8

                    clip: true
                    cellWidth: width / 8 * scaleFactor
                    cellHeight: cellWidth * 3 / 4 + labelHeight
                    cacheBuffer: cellHeight
                    ScrollIndicator.vertical: ScrollIndicator { minimumSize: 0.05 }

                    model: BL.ItemModel {
                        id: itemListModel
                        filterWithoutInventory: true
                        filterText: filter.text

                        Component.onCompleted: { sort(1, Qt.AscendingOrder) }
                    }

                    property var noImage: BL.BrickLink.noImage(cellWidth, cellHeight)

                    delegate: MouseArea {
                        id: delegate
                        width: GridView.view.cellWidth
                        height: GridView.view.cellHeight
                        required property string id
                        required property string name
                        required property var itemPointer
                        property BL.Item blitem: BL.BrickLink.item(delegate.itemPointer)
                        property BL.Picture pic: BL.BrickLink.picture(blitem, blitem.defaultColor)

                        QImageItem {
                            anchors.fill: parent
                            anchors.bottomMargin: itemList.labelHeight
                            image: delegate.pic && delegate.pic.isValid ? delegate.pic.image : itemList.noImage
                        }
                        Label {
                            x: 8
                            width: parent.width - 2 * x
                            height: itemList.labelHeight
                            anchors.bottom: parent.bottom
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 5
                            clip: true
                            text: delegate.id
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            root.currentItem = blitem
                            pages.currentIndex = 2
                        }
                    }

                    PinchHandler {
                        target: null

                        property real startScale

                        grabPermissions: PointerHandler.CanTakeOverFromAnything

                        onActiveChanged: {
                            if (active)
                                startScale = zoom.value / 100
                        }
                        onActiveScaleChanged: {
                            if (active)
                                zoom.value = startScale * activeScale * 100
                        }
                    }
                }
            }
        }

        ImportInventoryWidget {
            id: importWidget
            padding: 0
            currentItem: root.currentItem
        }
    }
}
