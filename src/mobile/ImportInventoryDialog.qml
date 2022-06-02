import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS
import "./uihelpers" as UIHelpers

Page {
    id: root
    title: qsTr("Import")

    property var goBackFunction

    property var currentItem: BS.BrickLink.noItem
    property bool hasInstructions: false
    property bool hasCounterParts: false
    property bool hasAlternates: false
    property bool hasExtras: false

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-previous"
                onClicked: {
                    if (pages.currentIndex === 0)
                        goBackFunction()
                    else if (pages.currentIndex > 0)
                        pages.currentIndex = pages.currentIndex - 1
                }
            }
            Label {
                Layout.fillWidth: true
                scale: 1.3
                text: root.title
                minimumPointSize: font.pointSize / 2
                fontSizeMode: Text.Fit
                horizontalAlignment: Qt.AlignHCenter
            }
            ToolButton {
                property bool lastPage: pages.currentIndex == (pages.count - 1)
                text: lastPage ? qsTr("Import") : qsTr("Next")
                icon.name: lastPage ? "brick-1x1" : ""
                onClicked: {
                    if (lastPage && !root.currentItem.isNull) {
                        let condition = conditionNew.checked ? BS.BrickLink.Condition.New
                                                             : BS.BrickLink.Condition.Used
                        let extra = extraInclude.checked ? BS.BrickLink.Status.Include
                                                         : extraExclude.checked ? BS.BrickLink.Status.Exclude
                                                                                : BS.BrickLink.Status.Extra
                        if (BrickStore.importPartInventory(root.currentItem, BS.BrickLink.noColor,
                                                           quantity.value, condition, extra,
                                                           root.hasInstructions && includeInstructions.checked,
                                                           root.hasAlternates && includeAlternates.checked,
                                                           root.hasCounterParts && includeCounterParts.checked)) {
                            goBackFunction()
                        }
                    } else if (visible) {
                        pages.currentIndex = pages.currentIndex + 1
                    }
                }
            }
        }
     }

    SwipeView {
        id: pages
        interactive: false
        anchors.fill: parent

        Pane {
            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    ButtonGroup {
                        id: ittGroup
                    }
                    Repeater {
                        id: ittRepeater
                        model: BS.ItemTypeModel {
                            filterWithoutInventory: true
                        }
                        Button {
                            Layout.fillWidth: true
                            ButtonGroup.group: ittGroup
                            checkable: true
                            text: name
                            onClicked: {
                                checked = true
                                catList.model.filterItemType = itemType
                                itemList.model.filterItemType = itemType
                            }
                        }
                    }
                    Component.onCompleted: {
                        ittRepeater.itemAt(0).checked = true
                    }
                }

                ListView {
                    id: catList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollIndicator.vertical: ScrollIndicator { }

                    model: BS.CategoryModel { }
                    ButtonGroup { id: catListGroup }

                    delegate: RadioDelegate {
                        width: ListView.view.width
                        required property int index
                        required property string name
                        required property var category
                        text: name
                        highlighted: checked
                        ButtonGroup.group: catListGroup
                        checked: index === 0

                        onCheckedChanged: {
                            if (checked)
                                itemList.model.filterCategory = category
                        }
                        onClicked: {
                            pages.currentIndex = 1
                        }
                    }
                }
            }
        }

        Pane {
            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    Label { text: qsTr("Filter") }
                    TextField {
                        id: filter
                        Layout.fillWidth: true
                        placeholderText: qsTr("No filter")
                    }
                    Label { text: qsTr("Zoom") }
                    SpinBox {
                        id: zoom
                        from: 50
                        to: 500
                        value: 100
                        stepSize: 25
                        textFromValue: function(value, locale) { return value + "%" }
                    }
                }
                GridView {
                    id: itemList

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    property real scaleFactor: zoom.value / 100

                    clip: true
                    cellWidth: width / 8 * scaleFactor
                    cellHeight: cellWidth * 3 / 4
                    cacheBuffer: 8 * 2 * 2
                    ScrollIndicator.vertical: ScrollIndicator { }

                    model: BS.ItemModel {
                        filterWithoutInventory: true
                        filterText: filter.text
                    }

                    property var noImage: BS.BrickLink.noImage(cellWidth, cellHeight)

                    delegate: ItemDelegate {
                        width: GridView.view.cellWidth
                        height: GridView.view.cellHeight
                        required property string id
                        required property string name
                        required property var item
                        property BS.Picture pic: BS.BrickLink.picture(BS.BrickLink.item(item), BS.BrickLink.color(item.defaultColor))

                        BS.QImageItem {
                            anchors.fill: parent
                            image: parent.pic && parent.pic.isValid ? parent.pic.image() : itemList.noImage
                        }
                        onClicked: {
                            let it = BS.BrickLink.item(item)

                            root.currentItem = it
                            root.hasAlternates = false
                            root.hasCounterParts = false
                            root.hasInstructions = false
                            root.hasExtras = false

                            if (!it.isNull) {
                                if (it.itemType.id === "S")
                                    root.hasInstructions = !BS.BrickLink.item("I", it.id).isNull

                                it.consistsOf().forEach(function(lot) {
                                    if (lot.status === BS.BrickLink.Status.Extra)
                                        root.hasExtras = true
                                    if (lot.counterPart)
                                        root.hasCounterParts = true
                                    if (lot.alternate)
                                        root.hasAlternates = true
                                })
                            }
                            pages.currentIndex = 2
                        }
                    }

                    PinchHandler {
                        target: null

                        property real startScale

                        grabPermissions: PointerHandler.CanTakeOverFromAnything

                        onActiveChanged: {
                            if (active)
                                startScale = zoom.value / 100
                        }
                        onActiveScaleChanged: {
                            if (active)
                                zoom.value = startScale * activeScale * 100
                        }
                    }
                }
            }
        }

        Pane {
            GridLayout {
                enabled: root.currentItem !== BS.BrickLink.noItem
                anchors.fill: parent
                columns: 2
                Label { text: qsTr("Quantity") }
                SpinBox { id: quantity; from: 1; to: 1000; value: 1 }

                Label { text: qsTr("Condition") }
                RowLayout {
                    RadioButton { text: qsTr("New"); checked: true; id: conditionNew }
                    RadioButton { text: qsTr("Used") }
                }
                Label { text: qsTr("Extra parts") }
                RowLayout {
                    enabled: root.hasExtras
                    RadioButton { text: qsTr("Include"); checked: true; id: extraInclude }
                    RadioButton { text: qsTr("Exclude"); id: extraExclude }
                    RadioButton { text: qsTr("Extra"); id: extraExtra }
                }

                Label { text: qsTr("Include") }
                ColumnLayout {
                    Switch { text: qsTr("Instructions"); enabled: root.hasInstructions; id: includeInstructions }
                    Switch { text: qsTr("Alternates"); enabled: root.hasAlternates; id: includeAlternates }
                    Switch { text: qsTr("Counterparts"); enabled: root.hasCounterParts; id: includeCounterParts }
                }
            }
        }

    }
}
