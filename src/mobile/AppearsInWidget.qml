// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS
import BrickLink as BL


Control {
    id: root
    property BS.Document document: null
    property var items: []
    property var colors: []

    onItemsChanged: { delayedUpdateTimer.start() }
    onColorsChanged: { delayedUpdateTimer.start() }

    Timer {
        id: delayedUpdateTimer
        interval: 100
        onTriggered: { root.updateAppearsIn() }
    }

    function updateAppearsIn() {
        listView.model = BL.BrickLink.appearsInModel(root.items, root.colors)
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
            visible: listView.model ? !listView.model.count : true
        }

        ScrollIndicator.vertical: ScrollIndicator { }

        delegate: ItemDelegate {
            id: delegate
            required property string id
            required property string name
            required property int quantity
            required property var itemPointer

            property int xspacing: 16
            width: ListView.view.width
            height: layout.height + xspacing

            property BL.Item blitem: BL.BrickLink.item(itemPointer)

            GridLayout {
                id: layout
                x: delegate.xspacing
                width: parent.width - 2 * delegate.xspacing
                columnSpacing: delegate.xspacing
                columns: 3

                QImageItem {
                    Layout.rowSpan: 2
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    FontMetrics { id: fm; font: delegate.font }

                    implicitHeight: fm.height * 2
                    implicitWidth: height * 4 / 3

                    property BL.Picture pic: BL.BrickLink.picture(delegate.blitem, BL.BrickLink.noColor)
                    property var noImage: BL.BrickLink.noImage(width, height)

                    image: pic && pic.isValid ? pic.image : noImage
                }
                Label {
                    Layout.fillWidth: true
                    text: (delegate.blitem.isNull ? '' : (delegate.blitem.itemType.id + ' ')) + delegate.id
                }
                Label {
                    font.bold: true
                    text: delegate.quantity
                    visible: delegate.quantity > 0
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    text: delegate.name
                    elide: Text.ElideRight
                }
            }
        }
    }
}
