import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS
import BrickLink as BL
import Mobile

Page {
    id: root
    title: qsTr("Part out")

    property var goBackFunction

    property var currentItem: BL.BrickLink.noItem
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
                        root.goBackFunction()
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
                property bool lastPage: pages.currentIndex === (pages.count - 1)
                text: lastPage ? qsTr("Import") : qsTr("Next")
                icon.name: lastPage ? "brick-1x1" : ""
                enabled: (pages.currentIndex !== 1) || (!root.currentItem.isNull)
                onClicked: {
                    if (lastPage && !root.currentItem.isNull) {
                        let condition = conditionNew.checked ? BL.BrickLink.Condition.New
                                                             : BL.BrickLink.Condition.Used
                        let extra = extraInclude.checked ? BL.BrickLink.Status.Include
                                                         : extraExclude.checked ? BL.BrickLink.Status.Exclude
                                                                                : BL.BrickLink.Status.Extra
                        if (BS.BrickStore.importPartInventory(root.currentItem, BL.BrickLink.noColor,
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
            padding: 0

            ColumnLayout {
                anchors.fill: parent

                TabBar {
                    Layout.fillWidth: true
                    id: ittTabs
                    position: TabBar.Header

                    Repeater {
                        model: BL.ItemTypeModel {
                            filterWithoutInventory: true

                            // get "Set" to the left side
                            Component.onCompleted: { sort(0, Qt.DescendingOrder) }
                        }
                        delegate: TabButton {
                            required property var itemType
                            required property string name
                            text: name
                        }
                    }
                    onCurrentItemChanged: {
                        catList.model.filterItemType = currentItem.itemType
                        itemList.model.filterItemType = currentItem.itemType
                    }
                    currentIndex: 0
                }

                ListView {
                    id: catList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollIndicator.vertical: ScrollIndicator { }

                    model: BL.CategoryModel {
                        filterWithoutInventory: true
                        Component.onCompleted: { sort(0, Qt.AscendingOrder) }
                    }
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
            id: itemListPage
            padding: 0
            property bool isPageVisible: SwipeView.isCurrentItem

            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    //spacing: 16
                    Item { width: 8 }
                    TextField {
                        id: filter
                        Layout.fillWidth: true
                        placeholderText: qsTr("Filter")
                    }
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

                    FontMetrics { id: fm; font: itemListPage.font }

                    property int labelHeight: fm.height + 8

                    clip: true
                    cellWidth: width / 8 * scaleFactor
                    cellHeight: cellWidth * 3 / 4 + labelHeight
                    cacheBuffer: cellHeight
                    ScrollIndicator.vertical: ScrollIndicator { minimumSize: 0.05 }

                    model: BL.ItemModel {
                        filterWithoutInventory: true
                        filterText: filter.text

                        Component.onCompleted: { sort(1, Qt.AscendingOrder) }
                    }

                    property var noImage: BL.BrickLink.noImage(cellWidth, cellHeight)

                    delegate: MouseArea {
                        id: delegate
                        width: GridView.view.cellWidth
                        height: GridView.view.cellHeight
                        required property string id
                        required property string name
                        required property var item
                        property BL.Picture pic: itemListPage.isPageVisible
                                                 ? BL.BrickLink.picture(BL.BrickLink.item(delegate.item), BL.BrickLink.item(delegate.item).defaultColor)
                                                 : null

                        QImageItem {
                            anchors.fill: parent
                            anchors.bottomMargin: itemList.labelHeight
                            image: delegate.pic && delegate.pic.isValid ? delegate.pic.image : itemList.noImage
                        }
                        Label {
                            x: 8
                            width: parent.width - 2 * x
                            height: itemList.labelHeight
                            anchors.bottom: parent.bottom
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 5
                            clip: true
                            text: delegate.id
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            let it = BL.BrickLink.item(delegate.item)

                            root.currentItem = it
                            root.hasAlternates = false
                            root.hasCounterParts = false
                            root.hasInstructions = false
                            root.hasExtras = false

                            if (!it.isNull) {
                                if (it.itemType.id === "S")
                                    root.hasInstructions = !BL.BrickLink.item("I", it.id).isNull

                                it.consistsOf().forEach(function(lot) {
                                    if (lot.status === BL.BrickLink.Status.Extra)
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
            padding: 0
            ColumnLayout {
                enabled: !root.currentItem.isNull
                anchors.fill: parent
                RowLayout {
                    Layout.topMargin: 8
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 0
                    QImageItem {
                        height: lfm.height * 5
                        width: height * 4 / 3

                        property BL.Picture pic: BL.BrickLink.picture(root.currentItem, BL.BrickLink.noColor, true)
                        image: pic ? pic.image : BL.BrickLink.noImage(width, height)
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: "<b>" + root.currentItem.id + "</b> " + root.currentItem.name
                              + " <i>(" + root.currentItem.itemType.name + ")</i>"
                        textFormat: Text.StyledText
                        wrapMode: Text.Wrap
                        verticalAlignment: Text.AlignVCenter

                        FontMetrics { id: lfm }
                    }
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    Layout.rightMargin: 16
                    ItemDelegate { text: qsTr("Quantity") }
                    SpinBox { id: quantity; editable: true; from: 1; to: 1000; value: 1 }

                    component CheckButton : Button {
                        flat: true
                        checkable: true
                        autoExclusive: true
                        icon.color: "transparent"
                        leftPadding: 16
                        rightPadding: 16
                    }

                    ItemDelegate { text: qsTr("Condition") }
                    Flow {
                        Layout.fillWidth: true
                        spacing: parent.columnSpacing
                        CheckButton { text: qsTr("New"); checked: true; id: conditionNew }
                        CheckButton { text: qsTr("Used") }
                    }
                    ItemDelegate {
                        text: qsTr("Extra parts")
                        visible: root.hasExtras
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: parent.columnSpacing
                        visible: root.hasExtras
                        CheckButton { icon.name: "vcs-normal";  text: qsTr("Include"); id: extraInclude; checked: true }
                        CheckButton { icon.name: "vcs-removed"; text: qsTr("Exclude"); id: extraExclude }
                        CheckButton { icon.name: "vcs-added";   text: qsTr("Extra");   id: extraExtra }
                    }

                    ItemDelegate { text: qsTr("Include") }
                    Switch { text: qsTr("Instructions"); enabled: root.hasInstructions; id: includeInstructions }
                    ItemDelegate { }
                    Switch { text: qsTr("Alternates"); enabled: root.hasAlternates; id: includeAlternates }
                    ItemDelegate { }
                    Switch { text: qsTr("Counterparts"); enabled: root.hasCounterParts; id: includeCounterParts }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
