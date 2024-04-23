// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickLink as BL
import BrickStore as BS


AutoSizingDialog {
    id: root
    title: qsTr("Set To Price Guide")
    keepPaddingInSmallMode: true
    standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok

    property alias forceUpdate: force.checked
    property int time: timePastSix.checked ? BL.BrickLink.Time.PastSix
                                           : BL.BrickLink.Time.Current
    property int price: priceLow.checked ? BL.BrickLink.Price.Lowest
                                         : (priceAvg.checked ? BL.BrickLink.Price.Average
                                                             : (priceWAvg.checked ? BL.BrickLink.Price.WAverage
                                                                                  : BL.BrickLink.Price.Highest))

    BS.ExtraConfig {
        category: "SetToPriceGuideDialog"

        property alias time: root.time
        property alias price: root.price
    }

    Connections { // 2-way binding for ExtraConfig
        target: root
        function onTimeChanged() {
            switch (root.time) {
            case BL.BrickLink.Time.PastSix: timePastSix.checked = true; break
            case BL.BrickLink.Time.Current: timeCurrent.checked = true; break
            }
        }

        function onPriceChanged() {
            switch (root.price) {
            case BL.BrickLink.Price.Lowest:   priceLow.checked = true; break;
            case BL.BrickLink.Price.Average:  priceAvg.checked = true; break;
            case BL.BrickLink.Price.WAverage: priceWAvg.checked = true; break;
            case BL.BrickLink.Price.Highest:  priceHigh.checked = true; break;
            }
        }
    }

    ScrollableLayout {
        id: scrollableLayout
        anchors.fill: parent

        ColumnLayout {
            id:layout
            width: scrollableLayout.width

            Label {
                text: qsTr("The prices of all selected items will be set to Price Guide values.<br /><br />Select which part of the price guide should be used:")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: layout.spacing
                CheckButton { id: timePastSix; text: qsTr("Last 6 Months Sales"); checked: true }
                CheckButton { id: timeCurrent; text: qsTr("Current Inventory") }
            }
            Label {
                text: qsTr("Which price:")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: layout.spacing
                CheckButton { id: priceLow;  text: qsTr("Minimum") }
                CheckButton { id: priceAvg;  text: qsTr("Average") }
                CheckButton { id: priceWAvg; text: qsTr("Quantity Average"); checked: true }
                CheckButton { id: priceHigh; text: qsTr("Maximum") }
            }
            RowLayout {
                IconImage {
                    name: BL.BrickLink.iconForVatType(BL.BrickLink.currentVatType)
                    color: "transparent"
                    sourceSize: Qt.size(vatTypeDescription.implicitHeight, vatTypeDescription.implicitHeight)
                }
                Label {
                    id: vatTypeDescription
                    text: BL.BrickLink.descriptionForVatType(BL.BrickLink.currentVatType)
                }
            }
            Switch {
                id: advancedSwitch
                text: qsTr("Advanced options")
                checked: false
            }
            ColumnLayout {
                id: advanced
                enabled: visible
                visible: advancedSwitch.checked

                Layout.leftMargin: 20

                Label {
                    text: qsTr("Only use these options if you know what you are doing!")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Switch {
                    id: force
                    Layout.fillWidth: true
                    text: qsTr("Download even if already in cache.")
                }
            }
            Item {
                visible: Style.smallSize
                Layout.fillHeight: true
            }
        }
    }
}
