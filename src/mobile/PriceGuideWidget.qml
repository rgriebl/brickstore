// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import BrickLink as BL
import BrickStore as BS


Control {
    id: root
    property BS.Document document: null
    property BL.Item item: BL.BrickLink.noItem
    property BL.Color color: BL.BrickLink.noColor

    property BL.PriceGuide priceGuide: null
    property bool isUpdating: (priceGuide && (priceGuide.updateStatus === BL.BrickLink.UpdateStatus.Updating))
    property real currencyRate: document ? BS.Currency.rate(document.currencyCode) : 0


    onItemChanged: { delayedUpdateTimer.start() }
    onColorChanged: { delayedUpdateTimer.start() }

    Timer {
        id: delayedUpdateTimer
        interval: 100
        onTriggered: { root.updatePriceGuide() }
    }

    function updatePriceGuide() {
        if (priceGuide)
            priceGuide.release()

        if (root.item.isNull || root.color.isNull)
            priceGuide = null
        else
            priceGuide = BL.BrickLink.priceGuide(root.item, root.color, true)

        if (priceGuide)
            priceGuide.addRef()
    }

    implicitHeight: layout.implicitHeight
    implicitWidth: layout.implicitWidth

    Component.onDestruction: {
        if (priceGuide)
            priceGuide.release()
    }

    component HeaderLabel : Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        font.bold: true
    }
    component PriceLabel : Label {
        property int time
        property int condition
        property int price

        Layout.fillWidth: true
        horizontalAlignment: Text.AlignRight
        text: !root.priceGuide || !root.priceGuide.isValid || root.isUpdating
              ? '-' : BS.Currency.format(root.priceGuide.price(time, condition, price) * root.currencyRate)
    }
    component QuantityLabel : Label {
        property int time
        property int condition

        Layout.fillWidth: true
        horizontalAlignment: Text.AlignRight
        text: !root.priceGuide || !root.priceGuide.isValid || root.isUpdating
              ? '-' : "%L1 (%L2)".arg(root.priceGuide.quantity(time, condition)).arg(root.priceGuide.lots(time, condition))
    }

    GridLayout {
        id: layout
        anchors.fill: parent

        columns: 3

        RowLayout {
            IconImage {
                id: vatType
                name: BL.BrickLink.iconForVatType(BL.BrickLink.currentVatType)
                color: "transparent"
                sourceSize: Qt.size(currency.implicitHeight, currency.implicitHeight)

                ToolTip.text: BL.BrickLink.descriptionForVatType(BL.BrickLink.currentVatType)
                ToolTip.timeout: 5000
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: vatType.ToolTip.visible = true
                }
            }
            HeaderLabel {
                id: currency
                text: root.document ? root.document.currencyCode : ''
            }
        }
        HeaderLabel { text: qsTr("New") }
        HeaderLabel { text: qsTr("Used") }

        HeaderLabel {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            text: qsTr("Last 6 Months Sales")
            background: Rectangle { anchors.fill: parent; color: Qt.darker(Style.backgroundColor, 1.1) }
        }

        Label { text: qsTr("Qty.") }
        QuantityLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.New }
        QuantityLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.Used }

        Label { text: qsTr("Min.") }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Lowest }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Lowest }

        Label { text: qsTr("Avg.") }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Average }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Average }

        Label { text: qsTr("Q.Avg.") }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.WAverage }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.WAverage }

        Label { text: qsTr("Max.") }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Highest }
        PriceLabel { time: BL.BrickLink.Time.PastSix; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Highest }

        HeaderLabel {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            text: qsTr("Current Inventory")
            background: Rectangle { anchors.fill: parent; color: Qt.darker(Style.backgroundColor, 1.1) }
        }

        Label { text: qsTr("Qty.") }
        QuantityLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.New }
        QuantityLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.Used }

        Label { text: qsTr("Min.") }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Lowest }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Lowest }

        Label { text: qsTr("Avg.") }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Average }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Average }

        Label { text: qsTr("Q.Avg.") }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.WAverage }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.WAverage }

        Label { text: qsTr("Max.") }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.New;  price: BL.BrickLink.Price.Highest }
        PriceLabel { time: BL.BrickLink.Time.Current; condition: BL.BrickLink.Condition.Used; price: BL.BrickLink.Price.Highest }

        ToolButton {
            Layout.columnSpan: 3
            Layout.alignment: Qt.AlignRight

            icon.name: "view-refresh"
            flat: true
            onClicked: {
                if (root.priceGuide)
                    root.priceGuide.update(true);
            }
        }
    }

    Label {
        id: pgUpdating
        anchors.fill: parent
        visible: root.isUpdating
        color: "black"
        fontSizeMode: Text.Fit
        font.pointSize: root.font.pointSize * 3
        font.italic: true
        minimumPointSize: root.font.pointSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: qsTr("Please wait... updating")
    }

    Glow {
        anchors.fill: pgUpdating
        visible: root.isUpdating
        radius: 8
        spread: 0.9
        color: "white"
        source: pgUpdating
    }
}

