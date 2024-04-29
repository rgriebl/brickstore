// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickLink as BL
import BrickStore as BS
import QtQuick.Window


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8
    padding: 8
    topPadding: 8

    property BL.Color color
    property BL.Item item

    function setItemAndColor(newItem : BL.Item, newColor : BL.Color) {
        root.item = newItem;
        root.color = newColor;

        updateColorFilter();
    }

    function updateColorFilter() {
        colorModel.clearFilters()

        const filterType = Number(filter.currentValue)

        if (filterType > 0) {
            colorModel.colorTypeFilter = filterType
        } else if (filterType < 0) {
            let popularity = 0
            if (filterType === -2)
                popularity = 0.005
            else if (filterType === -3)
                popularity = 0.05

            // Modulex colors are fine in their own category, but not in the 'all' lists
            colorModel.colorTypeFilter = BL.BrickLink.ColorType.Mask & ~BL.BrickLink.ColorType.Modulex
            colorModel.popularityFilter = popularity

        } else if (filterType === 0 && !root.item.isNull) {
            colorModel.colorListFilter = root.item.knownColors
        }

        table.currentIndex = colorModel.indexOfColor(root.color).row
        if (table.currentIndex >= 0)
            table.positionViewAtIndex(table.currentIndex, ListView.Center)

        flashScroller.flash()
    }

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            ComboBox {
                id: filter
                Layout.fillWidth: true
                Layout.leftMargin: 8

                Layout.preferredWidth: implicitWidth

                textRole: "text"
                valueRole: "value"

                property ListModel colorFilterModel: ListModel {
                    Component.onCompleted: {
                        append({ text: qsTr("Known Colors"),        value:  0 /*KnownColors*/ })
                        append({ text: qsTr("All Colors"),          value: -1 /*AllColors*/ })
                        append({ text: qsTr("Popular Colors"),      value: -2 /*PopularColors*/ })
                        append({ text: qsTr("Most Popular Colors"), value: -3 /*MostPopularColors*/ })

                        BL.BrickLink.colorTypes.forEach((ct) => {
                                                            append({ text: qsTr("Only \"%1\" Colors").arg(BL.BrickLink.colorTypeName(ct)), value: ct })
                                                        })
                    }
                }

                model: colorFilterModel
                currentIndex: 0

                onCurrentValueChanged: root.updateColorFilter()
            }
            ToolButton {
                icon.name: "view-sort"
                icon.color: "transparent"

                AutoSizingMenu {
                    id: sortMenu
                    MenuItem {
                        text: qsTr("Color by Name")
                        onTriggered: { colorModel.sortByName(); root.updateColorFilter() }
                    }
                    MenuItem {
                        text: qsTr("Color by Hue")
                        onTriggered: { colorModel.sortByHue(); root.updateColorFilter() }
                    }
                }

                onClicked: {
                    sortMenu.open()
                }
            }
        }

        ListView {
            id: table
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true

            ScrollIndicator.vertical: ScrollIndicator { active: true }
            FlashScrollIndicators { id: flashScroller; target: table }

            currentIndex: colorModel.indexOfColor(root.color).row

            model: BL.ColorModel {
                id: colorModel
                colorListFilter: root.item.isNull ? [] : root.item.knownColors
                Component.onCompleted: { sortByName() }
            }
            delegate: ItemDelegate {
                id: delegate
                required property string name
                required property BL.Color colorObject

                property bool selected: ListView.isCurrentItem

                onSelectedChanged: {
                    delegate.background.color = selected ? Style.primaryColor : "transparent"
                    delegate.contentItem.color = selected ? Style.primaryHighlightedTextColor : Style.textColor
                }

                text: name
                width: ListView.view.width
                leftPadding: 8 + height + 8

                QImageItem {
                    x: 8
                    width: parent.height
                    height: parent.height
                    property real s: Screen.devicePixelRatio
                    image: delegate.colorObject.sampleImage(width * s, height * s)
                }
                onClicked: {
                    root.color = colorObject
                    root.accept()
                }
            }
        }
    }
}
