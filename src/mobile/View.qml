//pragma ComponentBehavior: Bound

import Mobile
import Qt.labs.qmlmodels
import "utils.js" as Utils
import BrickStore as BS
import BrickLink as BL


Page {
    id: root
    padding: 0
    title: document.fileName ? document.fileName : (document.title ? document.title : qsTr("Untitled"))

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
            interactive: false

            property bool sorted: root.document.model.sorted
            property var sortColumns: root.document.model.sortColumns

            ViewHeaderMenu {
                id: headerMenu
                document: root.document
                model: table.model
            }

            delegate: GridHeader {
                onShowMenu: (logicalColumn, _) => {
                    headerMenu.field = logicalColumn
                    headerMenu.popup()
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

            ViewEditMenu {
                id: editMenu
                document: root.document
            }

            function showMenu(row, col) {
                if (col >= 0) {
                    editMenu.field = model.logicalColumn(col)
                    editMenu.popup()
                }
            }
            function toggleSelection(row, col) {
                if (row >= 0 && col >= 0) {
                    selectionModel.select(model.index(row, model.logicalColumn(col)),
                                          ItemSelectionModel.Rows | ItemSelectionModel.Toggle)
                }
            }

            model: BS.DocumentProxyModel {
                document: root.document
                onForceLayout: table.forceLayout()
            }

            delegate: DelegateChooser {
                id: chooser
                role: "documentField"

                component CurrencyGridCell: GridCell {
                    text: BS.Utility.fuzzyCompare(display, 0) ? "-" : BS.Currency.format(display)
                }
                component UIntGridCell : GridCell {
                    text: display === 0 ? "-" : display
                }
                component DateGridCell : GridCell {
                    property date dt: display

                    text: (isFinite(dt) && (dt.getUTCHours() === 0) && (dt.getUTCMinutes() === 0))
                          ? dt.toLocaleDateString(Locale.ShortFormat)
                          : isFinite(dt) ? dt.toLocaleString(Locale.ShortFormat)
                                         : '-'
                }

                DelegateChoice { roleValue: BS.DocumentModel.Condition
                    GridCell {
                        property bool isNew: display === BL.BrickLink.Condition.New
                        //horizontalAlignment: Text.AlignHCenter
                        text: isNew ? qsTr("N") : qsTr("U")
                        tint: isNew ? "transparent" : Qt.rgba(.5, .5, .5, .5)
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Status
                    GridCell {
                        property var status: display
                        required property var lot
                        property var bllot: BL.BrickLink.lot(lot)
                        property string tagText: bllot.counterPart
                                                 ? qsTr("CP")
                                                 : bllot.alternateId
                                                   ? bllot.alternateId
                                                   : ''
                        property bool tagBold: bllot.alternate
                        property color tagColor: bllot.alternateId
                                                 ? root.shadeColor(1 + bllot.alternateId, .5)
                                                 : Qt.rgba(.5, .5, .5, .3)

                        Label { }
                        CornerTag {
                            visible: parent.tagText !== ''
                            text: parent.tagText
                            bold: parent.tagBold
                            color: parent.tagColor
                        }
                        ToolButton {
                            anchors.fill: parent
                            enabled: false
                            icon.color: "transparent"
                            icon.name: parent.status === BL.BrickLink.Status.Exclude
                                       ? "vcs-removed" : (parent.status === BL.BrickLink.Status.Include
                                                          ? "vcs-normal" : "vcs-added")
                        }
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Picture
                    GridCell {
                        id: picCell
                        background: QImageItem {
                            fillColor: "white"
                            image: picCell.display
                        }
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Color
                    GridCell {
                        QImageItem {
                            id: colorImage
                            anchors {
                                left: parent.left
                                leftMargin: parent.leftPadding
                                top: parent.top
                                bottom: parent.bottom
                            }
                            width: parent.font.pixelSize * 2
                            image: BL.BrickLink.color(parent.display).image(width, height)
                        }
                        textLeftPadding: colorImage.width + 4
                        text: display
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Retain
                    GridCell {
                        property bool retain: display
                        text: ""
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: parent.retain
                        }
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Weight
                    GridCell {
                        text: BS.BrickStore.toWeightString(display, true)
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Stockroom
                    GridCell {
                        property var stockroom: display
                        text: stockroom === BL.BrickLink.Stockroom.A
                              ? 'A' : (stockroom === BL.BrickLink.Stockroom.B
                                ? 'B' : (stockroom === BL.BrickLink.Stockroom.C
                                  ? 'C' : ''))
                        CheckBox {
                            anchors.centerIn: parent
                            enabled: false
                            checked: false
                            visible: parent.stockroom === BL.BrickLink.Stockroom.None
                        }
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Sale
                    GridCell { text: root.humanReadableInteger(display, 0, "%") }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Bulk
                    GridCell { text: root.humanReadableInteger(display, 1) }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Price
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.PriceOrig
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.PriceDiff
                    GridCell {
//                        required property var baseLot  remove?
                        text: root.humanReadableCurrency(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }

                DelegateChoice { roleValue: BS.DocumentModel.Total
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(1, 1, 0, .1)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.Cost
                    CurrencyGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierP1
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierP2
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierP3
                    GridCell {
                        text: root.humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.Quantity
                    GridCell {
                        property bool isZero: display === 0
                        text: root.humanReadableInteger(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, .4)
                                          : display === 0 ? Qt.rgba(1, 1, 0, .4)
                                                          : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.QuantityOrig
                    GridCell {
                        text: display === 0 ? "-" : display.toLocaleString()
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.QuantityDiff
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierQ1
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierQ2
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.TierQ3
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.LotId
                    UIntGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.YearReleased
                    UIntGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.Category
                    GridCell {
                        required property var lot
                        tint: root.shadeColor(BL.BrickLink.lot(lot).category.id, 0.2)
                        text: display
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.ItemType
                    GridCell {
                        required property var lot
                        tint: root.shadeColor(BL.BrickLink.lot(lot).itemType.id.codePointAt(0), 0.1)
                        text: display
                    }
                }
                DelegateChoice { roleValue: BS.DocumentModel.DateAdded
                    DateGridCell { }
                }
                DelegateChoice { roleValue: BS.DocumentModel.DateLastSold
                    DateGridCell { }
                }

                DelegateChoice { GridCell { text: display } }
            }
        }
    }

    function humanReadableInteger(i, zero = 0, unit = '') {
        return i === zero ? "-" : i.toLocaleString() + unit
    }
    function humanReadableCurrency(c) {
        return BS.Utility.fuzzyCompare(c, 0) ? "-" : BS.Currency.format(c)
    }

    property var shades: []

    function shadeColor(n, alpha) {
        if (shades.length === 0) {
            for (let i = 0; i < 13; i++)
                shades.push(Qt.hsva(i * 30 / 360, 1, 1, 1));
        }
        let c = shades[n % shades.length]
        c.a = alpha
        return c
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
            function onBlockingOperationProgress(done, total) {
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
    property bool active: document && (document === BS.ActionManager.activeDocument)

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
