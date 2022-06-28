import Mobile
import BrickStore as BS


AutoSizingMenu {
    id: root
    property int field
    property BS.Document document
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent

    function is(ids) {
        return Array.isArray(ids) ? ids.indexOf(field) >= 0 : ids === field
    }

    RowLayout {
        ToolButton { action: BS.ActionManager.quickAction("edit_cut")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
            onClicked: root.dismiss()
        }
        ToolButton { action: BS.ActionManager.quickAction("edit_copy")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
            onClicked: root.dismiss()
        }
        ToolButton { action: BS.ActionManager.quickAction("edit_paste")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
            onClicked: root.dismiss()
        }
        ToolButton { action: BS.ActionManager.quickAction("edit_delete")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
            onClicked: root.dismiss()
        }
        ToolSeparator { }
        ToolButton { action: BS.ActionManager.quickAction("edit_select_all")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
        }
        ToolButton { action: BS.ActionManager.quickAction("edit_select_none")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
        }
    }
    ActionDelegate { actionName: "edit_status_include"; visible: enabled && root.is(BS.DocumentModel.Status) }
    ActionDelegate { actionName: "edit_status_exclude"; visible: enabled && root.is(BS.DocumentModel.Status) }
    ActionDelegate { actionName: "edit_status_extra"; visible: enabled && root.is(BS.DocumentModel.Status) }
    ActionDelegate { actionName: "edit_status_toggle"; visible: enabled && root.is(BS.DocumentModel.Status) }
    ActionDelegate { actionName: "edit_item"; visible: enabled && root.is([ BS.DocumentModel.Picture, BS.DocumentModel.PartNo, BS.DocumentModel.Description ]) }
    ActionDelegate { actionName: "edit_cond_new"; visible: enabled && root.is(BS.DocumentModel.Condition) }
    ActionDelegate { actionName: "edit_cond_used"; visible: enabled && root.is(BS.DocumentModel.Condition) }
    MenuSeparator {
        visible: subCond.visible
        height: visible ? implicitHeight : 0
    }
    ActionDelegate { actionName: "edit_subcond_none"; visible: enabled && root.is(BS.DocumentModel.Condition); id: subCond }
    ActionDelegate { actionName: "edit_subcond_sealed"; visible: enabled && root.is(BS.DocumentModel.Condition) }
    ActionDelegate { actionName: "edit_subcond_complete"; visible: enabled && root.is(BS.DocumentModel.Condition) }
    ActionDelegate { actionName: "edit_subcond_incomplete"; visible: enabled && root.is(BS.DocumentModel.Condition) }
    ActionDelegate { actionName: "edit_color"
        visible: enabled && root.is(BS.DocumentModel.Color)
    }
    ActionDelegate { actionName: "edit_qty_set"; visible: enabled && root.is(BS.DocumentModel.Quantity) }
    ActionDelegate { actionName: "edit_qty_multiply"; visible: enabled && root.is(BS.DocumentModel.Quantity) }
    ActionDelegate { actionName: "edit_qty_divide"; visible: enabled && root.is(BS.DocumentModel.Quantity) }
    ActionDelegate { actionName: "edit_price_set"; visible: enabled && root.is([ BS.DocumentModel.Price, BS.DocumentModel.Total ]) }
    ActionDelegate { actionName: "edit_price_inc_dec"; visible: enabled && root.is([ BS.DocumentModel.Price, BS.DocumentModel.Total ]) }
    ActionDelegate { actionName: "edit_price_round"; visible: enabled && root.is([ BS.DocumentModel.Price, BS.DocumentModel.Total ]) }
    ActionDelegate { actionName: "edit_price_to_priceguide"; visible: enabled && root.is([ BS.DocumentModel.Price, BS.DocumentModel.Total ]) }
    ActionDelegate { actionName: "edit_cost_set"; visible: enabled && root.is(BS.DocumentModel.Cost) }
    ActionDelegate { actionName: "edit_cost_inc_dec"; visible: enabled && root.is(BS.DocumentModel.Cost) }
    ActionDelegate { actionName: "edit_cost_round"; visible: enabled && root.is(BS.DocumentModel.Cost) }
    ActionDelegate { actionName: "edit_cost_spread_price"; visible: enabled && root.is(BS.DocumentModel.Cost) }
    ActionDelegate { actionName: "edit_cost_spread_weight"; visible: enabled && root.is(BS.DocumentModel.Cost) }
    ActionDelegate { actionName: "edit_sale"; visible: enabled && root.is(BS.DocumentModel.Sale) }
    ActionDelegate { actionName: "edit_bulk"; visible: enabled && root.is(BS.DocumentModel.Bulk) }
    ActionDelegate { actionName: "edit_remark_set"; visible: enabled && root.is(BS.DocumentModel.Remarks) }
    ActionDelegate { actionName: "edit_remark_add"; visible: enabled && root.is(BS.DocumentModel.Remarks) }
    ActionDelegate { actionName: "edit_remark_rem"; visible: enabled && root.is(BS.DocumentModel.Remarks) }
    ActionDelegate { actionName: "edit_remark_clear"; visible: enabled && root.is(BS.DocumentModel.Remarks) }
    ActionDelegate { actionName: "edit_comment_set"; visible: enabled && root.is(BS.DocumentModel.Comments) }
    ActionDelegate { actionName: "edit_comment_add"; visible: enabled && root.is(BS.DocumentModel.Comments) }
    ActionDelegate { actionName: "edit_comment_rem"; visible: enabled && root.is(BS.DocumentModel.Comments) }
    ActionDelegate { actionName: "edit_comment_clear"; visible: enabled && root.is(BS.DocumentModel.Comments) }
    ActionDelegate { actionName: "edit_retain_yes"; visible: enabled && root.is(BS.DocumentModel.Retain) }
    ActionDelegate { actionName: "edit_retain_no"; visible: enabled && root.is(BS.DocumentModel.Retain) }
    ActionDelegate { actionName: "edit_retain_toggle"; visible: enabled && root.is(BS.DocumentModel.Retain) }
    ActionDelegate { actionName: "edit_stockroom_no"; visible: enabled && root.is(BS.DocumentModel.Stockroom) }
    ActionDelegate { actionName: "edit_stockroom_a"; visible: enabled && root.is(BS.DocumentModel.Stockroom) }
    ActionDelegate { actionName: "edit_stockroom_b"; visible: enabled && root.is(BS.DocumentModel.Stockroom) }
    ActionDelegate { actionName: "edit_stockroom_c"; visible: enabled && root.is(BS.DocumentModel.Stockroom) }
    ActionDelegate { actionName: "edit_reserved"; visible: enabled && root.is(BS.DocumentModel.Reserved) }
    ActionDelegate { actionName: "edit_marker_text"; visible: enabled && root.is(BS.DocumentModel.Marker) }
    ActionDelegate { actionName: "edit_marker_color"; visible: enabled && root.is(BS.DocumentModel.Marker) }
    MenuSeparator { }
    ActionDelegate { actionName: "edit_additems"
        visible: enabled
    }
    ActionDelegate { actionName: "edit_subtractitems"
        visible: enabled
    }
    ActionDelegate { actionName: "edit_mergeitems"
        visible: enabled
    }
    ActionDelegate { actionName: "edit_partoutitems"
        visible: enabled
    }
    MenuSeparator { }
    ActionDelegate { actionName: "bricklink_catalog"; visible: enabled }
    ActionDelegate { actionName: "bricklink_priceguide"; visible: enabled }
    ActionDelegate { actionName: "bricklink_lotsforsale"; visible: enabled }
    ActionDelegate { actionName: "bricklink_myinventory"; visible: enabled }

}
