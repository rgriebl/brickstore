import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS


BrickStoreDialog {
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
            root.title = ''

            let lot = BS.BrickLink.lot(document.selectedLots[0])
            info.lot = lot

            priceGuide.item = lot.item
            priceGuide.color = lot.color

            appearsIn.items = [ lot.item ]
            appearsIn.colors = [ lot.color ]
        } else {
            root.title = (selected === 0) ? qsTr("Document statistics")
                                          : qsTr("Multiple lots selected")

            info.lot = undefined

            priceGuide.item = BS.BrickLink.noItem
            priceGuide.color = BS.BrickLink.noColor

            let items = []
            let colors = []

            document.selectedLots.forEach(function(s) {
                let lot = BS.BrickLink.lot(s)

                if (!lot.item.isNull && !lot.color.isNull) {
                    items.push(lot.item)
                    colors.push(lot.color)
                }
            })

            appearsIn.items = items
            appearsIn.colors = colors
        }
    }

    function clearInfos() {
        info.lot = undefined

        priceGuide.item = BS.BrickLink.noItem
        priceGuide.color = BS.BrickLink.noColor

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
