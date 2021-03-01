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

#include <functional>

#include <QWidget>
#include <QHash>
#include <qdom.h>

#include "bricklinkfwd.h"
#include "document.h"
#include "config.h"
#include "currency.h"

QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)
class FrameWork;
class UndoStack;
class HeaderView;
class StatusBar;
class ColumnChangeWatcher;
class WindowProgress;


class Window : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool blockingOperationActive READ isBlockingOperationActive NOTIFY blockingOperationActiveChanged)
    Q_PROPERTY(bool blockingOperationCancelable READ isBlockingOperationCancelable NOTIFY blockingOperationCancelableChanged)
    Q_PROPERTY(QString blockingOperationTitle READ blockingOperationTitle WRITE setBlockingOperationTitle NOTIFY blockingOperationTitleChanged)

public:
    Window(Document *doc, QWidget *parent = nullptr);
    ~Window() override;

    Document *document() const { return m_doc; }

    enum class Consolidate {
        Not = -1,
        IntoTopSorted = 0,
        IntoBottomSorted = 1,
        IntoLowestIndex = 2,
        IntoHighestIndex = 3,
        IntoExisting = 4,
        IntoNew = 5
    };

    enum class AddItemMode {
        AddAsNew,
        ConsolidateWithExisting,
        ConsolidateInteractive,
    };

    static int restorableAutosaves();
    enum class AutosaveAction { Restore, Delete };
    static const QVector<Window *> processAutosaves(AutosaveAction action);

    const Document::ItemList &selection() const  { return m_selection; }

    uint setItems(const BrickLink::InvItemList &items, int multiply = 1);
    int addItems(const BrickLink::InvItemList &items, AddItemMode addItemMode = AddItemMode::AddAsNew);
    void deleteItems(const BrickLink::InvItemList &items);

    void consolidateItems(const Document::ItemList &items);

    QDomElement createGuiStateXML();
    void applyGuiStateXML(const QDomElement &root, bool &changedColumns, bool &changedSortFilter);

    enum class ColumnLayoutCommand {
        BrickStoreDefault,
        BrickStoreSimpleDefault,
        AutoResize,
        UserDefault,
        User,
    };

    static std::vector<ColumnLayoutCommand> columnLayoutCommands();
    static QString columnLayoutCommandName(ColumnLayoutCommand clc);
    static QString columnLayoutCommandId(ColumnLayoutCommand clc);
    static ColumnLayoutCommand columnLayoutCommandFromId(const QString &id);

    QByteArray currentColumnLayout() const;


    bool isBlockingOperationActive() const;
    void startBlockingOperation(const QString &title, std::function<void()> cancelCallback = { });
    void endBlockingOperation();

    bool isBlockingOperationCancelable() const;
    void setBlockingOperationCancelCallback(std::function<void()> cancelCallback);
    void cancelBlockingOperation();

    QString blockingOperationTitle() const;
    void setBlockingOperationTitle(const QString &title);
public slots:
    void setSelection(const Document::ItemList &);

    void on_document_save_triggered();
    void on_document_save_as_triggered();
    void on_document_export_bl_xml_triggered();
    void on_document_export_bl_xml_clip_triggered();
    void on_document_export_bl_update_clip_triggered();
    void on_document_export_bl_invreq_clip_triggered();
    void on_document_export_bl_wantedlist_clip_triggered();

    void on_document_print_triggered();
    void on_document_print_pdf_triggered();
    void on_document_close_triggered();

    void on_edit_cut_triggered();
    void on_edit_copy_triggered();
    void on_edit_paste_triggered();
    void on_edit_delete_triggered();

    void on_edit_subtractitems_triggered();
    void on_edit_copy_fields_triggered();
    void on_edit_mergeitems_triggered();
    void on_edit_partoutitems_triggered();

    void on_edit_select_all_triggered();
    void on_edit_select_none_triggered();

    void on_edit_filter_from_selection_triggered();

    void on_edit_status_include_triggered();
    void on_edit_status_exclude_triggered();
    void on_edit_status_extra_triggered();
    void on_edit_status_toggle_triggered();
    void on_edit_cond_new_triggered();
    void on_edit_cond_used_triggered();
    void on_edit_subcond_none_triggered();
    void on_edit_subcond_sealed_triggered();
    void on_edit_subcond_complete_triggered();
    void on_edit_subcond_incomplete_triggered();
    void on_edit_color_triggered();
    void on_edit_item_triggered();
    void on_edit_qty_set_triggered();
    void on_edit_qty_multiply_triggered();
    void on_edit_qty_divide_triggered();
    void on_edit_price_set_triggered();
    void on_edit_price_round_triggered();
    void on_edit_price_to_priceguide_triggered();
    void on_edit_price_inc_dec_triggered();
    void on_edit_cost_set_triggered();
    void on_edit_cost_round_triggered();
    void on_edit_cost_inc_dec_triggered();
    void on_edit_cost_spread_triggered();
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
    void on_edit_remark_clear_triggered();
    void on_edit_comment_add_triggered();
    void on_edit_comment_rem_triggered();
    void on_edit_comment_set_triggered();
    void on_edit_comment_clear_triggered();

    void on_bricklink_catalog_triggered();
    void on_bricklink_priceguide_triggered();
    void on_bricklink_lotsforsale_triggered();
    void on_bricklink_myinventory_triggered();

    void on_view_reset_diff_mode_triggered();

    void on_view_column_layout_save_triggered();
    void on_view_column_layout_list_load(const QString &layoutId);

    void gotoNextErrorOrDifference(bool difference = false);

    void setStatus(BrickLink::Status status);
    void setCondition(BrickLink::Condition condition);
    void setSubCondition(BrickLink::SubCondition subCondition);
    void setRetain(bool retain);
    void setStockroom(BrickLink::Stockroom stockroom);

signals:
    void selectionChanged(const Document::ItemList &);
    void blockingOperationActiveChanged(bool blocked);
    void blockingOperationCancelableChanged(bool cancelable);
    void blockingOperationTitleChanged(const QString &title);
    void blockingOperationProgress(int done, int total);

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;

    void print(bool aspdf);

private slots:
    void ensureLatestVisible();
    void updateCaption();
    void updateSelection();
    void documentItemsChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    void contextMenu(const QPoint &pos);
    void priceGuideUpdated(BrickLink::PriceGuide *);
    void updateItemFlagsMask();

    void autosave() const;

private:
    void applyTo(const Document::ItemList &items,
                 std::function<bool(const Document::Item &, Document::Item &)> callback);

    void cancelPriceGuideUpdates();

    Document::ItemList exportCheck() const;
    void resizeColumnsToDefault(bool simpleMode = false);
    int consolidateItemsHelper(const Document::ItemList &items, Consolidate conMode) const;
    void deleteAutosave();

    void moveColumnDirect(int logical, int oldVisual, int newVisual);
    void resizeColumnDirect(int logical, int oldSize, int newSize);
    friend class ColumnCmd;

private:
    Document *           m_doc;
    QItemSelectionModel *m_selection_model;
    Document::ItemList   m_selection;
    QTimer *             m_delayedSelectionUpdate = nullptr;
    QMenu *              m_contextMenu = nullptr;
    StatusBar *          w_statusbar;
    QTableView *         w_list;
    HeaderView *         w_header;
    bool                 m_diff_mode;
    ColumnChangeWatcher *m_ccw = nullptr;

    int                  m_latest_row;
    QTimer *             m_latest_timer;

    bool                 m_blocked = false;
    QString              m_blockTitle;
    std::function<void()> m_blockCancelCallback;

    struct SetToPriceGuideData
    {
        std::vector<std::pair<Document::Item *, Document::Item>> changes;
        QMultiHash<BrickLink::PriceGuide *, Document::Item *>    priceGuides;
        int              failCount = 0;
        int              doneCount = 0;
        int              totalCount = 0;
        BrickLink::Time  time;
        BrickLink::Price price;
        double           currencyRate;
        bool             canceled = false;
    };
    QScopedPointer<SetToPriceGuideData> m_setToPG;

    QUuid                m_uuid;  // for autosave
    QTimer               m_autosave_timer;
};

Q_DECLARE_METATYPE(Window::Consolidate)
