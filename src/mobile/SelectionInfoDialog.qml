import Mobile
import BrickLink as BL
import BrickStore as BS


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8

    padding: 0

    property BS.Document document

    property bool single: false
    property bool none: false


    function updateInfos() {
        let selected = document.selectedLots.length
        single = (selected === 1)
        none = (selected === 0)

        if (single) {

            let lot = BL.BrickLink.lot(document.selectedLots[0])
            info.lot = lot

            priceGuide.item = lot.item
            priceGuide.color = lot.color

            appearsIn.items = [ lot.item ]
            appearsIn.colors = [ lot.color ]

            root.title = BL.BrickLink.itemHtmlDescription(lot.item, lot.color, Style.accentColor)
        } else {
            root.title = (selected === 0) ? qsTr("Document statistics")
                                          : qsTr("Multiple lots selected")

            info.lot = undefined

            priceGuide.item = BL.BrickLink.noItem
            priceGuide.color = BL.BrickLink.noColor

            let items = []
            let colors = []

            document.selectedLots.forEach(function(s) {
                let lot = BL.BrickLink.lot(s)

                if (!lot.item.isNull && !lot.color.isNull) {
                    items.push(lot.item)
                    colors.push(lot.color)
                }
            })

            appearsIn.items = items
            appearsIn.colors = colors
        }
        if (!tabBar.currentItem.enabled)
            tabBar.currentIndex = 0
    }

    function clearInfos() {
        info.lot = undefined

        priceGuide.item = BL.BrickLink.noItem
        priceGuide.color = BL.BrickLink.noColor

        appearsIn.items = []
        appearsIn.colors = []
    }

    onAboutToShow: { updateInfos() }
    onClosed: { clearInfos() }

    footer: TabBar {
        id: tabBar

        currentIndex: swipeView.currentIndex

        TabButton { text: qsTr("Info"); }
        TabButton { text: qsTr("Price Guide"); enabled: root.single ? 1 : 0 }
        TabButton { text: qsTr("Appears In"); enabled: !root.none }
    }

    SwipeView {
        id: swipeView
        interactive: false
        anchors.fill: parent
        currentIndex: tabBar.currentIndex
        clip: true

        InfoWidget {
            id: info
            document: root.document
        }

        ScrollableLayout {
            id: pgScroller
            visible: root.single

            SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

            ColumnLayout {
                width: pgScroller.width
                PriceGuideWidget {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    id: priceGuide
                    document: root.document
                }
            }
        }

        AppearsInWidget {
            id: appearsIn
            document: root.document
        }
    }
}
