// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Qt.labs.qmlmodels
import Mobile
import BrickStore as BS
import BrickLink as BL

pragma ComponentBehavior: Bound

Control {
    id: root

    required property int mode // BrickLink.InventoryModel.Mode
    required property var simpleLots // [ { item: , color: , quantity: } ]

    onSimpleLotsChanged: { delayedUpdateTimer.start() }

    Timer {
        id: delayedUpdateTimer
        interval: 100
        onTriggered: { listView.model = BL.BrickLink.inventoryModel(root.mode, root.simpleLots) }
    }

    ListView {
        id: listView
        anchors.fill: parent
        clip: true

        Label {
            anchors.fill: parent
            text: qsTr("No matches found")
            color: Style.hintTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: listView.model ? (listView.model.count === 0) : true
        }

        ScrollIndicator.vertical: ScrollIndicator { }

        delegate: DelegateChooser {
            role: "isSectionHeader"
            DelegateChoice {
                roleValue: true
                ItemDelegate {
                    required property string sectionTitle
                    text: sectionTitle
                    font.bold: true
                    font.italic: true
                }
            }
            DelegateChoice {
                roleValue: false
                ItemDelegate {
                    id: delegate
                    required property string id
                    required property string name
                    required property int quantity
                    required property var itemPointer
                    required property var colorPointer

                    width: ListView.view.width
                    height: layout.height + 8

                    property BL.Item blitem: BL.BrickLink.item(itemPointer)
                    property BL.Color blcolor: (root.mode === BL.InventoryModel.Mode.ConsistsOf)
                                               ? BL.BrickLink.color(colorPointer) : BL.BrickLink.noColor
                    property int showQuantity: (root.mode === BL.InventoryModel.Mode.ConsistsOf)
                                               || (root.mode === BL.InventoryModel.Mode.AppearsIn)

                    GridLayout {
                        id: layout
                        width: parent.width - 16
                        x: 8
                        y: 4
                        columnSpacing: 0
                        columns: 3

                        QImageItem {
                            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                            Layout.leftMargin: 8

                            FontMetrics { id: fm; font: delegate.font }

                            implicitHeight: fm.height * 3
                            implicitWidth: height * 4 / 3

                            property BL.Picture pic: BL.BrickLink.picture(delegate.blitem, BL.BrickLink.noColor)
                            property var noImage: BL.BrickLink.noImage(width, height)

                            image: pic && pic.isValid ? pic.image : noImage
                        }
                        Label {
                            id: description
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            Layout.rightMargin: 8
                            textFormat: Text.RichText
                            text: BL.BrickLink.itemHtmlDescription(delegate.blitem, delegate.blcolor, Style.accentColor)
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.rightMargin: 8
                            font.bold: true
                            text: delegate.quantity
                            font.pointSize: description.font.pointSize * 1.75
                            visible: delegate.showQuantity && delegate.quantity > 0
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }
}
