/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <QWidget>
#include <QRegExp>
#include <QHash>
#include <qdom.h>

#include "bricklinkfwd.h"
#include "document.h"
#include "config.h"
#include "currency.h"

QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QTableView)
class FrameWork;
class UndoStack;
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)
class HeaderView;


class Window : public QWidget
{
    Q_OBJECT
public:
    Window(Document *doc, QWidget *parent = nullptr);

    Document *document() { return m_doc; }
    DocumentProxyModel *documentView() { return m_view; } // for scripting

    enum MergeFlags {
        MergeAction_None         = 0x00000000,
        MergeAction_Force        = 0x00000001,
        MergeAction_Ask          = 0x00000002,
        MergeAction_Mask         = 0x0000ffff,

        MergeKeep_Old            = 0x00000000,
        MergeKeep_New            = 0x00010000,
        MergeKeep_Mask           = 0xffff0000
    };

//    const Document::ItemList &items() const      { return m_doc->items(); }
    const Document::ItemList &selection() const  { return m_selection; }
    Document::Item *current() const              { return m_current; }

    uint setItems(const BrickLink::InvItemList &items, int multiply = 1);
    int addItems(const BrickLink::InvItemList &items, int multiply = 1, uint mergeflags = MergeAction_None, bool dont_change_sorting = false);
    void deleteItems(const BrickLink::InvItemList &items);

    void mergeItems(const Document::ItemList &items, uint globalmergeflags = MergeAction_Ask);

    void subtractItems(const BrickLink::InvItemList &items);
    void copyRemarks(const BrickLink::InvItemList &items);

    QString filter() const;
    QString filterToolTip() const;

    QDomElement createGuiStateXML();
    bool parseGuiStateXML(const QDomElement &root);

    bool isDifferenceMode() const;
    bool isSimpleMode() const;

public slots:
    void setFilter(const QString &str);
    void setDifferenceMode(bool b);
    void setSimpleMode(bool b);
    void setSelection(const Document::ItemList &);

    void on_view_difference_mode_toggled(bool);

    void on_file_save_triggered();
    void on_file_saveas_triggered();
    void on_file_export_bl_xml_triggered();
    void on_file_export_bl_xml_clip_triggered();
    void on_file_export_bl_update_clip_triggered();
    void on_file_export_bl_invreq_clip_triggered();
    void on_file_export_bl_wantedlist_clip_triggered();

    void on_file_print_triggered();
    void on_file_print_pdf_triggered();
    void on_file_close_triggered();

    void on_edit_cut_triggered();
    void on_edit_copy_triggered();
    void on_edit_paste_triggered();
    void on_edit_delete_triggered();

    void on_edit_subtractitems_triggered();
    void on_edit_copyremarks_triggered();
    void on_edit_mergeitems_triggered();
    void on_edit_partoutitems_triggered();
    void on_edit_setmatch_triggered();

    void on_edit_reset_diffs_triggered();

    void on_edit_select_all_triggered();
    void on_edit_select_none_triggered();

    void on_edit_filter_from_selection_triggered();

    void on_edit_status_include_triggered();
    void on_edit_status_exclude_triggered();
    void on_edit_status_extra_triggered();
    void on_edit_status_toggle_triggered();
    void on_edit_cond_new_triggered();
    void on_edit_cond_used_triggered();
    void on_edit_cond_toggle_triggered();
    void on_edit_subcond_none_triggered();
    void on_edit_subcond_sealed_triggered();
    void on_edit_subcond_complete_triggered();
    void on_edit_subcond_incomplete_triggered();
    void on_edit_color_triggered();
    void on_edit_qty_set_triggered();
    void on_edit_qty_multiply_triggered();
    void on_edit_qty_divide_triggered();
    void on_edit_price_set_triggered();
    void on_edit_price_round_triggered();
    void on_edit_price_to_priceguide_triggered();
    void on_edit_price_inc_dec_triggered();
    void on_edit_bulk_triggered();
    void on_edit_sale_triggered();
    void on_edit_retain_yes_triggered();
    void on_edit_retain_no_triggered();
    void on_edit_retain_toggle_triggered();
    void on_edit_stockroom_no_triggered();
    void on_edit_stockroom_a_triggered();
    void on_edit_stockroom_b_triggered();
    void on_edit_stockroom_c_triggered();
    void on_edit_reserved_triggered();
    void on_edit_remark_add_triggered();
    void on_edit_remark_rem_triggered();
    void on_edit_remark_set_triggered();
    void on_edit_comment_add_triggered();
    void on_edit_comment_rem_triggered();
    void on_edit_comment_set_triggered();

    void on_edit_bl_catalog_triggered();
    void on_edit_bl_priceguide_triggered();
    void on_edit_bl_lotsforsale_triggered();
    void on_edit_bl_myinventory_triggered();

    void on_view_column_layout_save_triggered();
    void on_view_column_layout_list_load(const QString &layoutId);
    void addItem(BrickLink::InvItem *, uint);

    void setPrice(double);

signals:
    void selectionChanged(const Document::ItemList &);
    void currentChanged(Document::Item *);

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

    void print(bool aspdf);

private slots:
    void ensureLatestVisible();
    void documentRowsInserted(const QModelIndex &, int, int);
    void updateCaption();
    void updateSelection();
    void updateCurrent();
    void documentItemsChanged(const Document::ItemList &items, bool grave);

    void contextMenu(const QPoint &);
    void priceGuideUpdated(BrickLink::PriceGuide *);
    void updateErrorMask();

    void setMatchProgress(int, int);
    void setMatchFinished(QVector<const BrickLink::Item *>);

private:
    Document::ItemList exportCheck() const;
    void resizeColumnsToDefault();

private:
    Document *           m_doc;
    DocumentProxyModel * m_view;
    QItemSelectionModel *m_selection_model;
    Document::ItemList   m_selection;
    Document::Item *     m_current;
    QTableView *         w_list;
    HeaderView *         w_header;
    bool                 m_diff_mode;
    bool                 m_simple_mode;

    int                  m_latest_row;
    QTimer *             m_latest_timer;

    uint                 m_settopg_failcnt = 0;
    uint                 m_settopg_todocnt = 0;
    BrickLink::Time      m_settopg_time;
    BrickLink::Price     m_settopg_price;
    QMultiHash<BrickLink::PriceGuide *, Document::Item *> *m_settopg_list;
};
