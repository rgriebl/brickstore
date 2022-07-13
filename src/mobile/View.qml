//pragma ComponentBehavior: Bound

import Mobile
import Qt.labs.qmlmodels
import "utils.js" as Utils
import BrickStore as BS
import BrickLink as BL


Page {
    id: root
    padding: 0
    title: document.fileName.length ? document.fileName : (document.title.length ? document.title : qsTr("Untitled"))

    required property BS.Document document

    property var goHomeFunction

    DialogLoader {
        id: infoDialog
        autoUnload: false
        Component.onCompleted: {
            setSource("SelectionInfoDialog.qml", { "document": root.document })
        }
    }

    Component.onCompleted: {
        Utils.flashScrollIndicators(table)
    }

    focus: true

    header: ToolBar {
        id: toolBar
        topPadding: Style.topScreenMargin

        // The text color might be off after switching themes:
        // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-home"
                onClicked: root.goHomeFunction()
            }
            ToolButton {
                id: editUndo
                action: BS.ActionManager.quickAction("edit_undo")
                visible: enabled || editRedo.enabled
                display: Button.IconOnly
            }
            ToolButton {
                id: editRedo
                action: BS.ActionManager.quickAction("edit_redo")
                visible: enabled || editUndo.enabled
                display: Button.IconOnly
            }
            Label {
                Layout.fillWidth: true
                font.pointSize: root.font.pointSize * 1.3
                minimumPointSize: font.pointSize / 2
                fontSizeMode: Text.Fit
                text: root.title
                elide: Label.ElideLeft
                horizontalAlignment: Qt.AlignLeft
            }
            ToolButton {
                icon.name: "help-about"
                onClicked: {
                    if (!root.document)
                        return

                    infoDialog.open()
                }
            }
            ToolButton {
                icon.name: "overflow-menu"
                onClicked: viewMenu.open()

                AutoSizingMenu {
                    id: viewMenu
                    x: parent.width / 2 - width
                    y: parent.height / 2

                    transformOrigin: Menu.TopRight
                    modal: true
                    cascade: false

                    MenuItem { action: BS.ActionManager.quickAction("document_save") }
                    MenuItem { action: BS.ActionManager.quickAction("document_save_as") }
                    AutoSizingMenu {
                        title: BS.ActionManager.quickAction("document_export").text
                        modal: true
                        cascade: false
                        MenuItem { action: BS.ActionManager.quickAction("document_export_bl_xml") }
                        MenuItem { action: BS.ActionManager.quickAction("document_export_bl_xml_clip") }
                        MenuItem { action: BS.ActionManager.quickAction("document_export_bl_update_clip") }
                        MenuItem { action: BS.ActionManager.quickAction("document_export_bl_wantedlist_clip") }
                    }
                    MenuItem { action: BS.ActionManager.quickAction("document_close") }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HorizontalHeaderView {
            id: header
            syncView: table
            clip: true
            reuseItems: false
            //interactive: false

            property bool sorted: root.document.sorted
            property var sortColumns: root.document.sortColumns

            Loader {
                id: headerMenu
                function open(field : int) {
                    if (!_menu)
                        asynchronous = false
                    if (_menu) {
                        _menu.field = field
                        _menu.open()
                    }
                }
                Component.onCompleted: { setSource("ViewHeaderMenu.qml", { "document": root.document }) }
                asynchronous: true
                onLoaded: { _menu = item }
                property ViewHeaderMenu _menu: null
            }

            delegate: GridHeader { document: root.document }

            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                function mapPoint(point) {
                    point = header.mapFromItem(target, point)
                    let cell = header.cellAtPos(point)
                    let lx = cell.x < 0 ? -1 : root.document.logicalColumn(cell.x)
                    return { row: cell.y, column: lx }
                }

                onTapped: function(eventPoint) {
                    let m = mapPoint(eventPoint.position)
                    if (m.column >= 0)
                        headerMenu.open(m.column)
                }
            }
        }

        TableView {
            id: table
//            ScrollIndicator.horizontal: ScrollIndicator { id: hbar; active: vbar.active }
//            ScrollIndicator.vertical: ScrollIndicator { id: vbar; active: hbar.active }
            ScrollBar.horizontal: ScrollBar { id: hbar; minimumSize: 0.07; active: vbar.active }
            ScrollBar.vertical: ScrollBar { id: vbar; minimumSize: 0.07; active: hbar.active }

            boundsBehavior: Flickable.StopAtBounds

            Layout.fillHeight: true
            Layout.fillWidth: true

            columnSpacing: 0
            rowSpacing: 0
            clip: true
            reuseItems: true

            FontMetrics { id: fontMetrics; font: root.font }
            property int cellHeight: fontMetrics.height * 2 * BS.Config.itemImageSizePercent / 100 + 8
            onCellHeightChanged: Qt.callLater(function() { forceLayout() })

            columnWidthProvider: (c) => table.model.headerData(c, Qt.Horizontal, Qt.CheckStateRole)
                                 ? 0 : table.model.headerData(c, Qt.Horizontal, Qt.SizeHintRole)
            rowHeightProvider: () => cellHeight

            selectionModel: root.document.selectionModel

            Loader {
                id: editMenu
                function open(field : int) {
                    if (!_menu)
                        asynchronous = false
                    if (_menu) {
                        _menu.field = field
                        _menu.open()
                    }
                }

                property ViewEditMenu _menu: null
                asynchronous: true
                onLoaded: { _menu = item }
                Component.onCompleted: { setSource("ViewEditMenu.qml", { "document": root.document }) }
            }

            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                function mapPoint(point) {
                    point = table.mapFromItem(target, point)
                    let cell = table.cellAtPos(point)
                    let lx = cell.x < 0 ? -1 : root.document.logicalColumn(cell.x)
                    return { row: cell.y, column: lx }
                }

                onTapped: function(eventPoint) {
                    let m = mapPoint(eventPoint.position)
                    if (m.row >= 0 && m.column >= 0) {
                        table.selectionModel.select(table.model.index(m.row, m.column),
                                                    ItemSelectionModel.Rows | ItemSelectionModel.Toggle)
                    }
                }
                onLongPressed: function() {
                    let m = mapPoint(point.position)
                    editMenu.open(m.column)
                }
            }


            model: root.document

            Connections {
                target: root.document
                function onForceLayout() { table.forceLayout() }
            }

            delegate: DelegateChooser {
                id: chooser
                role: "documentField"

                component CurrencyGridCell: GridCell {
                    text: BS.Utility.fuzzyCompare(display, 0) ? "-" : BS.Currency.format(display)
                }
                component UIntGridCell : GridCell {
                    property int ui: display
                    text: ui === 0 ? "-" : ui
                }
                component DateGridCell : GridCell {
                    property date dt: display

                    text: (isFinite(dt) && (dt.getUTCHours() === 0) && (dt.getUTCMinutes() === 0))
                          ? dt.toLocaleDateString(Locale.ShortFormat)
                          : isFinite(dt) ? dt.toLocaleString(Locale.ShortFormat)
                                         : '-'
                }

                DelegateChoice { roleValue: BS.Document.Condition
                    GridCell {
                        property bool isNew: display === BL.BrickLink.Condition.New
                        //horizontalAlignment: Text.AlignHCenter
                        text: isNew ? qsTr("N") : qsTr("U")
                        tint: isNew ? "transparent" : Qt.rgba(.5, .5, .5, .5)
                    }
                }

                DelegateChoice { roleValue: BS.Document.Status
                    GridCell {
                        id: statusCell
                        property var status: display
                        required property var lot
                        property BL.Lot bllot: BL.BrickLink.lot(lot)
                        property string tagText: bllot.counterPart
                                                 ? qsTr("CP")
                                                 : bllot.alternateId
                                                   ? bllot.alternateId
                                                   : ''
                        property bool tagBold: bllot.alternate
                        property color tagColor: bllot.alternateId
                                                 ? BS.Utility.shadeColor(1 + bllot.alternateId, .5)
                                                 : Qt.rgba(.5, .5, .5, .3)

                        Label { }
                        CornerTag {
                            visible: statusCell.tagText !== ''
                            text: statusCell.tagText
                            bold: statusCell.tagBold
                            color: statusCell.tagColor
                        }
                        ToolButton {
                            anchors.fill: parent
                            enabled: false
                            icon.color: "transparent"
                            icon.name: statusCell.status === BL.BrickLink.Status.Exclude
                                       ? "vcs-removed" : (statusCell.status === BL.BrickLink.Status.Include
                                                          ? "vcs-normal" : "vcs-added")
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Picture
                    GridCell {
                        id: picCell
                        background: QImageItem {
                            fillColor: "white"
                            image: picCell.display
                            fallbackImage: BL.BrickLink.noImage(width, height)
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Color
                    GridCell {
                        id: colorCell
                        QImageItem {
                            id: colorImage
                            anchors {
                                left: colorCell.left
                                leftMargin: colorCell.leftPadding
                                top: colorCell.top
                                bottom: colorCell.bottom
                            }
                            width: colorCell.font.pixelSize * 2
                            image: BL.BrickLink.color(colorCell.display).sampleImage(width, height)
                        }
                        textLeftPadding: colorImage.width + 4
                        text: display
                    }
                }

                DelegateChoice { roleValue: BS.Document.Retain
                    GridCell {
                        id: retainCell
                        property bool retain: display
                        text: ""
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: retainCell.retain
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Weight
                    GridCell {
                        text: BS.BrickStore.toWeightString(display, true)
                    }
                }

                DelegateChoice { roleValue: BS.Document.Stockroom
                    GridCell {
                        id: stockroomCell
                        property var stockroom: display
                        text: stockroom === BL.BrickLink.Stockroom.A
                              ? 'A' : (stockroom === BL.BrickLink.Stockroom.B
                                ? 'B' : (stockroom === BL.BrickLink.Stockroom.C
                                  ? 'C' : ''))
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: false
                            visible: stockroomCell.stockroom === BL.BrickLink.Stockroom.None
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Sale
                    GridCell { text: root.humanReadableInteger(display, 0, "%") }
                }

                DelegateChoice { roleValue: BS.Document.Bulk
                    GridCell { text: root.humanReadableInteger(display, 1) }
                }

                DelegateChoice { roleValue: BS.Document.Price
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.PriceOrig
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.PriceDiff
                    GridCell {
//                        required property var baseLot  remove?
                        text: root.humanReadableCurrency(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }

                DelegateChoice { roleValue: BS.Document.Total
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(1, 1, 0, .1)
                    }
                }
                DelegateChoice { roleValue: BS.Document.Cost
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.TierP1
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierP2
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierP3
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: BS.Document.Quantity
                    GridCell {
                        property int qty: display
                        text: root.humanReadableInteger(display)
                        tint: qty < 0 ? Qt.rgba(1, 0, 0, .4)
                                      : qty === 0 ? Qt.rgba(1, 1, 0, .4)
                                                  : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.Document.QuantityOrig
                    GridCell {
                        property int qty: display
                        text: qty === 0 ? "-" : qty.toLocaleString()
                    }
                }
                DelegateChoice { roleValue: BS.Document.QuantityDiff
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ1
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ2
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ3
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: BS.Document.LotId
                    UIntGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.YearReleased
                    UIntGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.Category
                    GridCell {
                        required property int decoration // the cateogry id
                        tint: BS.Utility.shadeColor(decoration, 0.2)
                        text: display
                    }
                }
                DelegateChoice { roleValue: BS.Document.ItemType
                    GridCell {
                        required property int decoration // the itemtype id
                        tint: BS.Utility.shadeColor(decoration, 0.1)
                        text: display
                    }
                }
                DelegateChoice { roleValue: BS.Document.DateAdded
                    DateGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.DateLastSold
                    DateGridCell { }
                }
                DelegateChoice { roleValue: BS.Document.Marker
                    GridCell {
                        required property color decoration // the marker color
                        text: display
                        tint: decoration.valid ? decoration : "transparent"
                    }
                }

                DelegateChoice { GridCell { text: display } }
            }
        }
    }

    function humanReadableInteger(i : int, zero : int, unit : string) : string {
        if (typeof zero === 'undefined')
            zero = 0
        if (typeof unit === 'undefined')
            unit = ''
        return i === zero ? "-" : i.toLocaleString() + unit
    }
    function humanReadableCurrency(c : real) : string {
        return BS.Utility.fuzzyCompare(c, 0) ? "-" : BS.Currency.format(c)
    }

    Loader {
        id: blockDialog
        active: root.document.blockingOperationActive
        source: "ProgressDialog.qml"
        Binding {
            target: blockDialog.item
            property: "text"
            value: root.document.blockingOperationTitle
        }
        Binding {
            target: blockDialog.item
            property: "cancelable"
            value: root.document.blockingOperationCancelable
        }
        Connections {
            target: root.document
            function onBlockingOperationProgress(done : int, total : int) {
                blockDialog.item.total = total
                blockDialog.item.done = done
            }
        }
        Connections {
            target: blockDialog.item
            function onRequestCancel() { root.document.cancelBlockingOperation() }
        }
        onLoaded: { item.open() }
    }

    DialogLoader {
        id: setToPGDialog
        source: "SetToPriceGuideDialog.qml"
        onAccepted: root.document.setPriceToGuide(item.time, item.price, item.forceUpdate)
    }

    property QtObject connectionContext: null
    property bool active: document && (document.document === BS.ActionManager.activeDocument)

    onActiveChanged: {
        if (active) {
            connectionContext = BS.ActionManager.connectQuickActionTable
                    ({
                         "edit_price_to_priceguide": () => {
                             setToPGDialog.open()
                         },
                     })
        } else {
            BS.ActionManager.disconnectQuickActionTable(connectionContext)
            connectionContext = null
        }
    }
}
