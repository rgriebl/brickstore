// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS


AutoSizingMenu {
    id: root
    property int field
    property BS.Document document
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent

    function is(ids) : bool {
        return Array.isArray(ids) ? ids.indexOf(field) >= 0 : ids === field
    }

    RowLayout {
        spacing: 0

        ToolButton { action: BS.ActionManager.quickAction("edit_cut")
            display: AbstractButton.IconOnly
            Layout.fillWidth: true
            onClicked: { root.dismiss(); root.document.selectionModel.clear() }
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
            onClicked: { root.dismiss(); root.document.selectionModel.clear() }
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

    ActionMenuItem { actionName: "edit_status_include"; visible: enabled && root.is(BS.Document.Status) }
    ActionMenuItem { actionName: "edit_status_exclude"; visible: enabled && root.is(BS.Document.Status) }
    ActionMenuItem { actionName: "edit_status_extra"; visible: enabled && root.is(BS.Document.Status) }
    ActionMenuItem { actionName: "edit_status_toggle"; visible: enabled && root.is(BS.Document.Status) }
//    ActionMenuItem { actionName: "edit_item"; visible: enabled && root.is([ BS.Document.Picture, BS.Document.PartNo, BS.Document.Description ]) }
    ActionMenuItem { actionName: "edit_cond_new"; visible: enabled && root.is(BS.Document.Condition) }
    ActionMenuItem { actionName: "edit_cond_used"; visible: enabled && root.is(BS.Document.Condition) }
    ActionMenuSeparator { visible: subCond.visible }
    ActionMenuItem { actionName: "edit_subcond_none"; visible: enabled && root.is(BS.Document.Condition); id: subCond }
    ActionMenuItem { actionName: "edit_subcond_sealed"; visible: enabled && root.is(BS.Document.Condition) }
    ActionMenuItem { actionName: "edit_subcond_complete"; visible: enabled && root.is(BS.Document.Condition) }
    ActionMenuItem { actionName: "edit_subcond_incomplete"; visible: enabled && root.is(BS.Document.Condition) }
    ActionMenuItem { actionName: "edit_color"; visible: enabled && root.is(BS.Document.Color) }
    ActionMenuItem { actionName: "edit_qty_set"; visible: enabled && root.is(BS.Document.Quantity) }
    ActionMenuItem { actionName: "edit_qty_multiply"; visible: enabled && root.is(BS.Document.Quantity) }
    ActionMenuItem { actionName: "edit_qty_divide"; visible: enabled && root.is(BS.Document.Quantity) }
    ActionMenuItem { actionName: "edit_price_set"; visible: enabled && root.is([ BS.Document.Price, BS.Document.Total ]) }
    ActionMenuItem { actionName: "edit_price_inc_dec"; visible: enabled && root.is([ BS.Document.Price, BS.Document.Total ]) }
    ActionMenuItem { actionName: "edit_price_round"; visible: enabled && root.is([ BS.Document.Price, BS.Document.Total ]) }
    ActionMenuItem { actionName: "edit_price_to_priceguide"; visible: enabled && root.is([ BS.Document.Price, BS.Document.Total ]) }
    ActionMenuItem { actionName: "edit_cost_set"; visible: enabled && root.is(BS.Document.Cost) }
    ActionMenuItem { actionName: "edit_cost_inc_dec"; visible: enabled && root.is(BS.Document.Cost) }
    ActionMenuItem { actionName: "edit_cost_round"; visible: enabled && root.is(BS.Document.Cost) }
    ActionMenuItem { actionName: "edit_cost_spread_price"; visible: enabled && root.is(BS.Document.Cost) }
    ActionMenuItem { actionName: "edit_cost_spread_weight"; visible: enabled && root.is(BS.Document.Cost) }
    ActionMenuItem { actionName: "edit_sale"; visible: enabled && root.is(BS.Document.Sale) }
    ActionMenuItem { actionName: "edit_bulk"; visible: enabled && root.is(BS.Document.Bulk) }
    ActionMenuItem { actionName: "edit_remark_set"; visible: enabled && root.is(BS.Document.Remarks) }
    ActionMenuItem { actionName: "edit_remark_add"; visible: enabled && root.is(BS.Document.Remarks) }
    ActionMenuItem { actionName: "edit_remark_rem"; visible: enabled && root.is(BS.Document.Remarks) }
    ActionMenuItem { actionName: "edit_remark_clear"; visible: enabled && root.is(BS.Document.Remarks) }
    ActionMenuItem { actionName: "edit_comment_set"; visible: enabled && root.is(BS.Document.Comments) }
    ActionMenuItem { actionName: "edit_comment_add"; visible: enabled && root.is(BS.Document.Comments) }
    ActionMenuItem { actionName: "edit_comment_rem"; visible: enabled && root.is(BS.Document.Comments) }
    ActionMenuItem { actionName: "edit_comment_clear"; visible: enabled && root.is(BS.Document.Comments) }
    ActionMenuItem { actionName: "edit_retain_yes"; visible: enabled && root.is(BS.Document.Retain) }
    ActionMenuItem { actionName: "edit_retain_no"; visible: enabled && root.is(BS.Document.Retain) }
    ActionMenuItem { actionName: "edit_retain_toggle"; visible: enabled && root.is(BS.Document.Retain) }
    ActionMenuItem { actionName: "edit_stockroom_no"; visible: enabled && root.is(BS.Document.Stockroom) }
    ActionMenuItem { actionName: "edit_stockroom_a"; visible: enabled && root.is(BS.Document.Stockroom) }
    ActionMenuItem { actionName: "edit_stockroom_b"; visible: enabled && root.is(BS.Document.Stockroom) }
    ActionMenuItem { actionName: "edit_stockroom_c"; visible: enabled && root.is(BS.Document.Stockroom) }
    ActionMenuItem { actionName: "edit_reserved"; visible: enabled && root.is(BS.Document.Reserved) }
    ActionMenuItem { actionName: "edit_marker_text"; visible: enabled && root.is(BS.Document.Marker) }
    ActionMenuItem { actionName: "edit_marker_color"; visible: enabled && root.is(BS.Document.Marker) }
    ActionMenuSeparator { visible: (ed3.visible) }
//    ActionMenuItem { actionName: "edit_additems"
//        visible: enabled
//    }
//    ActionMenuItem { actionName: "edit_subtractitems"
//        visible: enabled
//    }
    ActionMenuItem { id: ed3; actionName: "edit_mergeitems"; visible: enabled }
//    ActionMenuItem { actionName: "edit_partoutitems"
//        visible: enabled
//    }
    ActionMenuSeparator { visible: (bl1.visible || bl2.visible || bl3.visible || bl4.visible) }
    ActionMenuItem { id: bl1; actionName: "bricklink_catalog"; visible: enabled }
    ActionMenuItem { id: bl2; actionName: "bricklink_priceguide"; visible: enabled }
    ActionMenuItem { id: bl3; actionName: "bricklink_lotsforsale"; visible: enabled }
    ActionMenuItem { id: bl4; actionName: "bricklink_myinventory"; visible: enabled }

}
