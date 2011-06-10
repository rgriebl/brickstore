/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <cfloat>
#include <cmath>

#include <QLayout>
#include <QLabel>
#include <QApplication>
#include <QFileDialog>
#include <QValidator>
#include <QClipboard>
#include <QCursor>
#include <QTableView>
#include <QScrollArea>
#include <QHeaderView>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QPrinter>
#include <QPrintDialog>
#include <QProgressDialog>

#include "messagebox.h"
#include "config.h"
#include "framework.h"
#include "utility.h"
#include "currency.h"
#include "document.h"
#include "window.h"
#include "headerview.h"
#include "documentdelegate.h"
#include "report.h"
#include "bricklink/bricklink_setmatch.h"

#include "selectcolordialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "incompleteitemdialog.h"
#include "consolidateitemsdialog.h"
#include "selectreportdialog.h"



// should be a function, but MSVC.Net doesn't accept default
// template arguments for template functions...

template <typename TG, typename TS = TG> class setOrToggle {
public:
    static void toggle(Window *w, const QString &actiontext, TG(Document::Item::* getter)() const, void (Document::Item::* setter)(TS t), TS val1, TS val2)
    { set_or_toggle(w, true, actiontext, getter, setter, val1, val2); }
    static void set(Window *w, const QString &actiontext, TG(Document::Item::* getter)() const, void (Document::Item::* setter)(TS t), TS val)
    { set_or_toggle(w, false, actiontext, getter, setter, val, TG()); }

private:
    static void set_or_toggle(Window *w, bool toggle, const QString &actiontext, TG(Document::Item::* getter)() const, void (Document::Item::* setter)(TS t), TS val1, TS val2 = TG())
    {
        Document *doc = w->document();

        if (w->selection().isEmpty())
            return;

        //DisableUpdates wp ( w_list );
        doc->beginMacro();
        uint count = 0;

        foreach (Document::Item *pos, w->selection()) {
            if (toggle) {
                TG val = (pos->* getter)();

                if (val == val1)
                    val = val2;
                else if (val == val2)
                    val = val1;

                if (val != (pos->* getter)()) {
                    Document::Item item = *pos;

                    (item.* setter)(val);
                    doc->changeItem(pos, item);

                    count++;
                }
            }
            else {
                if ((pos->* getter)() != val1) {
                    Document::Item item = *pos;

                    (item.* setter)(val1);
                    doc->changeItem(pos, item);

                    count++;
                }
            }
        }
        doc->endMacro(actiontext.arg(count));
    }
};

class WindowProgress {
public:
    WindowProgress(QWidget *w, const QString &title = QString(), int total = 0)
        : m_w(w), m_reenabled(false)
    {
        QScrollArea *sa = qobject_cast<QScrollArea *>(w);
        m_w = sa ? sa->viewport() : w;
        m_upd_enabled = m_w->updatesEnabled();
        m_w->setUpdatesEnabled(false);
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        if (total) {
            m_progress = new QProgressDialog(title, QString(), 0, total, w->window());
            m_progress->setWindowModality(Qt::WindowModal);
            m_progress->setMinimumDuration(1500);
            m_progress->setValue(0);
        } else {
            m_progress = 0;
        }
    }

    ~WindowProgress()
    {
        stop();
    }

    void stop()
    {
        if (!m_reenabled) {
            delete m_progress;
            m_progress = 0;
            QApplication::restoreOverrideCursor();
            m_w->setUpdatesEnabled(m_upd_enabled);
            m_reenabled = true;
        }
    }

    void step(int d = 1)
    {
        if (m_progress)
            m_progress->setValue(m_progress->value() + d);
    }

private:
    QWidget * m_w;
    bool      m_upd_enabled : 1;
    bool      m_reenabled   : 1;
    QProgressDialog *m_progress;
};


class TableView : public QTableView {
public:
    TableView(QWidget *parent) : QTableView(parent) { }

protected:
    void keyPressEvent(QKeyEvent *e)
    {
        QTableView::keyPressEvent(e);
#if !defined(Q_OS_MAC)
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (state() != EditingState) {
                if (edit(currentIndex(), EditKeyPressed, e))
                    e->accept();
            }
        }
#endif
    }
};


class WindowBar : public QWidget {
    Q_OBJECT
public:
    WindowBar(QWidget *parent);

    void addWidget(QWidget *w);
    void removeWidget(QWidget *w);

    void setWidgetVisible(QWidget *w, bool show);
    bool isWidgetVisible(QWidget *w) const;

protected:
    void paintEvent(QPaintEvent *e);

private slots:
    void widgetWasDestroyed();

private:
    QList<QWidget *> widgets;
};


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////




Window::Window(Document *doc, QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);

    m_doc = doc;
    m_doc->setParent(this);

    m_view = new DocumentProxyModel(doc);
    m_latest_row = -1;
    m_latest_timer = new QTimer(this);
    m_latest_timer->setSingleShot(true);
    m_latest_timer->setInterval(400);
    m_current = 0;

    m_settopg_failcnt = 0;
    m_settopg_list = 0;
    m_settopg_time = BrickLink::PastSix;
    m_settopg_price = BrickLink::Average;
    m_simple_mode = false;
    m_diff_mode = false;

    w_list = new TableView(this);
    w_list->setHorizontalHeader(new HeaderView(Qt::Horizontal, w_list));
    w_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    w_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    w_list->setSortingEnabled(true);
    w_list->horizontalHeader()->setClickable(true);
    w_list->horizontalHeader()->setMovable(true);
    w_list->setAlternatingRowColors(true);
    w_list->setTabKeyNavigation(true);
    w_list->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    w_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    w_list->setContextMenuPolicy(Qt::CustomContextMenu);
    w_list->verticalHeader()->setResizeMode(QHeaderView::Fixed);
    w_list->verticalHeader()->hide();
    w_list->horizontalHeader()->setHighlightSections(false);
    w_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    setFocusProxy(w_list);

    w_list->setModel(m_view);
    m_selection_model = new QItemSelectionModel(m_view, this);
    w_list->setSelectionModel(m_selection_model);

    DocumentDelegate *dd = new DocumentDelegate(doc, m_view, w_list);
    w_list->setItemDelegate(dd);
    w_list->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(w_list));
    w_list->installEventFilter(this);

    int em = w_list->fontMetrics().width(QChar('m'));
    for (int i = 0; i < w_list->model()->columnCount(); i++) {
        int width = w_list->model()->headerData(i, Qt::Horizontal, Qt::UserRole).toInt();
        if (width)
            w_list->horizontalHeader()->resizeSection(i, (width < 0 ? -width : width * em) + 8);
    }

    setDifferenceMode(false);

    /*
    w_list->setShowSortIndicator ( true );

    if ( doc->doNotSortItems ( ))
     w_list->setSorting ( w_list->columns ( ) + 1 );
*/


    QBoxLayout *toplay = new QVBoxLayout(this);
    toplay->setSpacing(0);
    toplay->setMargin(0);
    toplay->addWidget(w_list, 10);

    connect(m_latest_timer, SIGNAL(timeout()), this, SLOT(ensureLatestVisible()));
    connect(w_list, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(contextMenu(const QPoint &)));

    connect(m_selection_model, SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(updateSelection()));
    connect(m_selection_model, SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(updateCurrent()));

    connect(BrickLink::core(), SIGNAL(priceGuideUpdated(BrickLink::PriceGuide *)), this, SLOT(priceGuideUpdated(BrickLink::PriceGuide *)));

    connect(Config::inst(), SIGNAL(showInputErrorsChanged(bool)), this, SLOT(updateErrorMask()));
    connect(Config::inst(), SIGNAL(measurementSystemChanged(QLocale::MeasurementSystem)), w_list->viewport(), SLOT(update()));

    //connect(Config::inst(), SIGNAL(localCurrencyChanged()), w_list->viewport(), SLOT(update()));

    connect(m_doc, SIGNAL(titleChanged(const QString &)), this, SLOT(updateCaption()));
    connect(m_doc, SIGNAL(modificationChanged(bool)), this, SLOT(updateCaption()));

    connect(m_doc, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(documentRowsInserted(const QModelIndex &, int, int)));

    connect(m_doc, SIGNAL(itemsChanged(const Document::ItemList &, bool)), this, SLOT(documentItemsChanged(const Document::ItemList &, bool)));

    updateErrorMask();
    updateCaption();

    setFocusProxy(w_list);
}

void Window::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        updateCaption();
    }
}

void Window::updateCaption()
{
    QString cap = m_doc->title();

    if (cap.isEmpty())
        cap = m_doc->fileName();
    if (cap.isEmpty())
        cap = tr("Untitled");

    cap += QLatin1String("[*]");

    setWindowTitle(cap);
    setWindowModified(m_doc->isModified());
}

Window::~Window()
{
}

bool Window::isSimpleMode() const
{
    return m_simple_mode;
}

void Window::setSimpleMode(bool b)
{
    HeaderView *header = qobject_cast<HeaderView *>(w_list->horizontalHeader());

    if (!header)
        return;
    m_simple_mode = b;

    header->setSectionAvailable(Document::Bulk, !b);
    header->setSectionAvailable(Document::Sale, !b);
    header->setSectionAvailable(Document::TierQ1, !b);
    header->setSectionAvailable(Document::TierQ2, !b);
    header->setSectionAvailable(Document::TierQ3, !b);
    header->setSectionAvailable(Document::TierP1, !b);
    header->setSectionAvailable(Document::TierP2, !b);
    header->setSectionAvailable(Document::TierP3, !b);
    header->setSectionAvailable(Document::Reserved, !b);
    header->setSectionAvailable(Document::Stockroom, !b);
    header->setSectionAvailable(Document::Retain, !b);
    header->setSectionAvailable(Document::LotId, !b);
    header->setSectionAvailable(Document::Comments, !b);

    updateErrorMask();
}

bool Window::isDifferenceMode() const
{
    return m_diff_mode;
}

void Window::setDifferenceMode(bool b)
{
    HeaderView *header = qobject_cast<HeaderView *>(w_list->horizontalHeader());

    if (!header)
        return;

    m_diff_mode = b;

    header->setSectionAvailable(Document::PriceOrig, b);
    header->setSectionAvailable(Document::PriceDiff, b);
    header->setSectionAvailable(Document::QuantityOrig, b);
    header->setSectionAvailable(Document::QuantityDiff, b);

    if (b) {
        if (!header->isSectionHidden(Document::Quantity)) {
            header->showSection(Document::QuantityDiff);
            header->showSection(Document::QuantityOrig);

            header->moveSection(header->visualIndex(Document::QuantityDiff), header->visualIndex(Document::Quantity));
            header->moveSection(header->visualIndex(Document::QuantityOrig), header->visualIndex(Document::QuantityDiff));
        }

        if (!header->isSectionHidden(Document::Price)) {
            header->showSection(Document::PriceDiff);
            header->showSection(Document::PriceOrig);

            header->moveSection(header->visualIndex(Document::PriceDiff), header->visualIndex(Document::Price));
            header->moveSection(header->visualIndex(Document::PriceOrig), header->visualIndex(Document::PriceDiff));
        }
    }
}


void Window::on_view_save_default_col_triggered()
{
    //TODO w_list->saveDefaultLayout ( );
}

void Window::documentRowsInserted(const QModelIndex &parent, int /*start*/, int end)
{
    if (!parent.isValid()) {
        m_latest_row = end;
        m_latest_timer->start();
    }
}

void Window::ensureLatestVisible()
{
    if (m_latest_row >= 0) {
        w_list->scrollTo(m_view->index(m_latest_row, 0));
        m_latest_row = -1;
    }
}


void Window::setFilter(const QString &str)
{
    m_view->setFilterExpression(str);
}

QString Window::filter() const
{
    return m_view->filterExpression();
}

QString Window::filterToolTip() const
{
    return m_view->filterToolTip();
}

uint Window::addItems(const BrickLink::InvItemList &items, int multiply, uint globalmergeflags, bool /*dont_change_sorting*/)
{
    bool waitcursor = (items.count() > 100);
    bool was_empty = (w_list->model()->rowCount() == 0);
    uint dropped = 0;
    int merge_action_yes_no_to_all = MergeAction_Ask;
    int mergecount = 0, addcount = 0;

    if (items.count() > 1)
        m_doc->beginMacro();

    // disable sorting: new items should appear at the end of the list
    /*
     if ( !dont_change_sorting )
    //TODO  w_list->setSorting ( w_list->columns ( ) + 1 ); // +1 because of Qt-bug !!
    */
    if (waitcursor)
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    WindowProgress wp(w_list);

    foreach(const BrickLink::InvItem *origitem, items) {
        uint mergeflags = globalmergeflags;

        Document::Item *newitem = new Document::Item(*origitem);

        if (newitem->isIncomplete()) {
            IncompleteItemDialog dlg(newitem, this);

            if (waitcursor)
                QApplication::restoreOverrideCursor();

            bool drop_this = (dlg.exec() != QDialog::Accepted);

            if (waitcursor)
                QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            if (drop_this) {
                dropped++;
                continue;
            }
        }

        Document::Item *olditem = 0;

        if (mergeflags != MergeAction_None) {
            foreach(Document::Item *item, m_doc->items()) {
                if ((newitem->item() == item->item()) &&
                    (newitem->color() == item->color()) &&
                    (newitem->status() == item->status()) &&
                    (newitem->condition() == item->condition())) {
                    olditem = item;
                    break;
                }
            }
            if (!olditem)
                mergeflags = MergeAction_None;
        }

        if ((mergeflags & MergeAction_Mask) == MergeAction_Ask) {
            if (merge_action_yes_no_to_all != MergeAction_Ask) {
                mergeflags = merge_action_yes_no_to_all;
            }
            else {
                ConsolidateItemsDialog dlg(olditem, newitem, ((mergeflags & MergeKeep_Mask) == MergeKeep_Old), this);

                int res = dlg.exec();

                mergeflags = (res == QDialog::Accepted) ? MergeAction_Force : MergeAction_None;

                if (mergeflags != MergeAction_None)
                    mergeflags |= (dlg.attributesFromExisting() ? MergeKeep_Old : MergeKeep_New);

                if (dlg.yesNoToAll())
                    merge_action_yes_no_to_all = mergeflags;
            }
        }

        switch (mergeflags & MergeAction_Mask) {
        case MergeAction_Force: {
            newitem->mergeFrom(*olditem, ((mergeflags & MergeKeep_Mask) != MergeKeep_New));

            m_doc->changeItem(olditem, *newitem);
            delete newitem;
            mergecount++;
            break;
        }
        case MergeAction_None:
        default: {
            if (multiply > 1)
                newitem->setQuantity(newitem->quantity() * multiply);

            m_doc->insertItem(0, newitem);
            addcount++;
            break;
        }
        }
    }
    wp.stop();

    if (waitcursor)
        QApplication::restoreOverrideCursor();

    if (items.count() > 1)
        m_doc->endMacro(tr("Added %1, merged %2 items").arg(addcount).arg(mergecount));

    if (was_empty)
        w_list->selectRow(0);

    return items.count() - dropped;
}


void Window::addItem(BrickLink::InvItem *item, uint mergeflags)
{
    BrickLink::InvItemList tmplist;
    tmplist.append(item);

    addItems(tmplist, 1, mergeflags);

    delete item;
}


void Window::mergeItems(const Document::ItemList &items, int globalmergeflags)
{
    if ((items.count() < 2) || (globalmergeflags & MergeAction_Mask) == MergeAction_None)
        return;

    int merge_action_yes_no_to_all = MergeAction_Ask;
    uint mergecount = 0;

    m_doc->beginMacro();

    WindowProgress wp(w_list);

    foreach(Document::Item *from, items) {
        Document::Item *to = 0;

        foreach(Document::Item *find_to, items) {
            if ((from != find_to) &&
                (from->item() == find_to->item()) &&
                (from->color() == find_to->color())&&
                (from->condition() == find_to->condition()) &&
                ((from->status() == BrickLink::Exclude) == (find_to->status() == BrickLink::Exclude)) &&
                (m_doc->items().indexOf(find_to) != -1)) {
                to = find_to;
                break;
            }
        }
        if (!to)
            continue;

        uint mergeflags = globalmergeflags;

        if ((mergeflags & MergeAction_Mask) == MergeAction_Ask) {
            if (merge_action_yes_no_to_all != MergeAction_Ask) {
                mergeflags = merge_action_yes_no_to_all;
            }
            else {
                ConsolidateItemsDialog dlg(to, from, ((mergeflags & MergeKeep_Mask) == MergeKeep_Old), this);

                int res = dlg.exec();

                mergeflags = (res ? MergeAction_Force : MergeAction_None);

                if (mergeflags != MergeAction_None)
                    mergeflags |= (dlg.attributesFromExisting() ? MergeKeep_Old : MergeKeep_New);

                if (dlg.yesNoToAll())
                    merge_action_yes_no_to_all = mergeflags;
            }
        }

        if ((mergeflags & MergeAction_Mask) == MergeAction_Force) {
            bool prefer_from = ((mergeflags & MergeKeep_Mask) == MergeKeep_New);

            Document::Item newitem = *to;
            newitem.mergeFrom(*from, prefer_from);
            m_doc->changeItem(to, newitem);
            m_doc->removeItem(from);

            mergecount++;
        }
    }
    m_doc->endMacro(tr("Merged %1 items").arg(mergecount));
}

QDomElement Window::createGuiStateXML(QDomDocument doc)
{
    int version = 1;

    QDomElement root = doc.createElement("GuiState");
    root.setAttribute("Application", "BrickStore");
    root.setAttribute("Version", version);

    if (isDifferenceMode())
        root.appendChild(doc.createElement("DifferenceMode"));

    QMap <QString, QString> list_map; //TODO: = w_list->saveSettings ( );

    if (!list_map.isEmpty()) {
        QDomElement e = doc.createElement("ItemView");
        root.appendChild(e);

        for (QMap <QString, QString>::const_iterator it = list_map.begin(); it != list_map.end(); ++it)
            e.appendChild(doc.createElement(it.key()).appendChild(doc.createTextNode(it.value())).parentNode());
    }

    return root;
}

bool Window::parseGuiStateXML(QDomElement root)
{
    bool ok = true;

    if ((root.nodeName() == "GuiState") &&
        (root.attribute("Application") == "BrickStore") &&
        (root.attribute("Version").toInt() == 1)) {
        for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
            if (!n.isElement())
                continue;
            QString tag = n.toElement().tagName();

            if (tag == "DifferenceMode") {
                on_view_difference_mode_toggled(true);
            }
            else if (tag == "ItemView") {
                QMap <QString, QString> list_map;

                for (QDomNode niv = n.firstChild(); !niv.isNull(); niv = niv.nextSibling()) {
                    if (!niv.isElement())
                        continue;
                    list_map [niv.toElement().tagName()] = niv.toElement().text();
                }

                //if (!list_map.isEmpty())
                //TODO: w_list->loadSettings ( list_map );
            }
        }

    }

    return ok;
}


void Window::on_edit_cut_triggered()
{
    on_edit_copy_triggered();
    on_edit_delete_triggered();
}

void Window::on_edit_copy_triggered()
{
    if (!selection().isEmpty())
        QApplication::clipboard()->setMimeData(new BrickLink::InvItemMimeData(selection()));
}

void Window::on_edit_paste_triggered()
{
    BrickLink::InvItemList bllist = BrickLink::InvItemMimeData::items(QApplication::clipboard()->mimeData());

    if (bllist.count()) {
        if (!selection().isEmpty()) {
            if (MessageBox::question(this, tr("Overwrite the currently selected items?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
                m_doc->removeItems(selection());
        }
        addItems(bllist, 1, MergeAction_Ask | MergeKeep_New);
        qDeleteAll(bllist);
    }
}

void Window::on_edit_delete_triggered()
{
    if (!selection().isEmpty())
        m_doc->removeItems(selection());
}

void Window::on_edit_select_all_triggered()
{
    w_list->selectAll();
}

void Window::on_edit_select_none_triggered()
{
    w_list->clearSelection();
}

void Window::on_edit_reset_diffs_triggered()
{
    WindowProgress wp(w_list);

    m_doc->resetDifferences(selection());
}


void Window::on_edit_status_include_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, tr("Set 'include' status on %1 items"), &Document::Item::status, &Document::Item::setStatus, BrickLink::Include);
}

void Window::on_edit_status_exclude_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, tr("Set 'exclude' status on %1 items"), &Document::Item::status, &Document::Item::setStatus, BrickLink::Exclude);
}

void Window::on_edit_status_extra_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, tr("Set 'extra' status on %1 items"), &Document::Item::status, &Document::Item::setStatus, BrickLink::Extra);
}

void Window::on_edit_status_toggle_triggered()
{
    setOrToggle<BrickLink::Status>::toggle(this, tr("Toggled status on %1 items"), &Document::Item::status, &Document::Item::setStatus, BrickLink::Include, BrickLink::Exclude);
}

void Window::on_edit_cond_new_triggered()
{
    setOrToggle<BrickLink::Condition>::set(this, tr("Set 'new' condition on %1 items"), &Document::Item::condition, &Document::Item::setCondition, BrickLink::New);
}

void Window::on_edit_cond_used_triggered()
{
    setOrToggle<BrickLink::Condition>::set(this, tr("Set 'used' condition on %1 items"), &Document::Item::condition, &Document::Item::setCondition, BrickLink::Used);
}

void Window::on_edit_cond_toggle_triggered()
{
    setOrToggle<BrickLink::Condition>::toggle(this, tr("Toggled condition on %1 items"), &Document::Item::condition, &Document::Item::setCondition, BrickLink::New, BrickLink::Used);
}

void Window::on_edit_subcond_none_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, tr("Set 'none' sub-condition on %1 items"), &Document::Item::subCondition, &Document::Item::setSubCondition, BrickLink::None);
}

void Window::on_edit_subcond_misb_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, tr("Set 'MISB' sub-condition on %1 items"), &Document::Item::subCondition, &Document::Item::setSubCondition, BrickLink::MISB);
}

void Window::on_edit_subcond_complete_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, tr("Set 'complete' sub-condition on %1 items"), &Document::Item::subCondition, &Document::Item::setSubCondition, BrickLink::Complete);
}

void Window::on_edit_subcond_incomplete_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, tr("Set 'incomplete' sub-condition on %1 items"), &Document::Item::subCondition, &Document::Item::setSubCondition, BrickLink::Incomplete);
}

void Window::on_edit_retain_yes_triggered()
{
    setOrToggle<bool>::set(this, tr("Set 'retain' flag on %1 items"), &Document::Item::retain, &Document::Item::setRetain, true);
}

void Window::on_edit_retain_no_triggered()
{
    setOrToggle<bool>::set(this, tr("Cleared 'retain' flag on %1 items"), &Document::Item::retain, &Document::Item::setRetain, false);
}

void Window::on_edit_retain_toggle_triggered()
{
    setOrToggle<bool>::toggle(this, tr("Toggled 'retain' flag on %1 items"), &Document::Item::retain, &Document::Item::setRetain, true, false);
}

void Window::on_edit_stockroom_yes_triggered()
{
    setOrToggle<bool>::set(this, tr("Set 'stockroom' flag on %1 items"), &Document::Item::stockroom, &Document::Item::setStockroom, true);
}

void Window::on_edit_stockroom_no_triggered()
{
    setOrToggle<bool>::set(this, tr("Cleared 'stockroom' flag on %1 items"), &Document::Item::stockroom, &Document::Item::setStockroom, false);
}

void Window::on_edit_stockroom_toggle_triggered()
{
    setOrToggle<bool>::toggle(this, tr("Toggled 'stockroom' flag on %1 items"), &Document::Item::stockroom, &Document::Item::setStockroom, true, false);
}


void Window::on_edit_price_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString price = Currency::toString(selection().front()->price(), m_doc->currencyCode(), Currency::NoSymbol);

    if (MessageBox::getString(this, tr("Enter the new price for all selected items:"), Currency::localSymbol(m_doc->currencyCode()), price, new CurrencyValidator(0, 10000, 3, 0))) {
        setOrToggle<double>::set(this, tr("Set price on %1 items"), &Document::Item::price, &Document::Item::setPrice, Currency::fromString(price));
    }
}

void Window::on_edit_price_round_triggered()
{
    if (selection().isEmpty())
        return;

    uint roundcount = 0;
    m_doc->beginMacro();

    WindowProgress wp(w_list);

    foreach(Document::Item *pos, selection()) {
        double p = ((pos->price() + double(0.005)) / 10) * 10;

        if (p != pos->price()) {
            Document::Item item = *pos;

            item.setPrice(p);
            m_doc->changeItem(pos, item);

            roundcount++;
        }
    }
    m_doc->endMacro(tr("Price change on %1 items").arg(roundcount));
}


void Window::on_edit_price_to_priceguide_triggered()
{
    if (selection().isEmpty())
        return;

    if (m_settopg_list) {
        MessageBox::information(this, tr("Prices are currently updated to Price Guide values.<br /><br />Please wait until this operation has finished."));
        return;
    }

    SetToPriceGuideDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        WindowProgress wp(w_list, tr("Verifying Price Guide data"), selection().count());

        m_settopg_list    = new QMultiHash<BrickLink::PriceGuide *, Document::Item *>();
        m_settopg_failcnt = 0;
        m_settopg_time    = dlg.time();
        m_settopg_price   = dlg.price();
        bool force_update = dlg.forceUpdate();

        foreach(Document::Item *item, selection()) {
            BrickLink::PriceGuide *pg = BrickLink::core()->priceGuide(item->item(), item->color());

            if (force_update && pg && (pg->updateStatus() != BrickLink::Updating))
                pg->update();

            if (pg && (pg->updateStatus() == BrickLink::Updating)) {
                m_settopg_list->insert(pg, item);
                pg->addRef();
            }
            else if (pg && pg->valid()) {
                double p = pg->price(m_settopg_time, item->condition(), m_settopg_price);

                if (p != item->price()) {
                    Document::Item newitem = *item;
                    newitem.setPrice(p);
                    m_doc->changeItem(item, newitem);
                }
            }
            else {
                Document::Item newitem = *item;
                newitem.setPrice(0);
                m_doc->changeItem(item, newitem);

                m_settopg_failcnt++;
            }
            wp.step();
        }
        wp.stop();

        if (m_settopg_list && m_settopg_list->isEmpty())
            priceGuideUpdated(0);
    }
}

void Window::priceGuideUpdated(BrickLink::PriceGuide *pg)
{
    if (m_settopg_list && pg) {
        foreach(Document::Item *item, m_settopg_list->values(pg)) {
            double p = pg->valid() ? pg->price(m_settopg_time, item->condition(), m_settopg_price) : 0;

            if (p != item->price()) {
                Document::Item newitem = *item;
                newitem.setPrice(p);
                m_doc->changeItem(item, newitem);
            }
            pg->release();
        }
        m_settopg_list->remove(pg);
    }

    if (m_settopg_list && m_settopg_list->isEmpty()) {
        QString s = tr("Prices of the selected items have been updated to Price Guide values.");

        if (m_settopg_failcnt)
            s += "<br /><br />" + tr("%1 have been skipped, because of missing Price Guide records and/or network errors.").arg(CMB_BOLD(QString::number(m_settopg_failcnt)));

        MessageBox::information(this, s);

        m_settopg_failcnt = 0;
        delete m_settopg_list;
        m_settopg_list = 0;
    }
}


void Window::on_edit_price_inc_dec_triggered()
{
    if (selection().isEmpty())
        return;

    IncDecPricesDialog dlg(m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);
        bool tiers       = dlg.applyToTiers();
        uint incdeccount = 0;

        m_doc->beginMacro();

        WindowProgress wp(w_list);

        foreach(Document::Item *pos, selection()) {
            Document::Item item = *pos;

            double p = item.price();

            if (percent != 0)     p *= factor;
            else if (fixed != 0)  p += fixed;

            item.setPrice(p);

            if (tiers) {
                for (uint i = 0; i < 3; i++) {
                    p = item.tierPrice(i);

                    if (percent != 0)     p *= factor;
                    else if (fixed != 0)  p += fixed;

                    item.setTierPrice(i, p);
                }
            }
            m_doc->changeItem(pos, item);
            incdeccount++;
        }

        m_doc->endMacro(tr("Price change on %1 items").arg(incdeccount));
    }
}

void Window::on_edit_qty_divide_triggered()
{
    if (selection().isEmpty())
        return;

    int divisor = 1;

    if (MessageBox::getInteger(this, tr("Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisible without reminder, before this operation is performed.)"), QString::null, divisor, new QIntValidator(1, 1000, 0))) {
        if (divisor > 1) {
            int lots_with_errors = 0;

            foreach(Document::Item *item, selection()) {
                if (qAbs(item->quantity()) % divisor)
                    lots_with_errors++;
            }

            if (lots_with_errors) {
                MessageBox::information(this, tr("The quantities of %1 lots are not divisible without remainder by %2.<br /><br />Nothing has been modified.").arg(lots_with_errors).arg(divisor));
            }
            else {
                uint divcount = 0;
                m_doc->beginMacro();

                WindowProgress wp(w_list);

                foreach(Document::Item *pos, selection()) {
                    Document::Item item = *pos;

                    item.setQuantity(item.quantity() / divisor);
                    m_doc->changeItem(pos, item);

                    divcount++;
                }
                m_doc->endMacro(tr("Quantity divide by %1 on %2 Items").arg(divisor).arg(divcount));
            }
        }
    }
}

void Window::on_edit_qty_multiply_triggered()
{
    if (selection().isEmpty())
        return;

    int factor = 1;

    if (MessageBox::getInteger(this, tr("Multiply the quantities of all selected items with this factor."), tr("x"), factor, new QIntValidator(-1000, 1000, 0))) {
        if ((factor <= 1) || (factor > 1)) {
            uint mulcount = 0;
            m_doc->beginMacro();

            WindowProgress wp(w_list);

            foreach(Document::Item *pos, selection()) {
                Document::Item item = *pos;

                item.setQuantity(item.quantity() * factor);
                m_doc->changeItem(pos, item);

                mulcount++;
            }
            m_doc->endMacro(tr("Quantity multiply by %1 on %2 Items").arg(factor).arg(mulcount));
        }
    }
}

void Window::on_edit_sale_triggered()
{
    if (selection().isEmpty())
        return;

    int sale = selection().front()->sale();

    if (MessageBox::getInteger(this, tr("Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed."), tr("%"), sale, new QIntValidator(-1000, 99, 0)))
        setOrToggle<int>::set(this, tr("Set sale on %1 items"), &Document::Item::sale, &Document::Item::setSale, sale);
}

void Window::on_edit_bulk_triggered()
{
    if (selection().isEmpty())
        return;

    int bulk = selection().front()->bulkQuantity();

    if (MessageBox::getInteger(this, tr("Set bulk quantity for the selected items:"), QString(), bulk, new QIntValidator(1, 99999, 0)))
        setOrToggle<int>::set(this, tr("Set bulk quantity on %1 items"), &Document::Item::bulkQuantity, &Document::Item::setBulkQuantity, bulk);
}

void Window::on_edit_color_triggered()
{
    if (selection().isEmpty())
        return;

    SelectColorDialog d(this);
    d.setWindowTitle(tr("Modify Color"));
    d.setColor(selection().front()->color());

    if (d.exec() == QDialog::Accepted)
        setOrToggle<const BrickLink::Color *>::set(this, tr( "Set color on %1 items" ), &Document::Item::color, &Document::Item::setColor, d.color());
}

void Window::on_edit_remark_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString remarks = selection().front()->remarks();

    if (MessageBox::getString(this, tr("Enter the new remark for all selected items:"), remarks))
        setOrToggle<QString, const QString &>::set(this, tr("Set remark on %1 items"), &Document::Item::remarks, &Document::Item::setRemarks, remarks);
}

void Window::on_edit_remark_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addremarks;

    if (MessageBox::getString(this, tr("Enter the text, that should be added to the remarks of all selected items:"), addremarks)) {
        uint remarkcount = 0;
        m_doc->beginMacro();

        QRegExp regexp("\\b" + QRegExp::escape(addremarks) + "\\b");

        WindowProgress wp(w_list, tr("Changing remarks"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->remarks();

            if (str.isEmpty())
                str = addremarks;
            else if (str.indexOf(regexp) != -1)
                ;
            else if (addremarks.indexOf(QRegExp("\\b" + QRegExp::escape(str) + "\\b")) != -1)
                str = addremarks;
            else
                str = str + " " + addremarks;

            if (str != pos->remarks()) {
                Document::Item item = *pos;

                item.setRemarks(str);
                m_doc->changeItem(pos, item);

                remarkcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified remark on %1 items").arg(remarkcount));
    }
}

void Window::on_edit_remark_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remremarks;

    if (MessageBox::getString(this, tr("Enter the text, that should be removed from the remarks of all selected items:"), remremarks)) {
        uint remarkcount = 0;
        m_doc->beginMacro();

        QRegExp regexp("\\b" + QRegExp::escape(remremarks) + "\\b");

        WindowProgress wp(w_list, tr("Changing remarks"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->remarks();
            str.remove(regexp);
            str = str.simplified();

            if (str != pos->remarks()) {
                Document::Item item = *pos;

                item.setRemarks(str);
                m_doc->changeItem(pos, item);

                remarkcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified remark on %1 items").arg(remarkcount));
    }
}


void Window::on_edit_comment_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString comments = selection().front()->comments();

    if (MessageBox::getString(this, tr("Enter the new comment for all selected items:"), comments))
        setOrToggle<QString, const QString &>::set(this, tr("Set comment on %1 items"), &Document::Item::comments, &Document::Item::setComments, comments);
}

void Window::on_edit_comment_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addcomments;

    if (MessageBox::getString(this, tr("Enter the text, that should be added to the comments of all selected items:"), addcomments)) {
        uint commentcount = 0;
        m_doc->beginMacro();

        QRegExp regexp("\\b" + QRegExp::escape(addcomments) + "\\b");

        WindowProgress wp(w_list, tr("Changing comments"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->comments();

            if (str.isEmpty())
                str = addcomments;
            else if (str.indexOf(regexp) != -1)
                ;
            else if (addcomments.indexOf(QRegExp("\\b" + QRegExp::escape(str) + "\\b")) != -1)
                str = addcomments;
            else
                str = str + " " + addcomments;

            if (str != pos->comments()) {
                Document::Item item = *pos;

                item.setComments(str);
                m_doc->changeItem(pos, item);

                commentcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified comment on %1 items").arg(commentcount));
    }
}

void Window::on_edit_comment_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remcomments;

    if (MessageBox::getString(this, tr("Enter the text, that should be removed from the comments of all selected items:"), remcomments)) {
        uint commentcount = 0;
        m_doc->beginMacro();

        QRegExp regexp("\\b" + QRegExp::escape(remcomments) + "\\b");

        WindowProgress wp(w_list, tr("Changing comments"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->comments();
            str.remove(regexp);
            str = str.simplified();

            if (str != pos->comments()) {
                Document::Item item = *pos;

                item.setComments(str);
                m_doc->changeItem(pos, item);

                commentcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified comment on %1 items").arg(commentcount));
    }
}


void Window::on_edit_reserved_triggered()
{
    if (selection().isEmpty())
        return;

    QString reserved = selection().front()->reserved();

    if (MessageBox::getString(this, tr("Reserve all selected items for this specific buyer (BrickLink username):"), reserved))
        setOrToggle<QString, const QString &>::set(this, tr("Set reservation on %1 items"), &Document::Item::reserved, &Document::Item::setReserved, reserved);
}

void Window::updateErrorMask()
{
    quint64 em = 0;

    if (Config::inst()->showInputErrors())
        em = (1ULL << Document::FieldCount) - 1;

    m_doc->setErrorMask(em);
}

void Window::on_edit_copyremarks_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Please choose the document that should serve as a source to fill in the remarks fields of the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        BrickLink::InvItemList list = dlg.items();

        if (!list.isEmpty())
            copyRemarks(list);

        qDeleteAll(list);
    }
}

void Window::copyRemarks(const BrickLink::InvItemList &items)
{
    if (items.isEmpty())
        return;

    m_doc->beginMacro();

    WindowProgress wp(w_list);

    int copy_count = 0;

    foreach(Document::Item *pos, m_doc->items()) {
        BrickLink::InvItem *match = 0;

        foreach(BrickLink::InvItem *ii, items) {
            const BrickLink::Item *item   = ii->item();
            const BrickLink::Color *color = ii->color();
            BrickLink::Condition cond     = ii->condition();
            int qty                       = ii->quantity();

            if (!item || !color || !qty)
                continue;

            if ((pos->item() == item) && (pos->condition() == cond)) {
                if (pos->color() == color)
                    match = ii;
                else if (!match)
                    match = ii;
            }
        }

        if (match && !match->remarks().isEmpty()) {
            Document::Item newitem = *pos;
            newitem.setRemarks(match->remarks());
            m_doc->changeItem(pos, newitem);

            copy_count++;
        }
    }
    m_doc->endMacro(tr("Copied Remarks for %1 Items").arg(copy_count));
}

void Window::on_edit_subtractitems_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Which items should be subtracted from the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        BrickLink::InvItemList list = dlg.items();

        if (!list.isEmpty())
            subtractItems(list);

        qDeleteAll(list);
    }
}

void Window::subtractItems(const BrickLink::InvItemList &items)
{
    if (items.isEmpty())
        return;

    m_doc->beginMacro();

    WindowProgress wp(w_list, tr("Subtracting items"), items.count() * m_doc->items().count());

    foreach(BrickLink::InvItem *ii, items) {
        const BrickLink::Item *item   = ii->item();
        const BrickLink::Color *color = ii->color();
        BrickLink::Condition cond     = ii->condition();
        int qty                       = ii->quantity();

        if (!item || !color || !qty) {
            wp.step(m_doc->items().count());
            continue;
        }

        Document::Item *last_match = 0;

        foreach(Document::Item *pos, m_doc->items()) {
            if ((pos->item() == item) && (pos->color() == color) && (pos->condition() == cond)) {
                Document::Item newitem = *pos;

                if (pos->quantity() >= qty) {
                    newitem.setQuantity(pos->quantity() - qty);
                    qty = 0;
                }
                else {
                    qty -= pos->quantity();
                    newitem.setQuantity(0);
                }
                m_doc->changeItem(pos, newitem);
                last_match = pos;
            }
            wp.step();
        }

        if (qty) {   // still a qty left
            if (last_match) {
                Document::Item newitem = *last_match;
                newitem.setQuantity(last_match->quantity() - qty);
                m_doc->changeItem(last_match, newitem);
            }
            else {
                Document::Item *newitem = new Document::Item();
                newitem->setItem(item);
                newitem->setColor(color);
                newitem->setCondition(cond);
                newitem->setOrigQuantity(0);
                newitem->setQuantity(-qty);

                m_doc->insertItem(0, newitem);
            }
        }
    }
    m_doc->endMacro(tr("Subtracted %1 Items").arg(items.count()));
}

void Window::on_edit_mergeitems_triggered()
{
    if (!selection().isEmpty())
        mergeItems(selection());
    else
        mergeItems(m_doc->items());
}

void Window::on_edit_partoutitems_triggered()
{
    if (selection().count() >= 1) {
        bool inplace = (MessageBox::question(this, tr("Should the selected items be parted out into the current document, replacing the selected items?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes);

        if (inplace)
            m_doc->beginMacro();

        WindowProgress wp(w_list, tr("Parting out items"), inplace ? selection().count() : 0);
        int partcount = 0;

        foreach(Document::Item *item, selection()) {
            if (inplace) {
                if (item->item()->hasInventory()) {
                    BrickLink::InvItemList items = item->item()->consistsOf();
                    if (!items.isEmpty()) {
                        int multiply = item->quantity();
                        int startpos = m_doc->positionOf(item);
                        QVector<int> positions(items.size());
                        for (int i = 0; i < items.size(); ++i) {
                            positions[i] = startpos + i;
                            if (multiply != 1)
                                items[i]->setQuantity(items[i]->quantity() * multiply);
                        }
                        m_doc->insertItems(positions, Document::ItemList(items));
                        m_doc->removeItem(item);
                        partcount++;
                    }
                    qDeleteAll(items);
                }
            } else {
                FrameWork::inst()->fileImportBrickLinkInventory(item->item());
            }
            wp.step();
        }
        if (inplace)
            m_doc->endMacro(tr("Parted out %1 Items").arg(partcount));
    }
    else
        QApplication::beep();
}

void Window::setMatchProgress(int /*pmax*/, int /*pcur*/)
{
    fputc('.', stderr);
}

void Window::setMatchFinished(QList<const BrickLink::Item *> result)
{
    foreach (const BrickLink::Item *item, result)
        qWarning("SetMatch: %s [%s]", item->name(), item->id());
}

void Window::on_edit_setmatch_triggered()
{
#if 0
    if (!m_doc->items().isEmpty()) {
    {
        BrickLink::SetMatch *sm = new BrickLink::SetMatch(m_doc);

        sm->setRecursiveBound(0.05f);
        sm->setGreedyPreference(BrickLink::SetMatch::PreferLargerSets);

        sm->setItemTypeConstraint(QList<const BrickLink::ItemType *>() << BrickLink::core()->itemType('S'));
        sm->setPartCountConstraint(100, -1);
        sm->setYearReleasedConstraint(1995, -1);

        connect(sm, SIGNAL(finished(QList<const BrickLink::Item *>)), this, SLOT(setMatchFinished(QList<const BrickLink::Item *>)));
        connect(sm, SIGNAL(progress(int, int)), this, SLOT(setMatchProgress(int, int)));

        if (!sm->startMaximumPossibleSetMatch(selection().isEmpty() ? m_doc->items() : selection(), BrickLink::SetMatch::Recursive))
            qWarning("SetMatch (Recursive): error.");
    } else
#endif
    {
        QApplication::beep();
    }
}

void Window::setPrice(double d)
{
    if (selection().count() == 1) {
        Document::Item *pos = selection().front();
        Document::Item item = *pos;

        item.setPrice(d);
        m_doc->changeItem(pos, item);
    }
    else
        QApplication::beep();
}

void Window::contextMenu(const QPoint &p)
{
    FrameWork::inst()->showContextMenu(true, w_list->viewport()->mapToGlobal(p));
}

void Window::on_file_close_triggered()
{
    close();
}

void Window::closeEvent(QCloseEvent *e)
{
    bool close_empty = (m_doc->items().isEmpty() && Config::inst()->closeEmptyDocuments());

    if (m_doc->isModified() && !close_empty) {
        switch (MessageBox::warning(this, tr("Save changes to %1?").arg(CMB_BOLD(windowTitle().replace(QLatin1String("[*]"), QString()))), MessageBox::Yes | MessageBox::No | MessageBox::Cancel, MessageBox::Yes)) {
        case MessageBox::Yes:
            m_doc->fileSave();

            if (!m_doc->isModified())
                e->accept();
            else
                e->ignore();
            break;

        case MessageBox::No:
            e->accept();
            break;

        default:
            e->ignore();
            break;
        }
    }
    else {
        e->accept();
    }

}

void Window::on_edit_bl_catalog_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo, (*selection().front()).item()));
}

void Window::on_edit_bl_priceguide_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, (*selection().front()).item(), (*selection().front()).color()));
}

void Window::on_edit_bl_lotsforsale_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale, (*selection().front()).item(), (*selection().front()).color()));
}

void Window::on_edit_bl_myinventory_triggered()
{
    if (!selection().isEmpty()) {
        uint lotid = (*selection().front()).lotId();
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_StoreItemDetail, &lotid));
    }
}

void Window::on_view_difference_mode_toggled(bool b)
{
    setDifferenceMode(b);
}

void Window::on_file_print_triggered()
{
    print(false);
}

void Window::on_file_print_pdf_triggered()
{
    print(true);
}

void Window::print(bool as_pdf)
{
    if (m_doc->items().isEmpty())
        return;

    if (ReportManager::inst()->reports().isEmpty()) {
        ReportManager::inst()->reload();

        if (ReportManager::inst()->reports().isEmpty())
            MessageBox::warning(this, tr("Couldn't find any print templates."), MessageBox::Ok);
            return;
    }

    QPrinter *prt = ReportManager::inst()->printer();

    if (!prt )
        return;

    QString doctitle = m_doc->title();
    if (doctitle == m_doc->fileName())
        doctitle = QFileInfo(doctitle).baseName();

    if (as_pdf) {
        QString pdfname = doctitle + QLatin1String(".pdf");
        pdfname = QFileDialog::getSaveFileName(FrameWork::inst(), tr("Save PDF as"), pdfname, tr("PDF Documents (*.pdf)"));

        if (pdfname.isEmpty())
            return;

        prt->setOutputFileName(pdfname);
        prt->setOutputFormat(QPrinter::PdfFormat);
    }
    else {
        QPrintDialog pd(prt, FrameWork::inst());

        pd.addEnabledOption(QAbstractPrintDialog::PrintToFile);
        pd.addEnabledOption(QAbstractPrintDialog::PrintCollateCopies);
        pd.addEnabledOption(QAbstractPrintDialog::PrintShowPageSize);

        if (!selection().isEmpty())
            pd.addEnabledOption(QAbstractPrintDialog::PrintSelection);

        //pd.setPrintRange(m_doc->selection().isEmpty() ? QAbstractPrintDialog::AllPages : QAbstractPrintDialog::Selection);

        if (pd.exec() != QDialog::Accepted)
            return;
    }

    prt->setDocName(doctitle);
    prt->setFullPage(true);

    SelectReportDialog d(this);

    if (d.exec() != QDialog::Accepted)
        return;

    const Report *rep = d.report();

    if (!rep)
        return;

    rep->print(prt, m_doc, m_view->sortItemList(prt->printRange() == QPrinter::Selection ? selection() : m_doc->items()));
}
void Window::on_file_save_triggered()
{
    m_doc->fileSave();
}

void Window::on_file_saveas_triggered()
{
    m_doc->fileSaveAs();
}

void Window::on_file_export_bl_xml_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        m_doc->fileExportBrickLinkXML(items);
}

void Window::on_file_export_bl_xml_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        m_doc->fileExportBrickLinkXMLClipboard(items);
}

void Window::on_file_export_bl_update_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        m_doc->fileExportBrickLinkUpdateClipboard(items);
}

void Window::on_file_export_bl_invreq_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        m_doc->fileExportBrickLinkInvReqClipboard(items);
}

void Window::on_file_export_bl_wantedlist_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        m_doc->fileExportBrickLinkWantedListClipboard(items);
}

Document::ItemList Window::exportCheck() const
{
    Document::ItemList items = m_doc->items();

    if (!selection().isEmpty() && (selection().count() != m_doc->items().count())) {
        if (MessageBox::question(FrameWork::inst(), tr("There are %1 items selected.<br /><br />Do you want to export only these items?").arg(selection().count()), MessageBox::Yes | MessageBox::No, MessageBox::No) == MessageBox::Yes) {
            items = selection();
        }
    }

    if (m_doc->statistics(items).errors()) {
        if (MessageBox::warning(FrameWork::inst(), tr("This list contains items with errors.<br /><br />Do you really want to export this list?"), MessageBox::Yes | MessageBox::No, MessageBox::No) != MessageBox::Yes)
            items.clear();
    }

    return m_view->sortItemList(items);
}


bool Window::eventFilter(QObject *o, QEvent *e)
{
    if (o == w_list && e->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(e)->key() == Qt::Key_Space) {
            FrameWork::inst()->toggleItemDetailPopup();
            e->accept();
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}

void Window::updateCurrent()
{
    QModelIndex idx = m_selection_model->currentIndex();
    m_current = idx.isValid() ? m_view->item(idx) : 0;
    emit currentChanged(m_current);
}

void Window::updateSelection()
{
    m_selection.clear();

    foreach (const QModelIndex &idx, m_selection_model->selectedRows())
        m_selection.append(m_view->item(idx));

    emit selectionChanged(m_selection);
}

void Window::setSelection(const Document::ItemList &lst)
{
    QItemSelection idxs;

    foreach (const Document::Item *item, lst) {
        QModelIndex idx(m_view->index(item));
        idxs.select(idx, idx);
    }
    m_selection_model->select(idxs, QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current | QItemSelectionModel::Rows);
}

void Window::documentItemsChanged(const Document::ItemList &items, bool grave)
{
    if (!items.isEmpty() && grave) {
        if (items.contains(m_current))
            emit currentChanged(m_current);
        foreach (Document::Item *item, m_selection) {
            if (items.contains(item)) {
                emit selectionChanged(m_selection);
                break;
            }
        }
    }
}
