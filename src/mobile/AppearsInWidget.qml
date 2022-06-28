import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

        ScrollIndicator.vertical: ScrollIndicator { }

        delegate: ItemDelegate {
            id: delegate
            required property string id
            required property string name
            required property int quantity
            required property var item
            required property var color

            property int xspacing: 16
            width: ListView.view.width
            height: layout.height + xspacing

            property var blitem: BL.BrickLink.item(item)

            GridLayout {
                id: layout
                x: parent.xspacing
                width: parent.width - 2 * parent.xspacing
                columnSpacing: parent.xspacing
                columns: 3

                QImageItem {
                    Layout.rowSpan: 2
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    FontMetrics { id: fm; font: delegate.font }

                    implicitHeight: fm.height * 2
                    implicitWidth: height * 4 / 3

                    property BL.Picture pic: BL.BrickLink.largePicture(delegate.blitem)
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
