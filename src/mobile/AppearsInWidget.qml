import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS

Control {
    id: root
    property BS.Document document: null
    property var items: []
    property var colors: []

    onItemsChanged: { updateAppearsIn() }
    onColorsChanged: { updateAppearsIn() }

    function updateAppearsIn() {
        listView.model = BS.BrickLink.appearsInModel(root.items, root.colors)
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

            property var blitem: BS.BrickLink.item(item)

            GridLayout {
                id: layout
                x: xspacing
                width: parent.width - 2 * xspacing
                columnSpacing: xspacing
                columns: 3

                BS.QImageItem {
                    Layout.rowSpan: 2
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    FontMetrics { id: fm; font: delegate.font }

                    height: fm.height * 2
                    width: height * 4 / 3

                    property BS.Picture pic: BS.BrickLink.picture(delegate.blitem, null)
                    property var noImage: BS.BrickLink.noImage(width, height)

                    image: pic && pic.isValid ? pic.image() : noImage
                }
                Label {
                    Layout.fillWidth: true
                    text: (delegate.blitem.isNull ? '' : (delegate.blitem.itemType.id + ' ')) + delegate.id
                }
                Label {
                    font.bold: true
                    text: delegate.quantity
                    visible: quantity > 0
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
