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

#include "bricklink/bricklinkfwd.h"
#include "document.h"
#include "lot.h"
#include "config.h"
#include "utility/currency.h"

QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)
QT_FORWARD_DECLARE_CLASS(QPrinter)
QT_FORWARD_DECLARE_CLASS(QMenu)
class FrameWork;
class UndoStack;
class TableView;
class HeaderView;
class StatusBar;
class ColumnChangeWatcher;
class WindowProgress;
class PrintingScriptAction;


class View : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool blockingOperationActive READ isBlockingOperationActive NOTIFY blockingOperationActiveChanged)
    Q_PROPERTY(bool blockingOperationCancelable READ isBlockingOperationCancelable NOTIFY blockingOperationCancelableChanged)
    Q_PROPERTY(QString blockingOperationTitle READ blockingOperationTitle WRITE setBlockingOperationTitle NOTIFY blockingOperationTitleChanged)

public:
    View(Document *doc, const QByteArray &columnLayout = { },
           const QByteArray &sortFilterState = { }, QWidget *parent = nullptr);
    ~View() override;

    static const QVector<View *> &allViews();

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

    enum class AddLotMode {
        AddAsNew,
        ConsolidateWithExisting,
        ConsolidateInteractive,
    };

    static int restorableAutosaves();
    enum class AutosaveAction { Restore, Delete };
    static const QVector<View *> processAutosaves(AutosaveAction action);

    const LotList &selectedLots() const  { return m_selectedLots; }

    int addLots(const LotList &lots, AddLotMode addLotMode = AddLotMode::AddAsNew);

    void consolidateLots(const LotList &lots);

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
    void setSelection(const LotList &);

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
    void on_edit_paste_silent_triggered();
    void on_edit_duplicate_triggered();
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
    void on_edit_marker_text_triggered();
    void on_edit_marker_color_triggered();
    void on_edit_marker_clear_triggered();

    void on_bricklink_catalog_triggered();
    void on_bricklink_priceguide_triggered();
    void on_bricklink_lotsforsale_triggered();
    void on_bricklink_myinventory_triggered();

    void on_view_reset_diff_mode_triggered();

    void on_view_column_layout_save_triggered();
    void on_view_column_layout_list_load(const QString &layoutId);

    void on_view_goto_next_diff_triggered();
    void on_view_goto_next_input_error_triggered();
    void gotoNextErrorOrDifference(bool difference = false);

    void setStatus(BrickLink::Status status);
    void setCondition(BrickLink::Condition condition);
    void setSubCondition(BrickLink::SubCondition subCondition);
    void setRetain(bool retain);
    void setStockroom(BrickLink::Stockroom stockroom);

    void printScriptAction(PrintingScriptAction *printingAction);

signals:
    void selectedLotsChanged(const LotList &);
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
    void updateMinimumSectionSize();
    void updateSelection();
    void documentDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    void contextMenu(const QPoint &pos);
    void priceGuideUpdated(BrickLink::PriceGuide *);
    void updateItemFlagsMask();

    void autosave() const;

private:
    void applyTo(const LotList &lots, std::function<bool(const Lot &, Lot &)> callback);

    void cancelPriceGuideUpdates();

    void editCurrentItem(int column);

    LotList exportCheck() const;
    void resizeColumnsToDefault(bool simpleMode = false);
    int consolidateLotsHelper(const LotList &lots, Consolidate conMode) const;
    void deleteAutosave();

    void moveColumnDirect(int logical, int oldVisual, int newVisual);
    void resizeColumnDirect(int logical, int oldSize, int newSize);
    friend class ColumnCmd;

    bool printPages(QPrinter *prt, const LotList &lots, const QList<uint> &pages, double scaleFactor,
                    uint *maxPageCount, double *maxWidth);

private:
    Document *           m_doc;
    QItemSelectionModel *m_selection_model;
    LotList              m_selectedLots;
    QTimer *             m_delayedSelectionUpdate = nullptr;
    QMenu *              m_contextMenu = nullptr;
    StatusBar *          w_statusbar;
    TableView *          w_list;
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
        std::vector<std::pair<Lot *, Lot>> changes;
        QMultiHash<BrickLink::PriceGuide *, Lot *>    priceGuides;
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
    QTimer               m_autosaveTimer;
    mutable bool         m_autosaveClean = true;

    static QVector<View *> s_views;

    friend class AutosaveJob;
};

Q_DECLARE_METATYPE(View::Consolidate)
