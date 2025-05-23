// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import Qt.labs.qmlmodels
import BrickStore as BS
import BrickLink as BL
import Scanner as Scanner

Page {
    id: root
    padding: 0
    title: document?.fileName.length ? document.fileName : (document?.title.length ? document.title : qsTr("Untitled"))

    required property BS.Document document

    property var goHomeFunction

    DialogLoader {
        id: infoDialog
        autoUnload: false

        Component {
            id: infoDialogComponent
            SelectionInfoDialog { }
        }
        sourceComponent: infoDialogComponent

        onOpened: {
            item.statistics = root.document.statistics()
            item.selection = root.document.selectedLots
        }
    }

    focus: true

    header: HeaderBar {
        title: root.title

        leftItem: ToolButton {
            icon.name: "application-menu"
            onClicked: viewMenu.open()

            AutoSizingMenu {
                id: viewMenu
                x: parent.width / 2
                y: parent.height / 2
                transformOrigin: Menu.TopLeft
                modal: true
                cascade: false

                ActionMenuItem { actionName: "go_home"; onClicked: root.goHomeFunction() }
                ActionMenuItem { actionName: "document_save" }
                ActionMenuItem { actionName: "document_save_as" }
                AutoSizingMenu {
                    title: BS.ActionManager.quickAction("document_export").text
                    modal: true
                    cascade: false
                    ActionMenuItem { autoHide: false; actionName: "document_export_bl_xml" }
                    ActionMenuItem { autoHide: false; actionName: "document_export_bl_xml_clip" }
                    ActionMenuItem { autoHide: false; actionName: "document_export_bl_update_clip" }
                    ActionMenuItem { autoHide: false; actionName: "document_export_bl_wantedlist_clip" }
                }
                ActionMenuItem {
                    // workaround for Qt bug: the overlay doesn't get removed in Qt 6.6+
                    onClicked: { viewMenu.modal = false }
                    actionName: "document_close"
                }
                ActionMenuSeparator {
                    visible: undoId.visible || redoId.visible
                }
                ActionMenuItem { id: undoId; actionName: "edit_undo" }
                ActionMenuItem { id: redoId; actionName: "edit_redo" }
            }
        }
        rightItem: RowLayout {
            ToolButton {
                icon.name: "help-about"
                onClicked: if (root.document) infoDialog.open()
            }
            ToolButton {
                action: BS.ActionManager.quickAction("edit_additems")
                display: AbstractButton.IconOnly
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

            property bool sorted: root.document?.sorted ?? false
            property var sortColumns: root.document?.sortColumns

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
                    point = header.contentItem.mapFromItem(target, point)
                    let cell = header.cellAtPosition(point)
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
            ScrollBar.horizontal: ScrollBar { id: hbar; minimumSize: 0.07; active: vbar.active }
            ScrollBar.vertical: ScrollBar { id: vbar; minimumSize: 0.07; active: hbar.active }

            boundsBehavior: Flickable.StopAtBounds

            Layout.fillHeight: true
            Layout.fillWidth: true

            columnSpacing: 0
            rowSpacing: 0
            clip: true
            reuseItems: false

            FontMetrics { id: fontMetrics; font: root.font }
            property int cellHeight: fontMetrics.height * 2 * BS.Config.rowHeightPercent / 100 + 8
            onCellHeightChanged: forceLayout()

            columnWidthProvider: (c) => table.model.headerData(c, Qt.Horizontal, Qt.CheckStateRole)
                                 ? 0 : table.model.headerData(c, Qt.Horizontal, Qt.SizeHintRole)
            rowHeightProvider: () => cellHeight

            model: root.document
            selectionModel: root.document?.selectionModel ?? null

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

            pointerNavigationEnabled: false

            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                function mapPoint(point) {
                    point = table.contentItem.mapFromItem(target, point)
                    let cell = table.cellAtPosition(point)
                    let lx = cell.x < 0 ? -1 : root.document.logicalColumn(cell.x)
                    return { row: cell.y, column: lx }
                }

                function toggleRowSelection(m) {
                    if (m.row >= 0 && m.column >= 0) {
                        table.selectionModel.setCurrentIndex(table.model.index(m.row, m.column),
                                                             ItemSelectionModel.Rows | ItemSelectionModel.Toggle)
                    }
                }

                onTapped: function(eventPoint) {
                    let m = mapPoint(eventPoint.position)
                    toggleRowSelection(m)
                }
                onLongPressed: function() {
                    let m = mapPoint(point.position)
                    if (!table.selectionModel.hasSelection)
                        toggleRowSelection(m)
                    editMenu.open(m.column)
                }
            }

            Connections {
                target: root.document
                function onColumnLayoutChanged() { Qt.callLater(table.forceLayout) }
            }

            delegate: DelegateChooser {
                id: chooser
                role: "documentField"

                component StandardGridCell: GridCell {
                    required property var edit
                    required property var nullValue
                    text: (nullValue !== undefined && edit === nullValue) ? '-' : display
                }

                DelegateChoice { roleValue: BS.Document.Condition
                    StandardGridCell {
                        property bool isNew: edit === BL.BrickLink.Condition.New
                        tint: isNew ? "transparent" : Qt.rgba(.5, .5, .5, .5)
                    }
                }

                DelegateChoice { roleValue: BS.Document.Status
                    GridCell {
                        id: statusCell
                        required property var edit
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
                            icon.name: statusCell.edit === BL.BrickLink.Status.Exclude
                                       ? "vcs-removed" : (statusCell.edit === BL.BrickLink.Status.Include
                                                       ? "vcs-normal" : "vcs-added")
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Picture
                    GridCell {
                        id: picCell
                        required property var edit
                        background: QImageItem {
                            fillColor: "white"
                            image: (picCell.edit as BL.Picture)?.image ?? BL.BrickLink.noImage(width, height)
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Color
                    GridCell {
                        id: colorCell
                        required property var edit
                        QImageItem {
                            id: colorImage
                            anchors {
                                left: colorCell.left
                                leftMargin: colorCell.leftPadding
                                top: colorCell.top
                                bottom: colorCell.bottom
                            }
                            property real s: Screen.devicePixelRatio
                            width: colorCell.font.pixelSize * 2
                            image: BL.BrickLink.color(colorCell.edit).sampleImage(width * s, height * s)
                        }
                        textLeftPadding: colorImage.width + 4
                        text: display
                    }
                }

                DelegateChoice { roleValue: BS.Document.Retain
                    GridCell {
                        id: retainCell
                        required property var edit
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: retainCell.edit
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.Stockroom
                    GridCell {
                        id: stockroomCell
                        required property var edit
                        text: edit === BL.BrickLink.Stockroom.A
                              ? 'A' : (edit === BL.BrickLink.Stockroom.B
                                ? 'B' : (edit === BL.BrickLink.Stockroom.C
                                  ? 'C' : ''))
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: false
                            visible: stockroomCell.edit === BL.BrickLink.Stockroom.None
                        }
                    }
                }

                DelegateChoice { roleValue: BS.Document.PriceDiff
                    StandardGridCell {
                        tint: edit < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                       : edit > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }

                DelegateChoice { roleValue: BS.Document.Total
                    StandardGridCell {
                        tint: Qt.rgba(1, 1, 0, .1)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierP1
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierP2
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierP3
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: BS.Document.Quantity
                    StandardGridCell {
                        tint: edit < 0 ? Qt.rgba(1, 0, 0, .4)
                                       : edit === 0 ? Qt.rgba(1, 1, 0, .4) : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.Document.QuantityDiff
                    StandardGridCell {
                        tint: edit < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                       : edit > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ1
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ2
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.Document.TierQ3
                    StandardGridCell {
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
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
                DelegateChoice { roleValue: BS.Document.Marker
                    GridCell {
                        required property color decoration // the marker color
                        text: display
                        tint: decoration.valid ? decoration : "transparent"
                    }
                }

                DelegateChoice { StandardGridCell { } }
            }
        }
    }

    Loader {
        id: blockDialog
        active: false
        source: "ProgressDialog.qml"
        Binding {
            target: blockDialog.item
            property: "text"
            value: root.document?.blockingOperationTitle
        }
        Binding {
            target: blockDialog.item
            property: "cancelable"
            value: root.document?.blockingOperationCancelable
        }
        Connections {
            target: root.document
            function onBlockingOperationProgress(done : int, total : int) {
                blockDialog.item.total = total
                blockDialog.item.done = done
            }
            function onBlockingOperationActiveChanged(blockingActive : bool) {
                // a direct binding doesn't close the dialog correctly and leaves the Overlay active
                blockDialog.active = blockingActive
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

    DialogLoader {
        id: selectColorDialog
        source: "SelectColor.qml"

        onOpened: {
            let rows = table.selectionModel.selectedRows()
            if (root.document.selectedLots.length) {
                let lot = root.document.selectedLots[0]
                item.setItemAndColor(lot.item, lot.color)
            }
        }
        onAccepted: root.document.setColor(item.color)
    }

    DialogLoader {
        id: adjustPriceDialog

        Component {
            id: adjustPrice

            IncDecPricesDialog {
                showTiers: !root.document.columnModel.isColumnHidden(BS.Document.TierQ1)
                text: qsTr("Increase or decrease the prices of the selected items by")
                currencyCode: root.document.currencyCode
            }
        }

        sourceComponent: adjustPrice
        onAccepted: root.document.priceAdjust(item.fixed, item.value, item.applyToTiers)
    }

    DialogLoader {
        id: adjustCostDialog

        Component {
            id: adjustCost

            IncDecPricesDialog {
                showTiers: false
                text: qsTr("Increase or decrease the costs of the selected items by")
                currencyCode: root.document.currencyCode
            }
        }

        sourceComponent: adjustCost
        onAccepted: root.document.costAdjust(item.fixed, item.value)
    }

    DialogLoader {
        id: addItemsDialog

        Component {
            id: addItems

            ItemScannerDialog {
                document: root.document
                title: qsTr("Add Items")
            }
        }
        sourceComponent: addItems
        onClosed: {
            if (!item.currentItem.isNull) {
                let newItem = item.currentItem
                let newColor = BL.BrickLink.color(0)
                if (newItem.itemType.hasColors) {
                    let knownColors = newItem.knownColors
                    if (knownColors.length > 0)
                        newColor = knownColors[0]
                }

                let row = root.document.lots.add(newItem, newColor)
                if (row !== -1) {
                    let index = table.model.index(row, 0)
                    table.selectionModel.select(index, ItemSelectionModel.Rows
                                                | ItemSelectionModel.ClearAndSelect)
                    table.positionViewAtRow(table.rowAtIndex(index), TableView.Contain)
                }
            }
        }
    }

    property QtObject connectionContext: null
    property bool active: document && (document.document === BS.ActionManager.activeDocument)

    onActiveChanged: {
        if (active) {
            connectionContext = BS.ActionManager.connectQuickActionTable
            ({
                 "edit_price_to_priceguide": () => { setToPGDialog.open() },
                 "edit_color": () => { selectColorDialog.open() },
                 "edit_price_inc_dec": () => { adjustPriceDialog.open() },
                 "edit_cost_inc_dec": () => { adjustCostDialog.open() },
                 "edit_additems": () => {
                     Scanner.Core.checkSystemPermissions(this, function(granted) {
                         if (granted) {
                             addItemsDialog.open()
                         }
                     })
                 },
             })
        } else {
            BS.ActionManager.disconnectQuickActionTable(connectionContext)
            connectionContext = null
        }
    }
}
