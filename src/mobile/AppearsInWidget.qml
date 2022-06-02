import QtQuick
import QtQuick.Controls
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
            required property string id
            required property string name
            required property int quantity

            text: (quantity ? quantity : '-') + " " + id + " " + name
        }
    }
}
