import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import BrickStore as BS

BrickStoreDialog {
    id: root
    relativeWidth: single ? (3 / 4) : (implicitWidth * 1.2 / Overlay.overlay.width)
    relativeHeight: single ? (3 / 4) : (implicitHeight * 1.2 / Overlay.overlay.height)

    property BS.Document document
    property bool single: false
    property bool none: false
    property bool is3D: false
    property BS.Picture picture
    property bool isUpdating: (picture && (picture.updateStatus === BS.BrickLink.UpdateStatus.Updating))
    property var renderController

    function updateStats() {
        let selected = document.selectedLots.length
        single = (selected === 1)
        none = (selected === 0)

        if (single) {
            let lot = BS.BrickLink.lot(document.selectedLots[0])
            infoText.text = BS.BrickLink.itemHtmlDescription(lot.item, lot.color, infoText.palette.highlight)
            root.title = ''

            if (picture)
                picture.release()
            picture = BS.BrickLink.picture(lot.item, lot.color, true)
            if (picture)
                picture.addRef()
            renderController.setPartAndColor(lot.item, lot.color)

            priceGuide.item = lot.item
            priceGuide.color = lot.color

            appearsIn.items = [ lot.item ]
            appearsIn.colors = [ lot.color ]
        } else {
            root.title = (selected === 0) ? qsTr("Document statistics")
                                          : qsTr("Multiple lots selected")
            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()

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

    Component.onCompleted: {
        renderController = BS.BrickStore.createRenderController()
        renderController.parent = root
        info3D.setSource("qrc:/ldraw/PartRenderer.qml", { "renderController": renderController })

        document.model.statisticsChanged.connect(function() { root.updateStats() })
    }
    Component.onDestruction: {
        if (picture)
            picture.release()
    }

    onAboutToShow: { updateStats() }
    onClosed: {
        infoImage.clear()
        isUpdating = false
    }

    footer: TabBar {
        id: tabBar

        currentIndex: swipeView.currentIndex

        TabButton { text: qsTr("Info"); }
        TabButton { text: qsTr("Price Guide"); enabled: root.single }
        TabButton { text: qsTr("Appears In"); enabled: !root.none }
    }

    SwipeView {
        id: swipeView
        interactive: false
        anchors.fill: parent
        currentIndex: tabBar.currentIndex
        clip: true

        Control {
            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true

                    id: infoText
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                StackLayout {
                    visible: root.single
                    clip: true

                    currentIndex: root.is3D ? 1 : 0

                    BS.QImageItem {
                        id: infoImage
                        fillColor: "white"
                        image: root.picture && root.picture.isValid ? root.picture.image() : noImage
                        property var noImage: BS.BrickLink.noImage(width, height)

                        Label {
                            id: infoImageUpdating
                            anchors.fill: parent
                            visible: root.isUpdating
                            color: "black"
                            fontSizeMode: Text.Fit
                            font.pointSize: root.font.pointSize * 3
                            minimumPointSize: root.font.pointSize
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: "<center><i>" + qsTr("Please wait... updating") + "</i></center>"
                        }

                        Glow {
                            anchors.fill: infoImageUpdating
                            visible: root.isUpdating
                            radius: 8
                            spread: 0.9
                            color: "white"
                            source: infoImageUpdating
                        }
                    }
                    Loader {
                        id: info3D
                        asynchronous: true
                    }
                }
            }
            RowLayout {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                }
                visible: root.single
                Button {
                    flat: true
                    text: root.is3D ? "2D" : "3D"
                    onClicked: root.is3D = !root.is3D
                }
                Item { Layout.fillWidth: true }
                Button {
                    icon.name: root.is3D ? "zoom-fit-best" : "view-refresh"
                    flat: true
                    onClicked: {
                        if (root.is3D)
                            root.renderController.resetCamera();
                        else if (root.picture)
                            root.picture.update(true);
                    }
                }
            }

        }

        PriceGuideWidget {
            id: priceGuide
            document: root.document
        }

        AppearsInWidget {
            id: appearsIn
            document: root.document
        }
    }
}
