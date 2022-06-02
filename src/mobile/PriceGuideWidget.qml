import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS

Control {
    id: root
    property BS.Document document: null
    property var item: BS.BrickLink.noItem
    property var color: BS.BrickLink.noColor

    property BS.PriceGuide priceGuide: null
    property bool isUpdating: (priceGuide && (priceGuide.updateStatus === BS.BrickLink.UpdateStatus.Updating))
    property real currencyRate: document ? BS.Currency.rate(document.currencyCode) : 0

    onItemChanged: { updatePriceGuide() }
    onColorChanged: { updatePriceGuide() }

    function updatePriceGuide() {
        if (!document || root.item.isNull || root.color.isNull)
            return

        if (priceGuide)
            priceGuide.release()
        priceGuide = BS.BrickLink.priceGuide(root.item, root.color, true)
        if (priceGuide)
            priceGuide.addRef()
    }

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
        anchors.fill: parent

        columns: 3
        rows: 13

        HeaderLabel { text: document ? document.currencyCode : '' }
        HeaderLabel { text: qsTr("New") }
        HeaderLabel { text: qsTr("Used") }

        HeaderLabel {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            text: qsTr("Last 6 Months Sale")
            background: Rectangle { anchors.fill: parent; color: root.palette.alternateBase}
        }

        Label { text: qsTr("Qty.") }
        QuantityLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.New }
        QuantityLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.Used }

        Label { text: qsTr("Min.") }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Lowest }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Lowest }

        Label { text: qsTr("Avg.") }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Average }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Average }

        Label { text: qsTr("Q.Avg.") }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.WAverage }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.WAverage }

        Label { text: qsTr("Max.") }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Highest }
        PriceLabel { time: BS.BrickLink.Time.PastSix; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Highest }

        HeaderLabel {
            Layout.fillWidth: true
            Layout.columnSpan: 3
            text: qsTr("Current Inventory")
            background: Rectangle { anchors.fill: parent; color: root.palette.alternateBase}
        }

        Label { text: qsTr("Qty.") }
        QuantityLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.New }
        QuantityLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.Used }

        Label { text: qsTr("Min.") }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Lowest }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Lowest }

        Label { text: qsTr("Avg.") }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Average }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Average }

        Label { text: qsTr("Q.Avg.") }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.WAverage }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.WAverage }

        Label { text: qsTr("Max.") }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.New;  price: BS.BrickLink.Price.Highest }
        PriceLabel { time: BS.BrickLink.Time.Current; condition: BS.BrickLink.Condition.Used; price: BS.BrickLink.Price.Highest }
    }
}

