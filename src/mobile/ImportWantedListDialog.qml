// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickLink as BL
import BrickStore as BS
import "utils.js" as Utils


FullscreenDialog {
    id: root
    title: qsTr("Import Wanted List")

    property var goBackFunction
    onBackClicked: goBackFunction()

    toolButtons: ToolButton {
        icon.name: "view-refresh"
        onClicked: BL.BrickLink.wantedLists.startUpdate()
        enabled: BL.BrickLink.wantedLists.updateStatus !== BL.BrickLink.UpdateStatus.Updating
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

            model: BS.SortFilterProxyModel {
                id: sortFilterModel
                sourceModel: BL.BrickLink.wantedLists
                sortOrder: Qt.AscendingOrder
                sortColumn: 0

                Component.onCompleted: {
                    sortRoleName = "name"
                }
            }

            delegate: ItemDelegate {
                id: delegate
                property int xspacing: 16
                width: ListView.view.width
                height: layout.height + xspacing

                required property BL.WantedList wantedList

                GridLayout {
                    id: layout
                    x: delegate.xspacing
                    y: delegate.xspacing / 2
                    width: parent.width - 2 * delegate.xspacing
                    columnSpacing: delegate.xspacing
                    rowSpacing: delegate.xspacing / 2
                    columns: 2

                    Label {
                        id: label
                        text: delegate.wantedList.name
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: delegate.wantedList.description
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: qsTr("%1 items (%2 lots)")
                        .arg(Number(delegate.wantedList.itemCount).toLocaleString())
                        .arg(Number(delegate.wantedList.lotCount).toLocaleString())
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("%1 items left, %2% filled")
                        .arg(Number(delegate.wantedList.itemLeftCount).toLocaleString())
                        .arg(delegate.wantedList.filled * 100)
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
                    BL.BrickLink.wantedLists.startFetchLots(wantedList)
                    root.goBackFunction()
                }
            }
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() { BL.BrickLink.wantedLists.startUpdate() })
        Utils.flashScrollIndicators(table)
    }
}
