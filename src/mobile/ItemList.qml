// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS
import BrickLink as BL


Control {
    id: root

    property BL.Item currentItem: BL.BrickLink.noItem

    signal itemSelected(item : BL.Item)

    property alias zoom: zoom.value
    property alias filterText: filter.text
    property alias filterWithoutInventory: itemListModel.filterWithoutInventory
    property alias model: itemListModel

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

            FontMetrics { id: fm; font: root.font }

            property int labelHeight: fm.height + 8

            clip: true
            cellWidth: zoom.value / 100 * Screen.pixelDensity * 20 // 20mm
            cellHeight: cellWidth * 3 / 4 + labelHeight
            cacheBuffer: cellHeight
            ScrollIndicator.vertical: ScrollIndicator { minimumSize: 0.05 }

            model: BL.ItemModel {
                id: itemListModel
                filterWithoutInventory: false
                filterText: root.filterText

                Component.onCompleted: { sort(1, Qt.AscendingOrder) }
            }

            property var noImage: BL.BrickLink.noImage(cellWidth, cellHeight)

            delegate: MouseArea {
                id: delegate
                width: (GridView.view as GridView).cellWidth
                height: (GridView.view as GridView).cellHeight
                required property string id
                required property string name
                required property var itemPointer
                property BL.Item blitem: BL.BrickLink.item(delegate.itemPointer)
                property BL.Picture pic: BL.BrickLink.picture(blitem, blitem.defaultColor)

                QImageItem {
                    anchors.fill: parent
                    anchors.bottomMargin: itemList.labelHeight
                    image: delegate.pic && delegate.pic.isValid ? delegate.pic.image : itemList.noImage
                    Rectangle {
                        visible: root.currentItem === delegate.blitem
                        anchors.fill: parent
                        color: "transparent"
                        border.color: Style.primaryColor
                        border.width: 3
                    }
                }
                Label {
                    leftPadding: 4
                    rightPadding: 4
                    width: parent.width
                    height: itemList.labelHeight
                    anchors.bottom: parent.bottom
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 5
                    clip: true
                    text: delegate.id
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    background: Rectangle {
                        visible: root.currentItem === delegate.blitem
                        color: Style.primaryColor
                    }
                }
                onClicked: {
                    root.currentItem = blitem
                    root.itemSelected(blitem)
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
