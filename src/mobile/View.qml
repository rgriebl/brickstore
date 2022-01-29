import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material.impl
import QtQuick.Layouts
import Qt.labs.qmlmodels
import BrickStore 1.0

Page {
    id: root
    padding: 0
    title: document.fileName ? document.fileName : (document.title ? document.title : qsTr("Untitled"))

    property Document document

    header: ToolBar {
        id: toolBar

        // The text color might be off after switching themes:
        // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-home"
                onClicked: setActiveDocument(null)
            }
            ToolButton {
                action: ActionManager.quickAction("edit_undo")
                display: Button.IconOnly
            }
            ToolButton {
                action: ActionManager.quickAction("edit_redo")
                display: Button.IconOnly
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                icon.name: "overflow-menu"
                onClicked: viewMenu.open()

                AutoSizingMenu {
                    id: viewMenu
                    x: parent.width - width
                    transformOrigin: Menu.TopRight
                    modal: true
                    cascade: false

                    MenuItem { action: ActionManager.quickAction("document_save") }
                    MenuItem { action: ActionManager.quickAction("document_save_as") }
                    AutoSizingMenu {
                        title: ActionManager.quickAction("document_export").text
                        modal: true
                        cascade: false
                        MenuItem { action: ActionManager.quickAction("document_export_bl_xml") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_xml_clip") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_update_clip") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_wantedlist_clip") }
                    }
                    MenuItem { action: ActionManager.quickAction("document_close") }
                }
            }
        }
        Label {
            anchors.centerIn: parent
            scale: 1.3
            text: root.title
            elide: Label.ElideLeft
            horizontalAlignment: Qt.AlignHCenter
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

            property bool sorted: document.model.sorted
            property var sortColumns: document.model.sortColumns

            ViewHeaderMenu {
                id: headerMenu
                document: root.document
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
            ScrollBar.horizontal: ScrollBar { id: hbar; active: vbar.active }
            ScrollBar.vertical: ScrollBar { id: vbar; active: hbar.active }

            boundsBehavior: Flickable.StopAtBounds

            Layout.fillHeight: true
            Layout.fillWidth: true

            columnSpacing: 0
            rowSpacing: 0
            clip: true
            reuseItems: true

            FontMetrics { id: fontMetrics; font: root.font }
            property int cellHeight: fontMetrics.height * 2 + 8

            columnWidthProvider: (c) => table.model.headerData(c, Qt.Horizontal, Qt.SizeHintRole)
            rowHeightProvider: () => cellHeight

            selectionModel: document.selectionModel

            ViewEditMenu {
                id: editMenu
                document: root.document
            }

            TapHandler {
                function mapPoint(point) {
                    point = table.mapFromItem(target, point)
                    let cell = table.cellAtPos(point)
                    let lx = cell.x < 0 ? -1 : table.model.logicalColumn(cell.x)
                    return {
                        row: cell.y,
                        logicalColumn: lx,
                        visualColumn: cell.x
                    }
                }

                onTapped: function(eventPoint) {
                    let m = mapPoint(eventPoint.position)
                    if (m.row >= 0) {
                        table.selectionModel.select(table.model.index(m.row, m.logicalColumn),
                                                    ItemSelectionModel.Rows | ItemSelectionModel.Toggle)
                    }
                }
                onLongPressed: function() {
                    let m = mapPoint(point.position)
                    if (m.logicalColumn >= 0) {
                        editMenu.field = m.logicalColumn
                        editMenu.popup()
                    }
                }
            }

            model: DocumentProxyModel {
                document: root.document
                onForceLayout: table.forceLayout()
            }

            Component {
                id: currencyDelegate
                GridCell {
                    function fuzzyCompare(d1, d2) {
                        return Math.abs(d1 - d2) <= 1e-12 * Math.max(1.0, Math.abs(d1), Math.abs(d2));
                    }

                    text: root.fuzzyCompare(display) ? "-" : BrickStore.toCurrencyString(display)
                }
            }
            Component {
                id: uintDelegate
                GridCell {
                    text: display === 0 ? "-" : display
                }
            }

            delegate: DelegateChooser {
                id: chooser
                role: "documentField"

                DelegateChoice { roleValue: Document.Condition
                    GridCell {
                        property bool isNew: display === BrickLink.New
                        //horizontalAlignment: Text.AlignHCenter
                        text: isNew ? qsTr("N") : qsTr("U")
                        tint: isNew ? "transparent" : Qt.rgba(.5, .5, .5, .5)
                    }
                }

                DelegateChoice { roleValue: Document.Status
                    GridCell {
                        property var status: display

                        Label { }
                        ToolButton {
                            anchors.fill: parent
                            enabled: false
                            icon.color: "transparent"
                            icon.name: status === BrickLink.Exclude ? "vcs-removed"
                                                                    : (status === BrickLink.Include
                                                                       ? "vcs-normal" : "vcs-added")
                        }
                    }
                }

                DelegateChoice { roleValue: Document.Picture
                    GridCell {
                        background: QImageItem { image: display }
                    }
                }

                DelegateChoice { roleValue: Document.Color
                    GridCell {
                        id: cell
                        QImageItem {
                            id: colorImage
                            anchors {
                                left: parent.left
                                leftMargin: cell.leftPadding
                                top: parent.top
                                bottom: parent.bottom
                            }
                            width: parent.font.pixelSize * 2
                            image: BrickLink.color(parent.display).image
                        }
                        textLeftPadding: colorImage.width + 4
                        text: display
                    }
                }

                DelegateChoice { roleValue: Document.Retain
                    GridCell {
                        text: ""
                        property int checkState: display ? Qt.Checked : Qt.Unchecked
                        CheckIndicator {
                            anchors.centerIn: parent
                            control: parent
                        }
                    }
                }

                DelegateChoice { roleValue: Document.Weight
                    GridCell {
                        text: BrickStore.toWeightString(display, true)
                    }
                }

                DelegateChoice { roleValue: Document.Stockroom
                    GridCell {
                        property var stockroom: display
                        text: {
                            switch(stockroom) {
                            case BrickLink.A: return "A"
                            case BrickLink.B: return "B"
                            case BrickLink.C: return "C"
                            default: return ""
                            }
                        }
                        property int checkState: Qt.Unchecked
                        CheckIndicator {
                            anchors.centerIn: parent
                            visible: stockroom === BrickLink.None
                            control: parent
                        }
                    }
                }

                DelegateChoice { roleValue: Document.Sale
                    GridCell { text: humanReadableInteger(display, 0, "%") }
                }

                DelegateChoice { roleValue: Document.Bulk
                    GridCell { text: humanReadableInteger(display, 1) }
                }

                DelegateChoice { roleValue: Document.Price; delegate: currencyDelegate }
                DelegateChoice { roleValue: Document.PriceOrig; delegate: currencyDelegate }
                DelegateChoice { roleValue: Document.PriceDiff
                    GridCell {
                        required property var baseLot
                        text: humanReadableCurrency(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }

                DelegateChoice { roleValue: Document.Total
                    GridCell {
                        text: humanReadableCurrency(display)
                        tint: Qt.rgba(1, 1, 0, .1)
                    }
                }
                DelegateChoice { roleValue: Document.Cost; delegate: currencyDelegate }
                DelegateChoice { roleValue: Document.TierP1
                    GridCell {
                        text: humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: Document.TierP2
                    GridCell {
                        text: humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: Document.TierP3
                    GridCell {
                        text: humanReadableCurrency(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: Document.Quantity
                    GridCell {
                        property bool isZero: display === 0
                        text: humanReadableInteger(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, .4)
                                          : display === 0 ? Qt.rgba(1, 1, 0, .4)
                                                          : "transparent"
                    }
                }
                DelegateChoice { roleValue: Document.QuantityOrig
                    GridCell {
                        text: display === 0 ? "-" : display.toLocaleString()
                    }
                }
                DelegateChoice { roleValue: Document.QuantityDiff
                    GridCell {
                        text: humanReadableInteger(display)
                        tint: display < 0 ? Qt.rgba(1, 0, 0, 0.3)
                                          : display > 0 ? Qt.rgba(0, 1, 0, 0.3) : "transparent"
                    }
                }
                DelegateChoice { roleValue: Document.TierQ1
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .12)
                    }
                }
                DelegateChoice { roleValue: Document.TierQ2
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .24)
                    }
                }
                DelegateChoice { roleValue: Document.TierQ3
                    GridCell {
                        text: root.humanReadableInteger(display)
                        tint: Qt.rgba(.5, .5, .5, .36)
                    }
                }
                DelegateChoice { roleValue: Document.LotId; delegate: uintDelegate }
                DelegateChoice { roleValue: Document.YearReleased; delegate: uintDelegate }

                DelegateChoice { roleValue: Document.Category
                    GridCell {
                        required property var lot
                        tint: root.shadeColor(BrickLink.lot(lot).category.id, 0.2)
                        text: display
                    }
                }
                DelegateChoice { roleValue: Document.ItemType
                    GridCell {
                        required property var lot
                        tint: root.shadeColor(BrickLink.lot(lot).itemType.id, 0.1)
                        text: display
                    }
                }

                DelegateChoice { GridCell { text: display } }
            }
        }
    }

    function humanReadableInteger(i, zero = 0, unit = '') {
        return i === zero ? "-" : i.toLocaleString() + unit
    }
    function humanReadableCurrency(c) {
        return fuzzyCompare(c, 0) ? "-" : BrickStore.toCurrencyString(c)
    }

    function fuzzyCompare(d1, d2) {
        return Math.abs(d1 - d2) <= 1e-12 * Math.max(1.0, Math.abs(d1), Math.abs(d2));
    }

    property var shades: []

    function shadeColor(n, alpha) {
        if (shades.length === 0) {
            for (let i = 0; i < 13; i++)
                shades.push(Qt.hsva(i * 30 / 360, 1, 1, 1));
        }
        let c = shades[n % shades.length]
        return Qt.rgba(c.r, c.g, c.b, alpha)
    }
}
