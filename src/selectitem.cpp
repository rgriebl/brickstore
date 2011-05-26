/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QComboBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QApplication>
#include <QCursor>
#include <QRegExp>
#include <QToolTip>
#include <QTreeView>
#include <QListView>
#include <QTableView>
#include <QHeaderView>
#include <QStackedLayout>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QFormLayout>
#include <QContextMenuEvent>

#include "bricklink.h"
#include "filteredit.h"
#include "selectitem.h"
#include "utility.h"
#include "messagebox.h"
#include "itemdetailpopup.h"


class SelectItemPrivate {
public:
    BrickLink::ItemTypeModel *itemTypeModel;
    BrickLink::CategoryModel *categoryModel;
    BrickLink::ItemModel *    itemModel;

    QLabel *         w_item_types_label;
    QComboBox *      w_item_types;
    QTreeView *      w_categories;
    QStackedLayout * m_stack;
    QTreeView *      w_items;
    QTreeView *      w_itemthumbs;
    QListView *      w_thumbs;
    FilterEdit *     w_filter;
    QComboBox *      w_viewmode;
    bool             m_inv_only;
    ItemDetailPopup *m_details;
};



class CategoryDelegate : public BrickLink::ItemDelegate {
public:
    CategoryDelegate(QObject *parent = 0)
        : BrickLink::ItemDelegate(parent, BrickLink::ItemDelegate::AlwaysShowSelection)
    { }

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QStyleOptionViewItemV4 myoption(option);
        if (index.isValid() && qvariant_cast<const BrickLink::Category *>(index.data(BrickLink::CategoryPointerRole)) == BrickLink::CategoryModel::AllCategories)
            myoption.font.setBold(true);
        BrickLink::ItemDelegate::paint(painter, myoption, index);
    }
};


class ItemThumbsDelegate : public BrickLink::ItemDelegate {
public:
    ItemThumbsDelegate(QObject *parent = 0)
        : BrickLink::ItemDelegate(parent, BrickLink::ItemDelegate::AlwaysShowSelection)
    { }

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (index.column() == 0) {
            const BrickLink::Item *item = qvariant_cast<const BrickLink::Item *>(index.data(BrickLink::ItemPointerRole));
            return item ? item->itemType()->pictureSize() : QSize(80, 60);
        }
        else
            return BrickLink::ItemDelegate::sizeHint(option, index);
    }
};



SelectItem::SelectItem(QWidget *parent)
    : QWidget(parent)
{
    d = new SelectItemPrivate();
    d->m_inv_only = false;
    init();
}

void SelectItem::init()
{
    d->m_details = 0;

    d->w_item_types_label = new QLabel(this);
    d->w_item_types = new QComboBox(this);
    d->w_item_types->setEditable(false);

    d->w_categories = new QTreeView(this);
    d->w_categories->setSortingEnabled(true);
    d->w_categories->setAlternatingRowColors(true);
    d->w_categories->setAllColumnsShowFocus(true);
    d->w_categories->setUniformRowHeights(true);
    d->w_categories->setRootIsDecorated(false);
    d->w_categories->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_categories->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_categories->setItemDelegate(new CategoryDelegate);

    d->w_filter = new FilterEdit(this);

    d->w_viewmode = new QComboBox(this);
    d->w_viewmode->addItem(QString());
    d->w_viewmode->addItem(QString());
    d->w_viewmode->addItem(QString());
    connect(d->w_viewmode, SIGNAL(currentIndexChanged(int)), this, SLOT(setViewMode(int)));

    d->w_items = new QTreeView(this);
    d->w_items->setSortingEnabled(true);
    d->w_items->setAlternatingRowColors(true);
    d->w_items->setAllColumnsShowFocus(true);
    d->w_items->setUniformRowHeights(true);
    d->w_items->setRootIsDecorated(false);
    d->w_items->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_items->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_items->setItemDelegate(new BrickLink::ItemDelegate(this, BrickLink::ItemDelegate::AlwaysShowSelection));
    d->w_items->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_items->installEventFilter(this);

    d->w_itemthumbs = new QTreeView(this);
    d->w_itemthumbs->setSortingEnabled(true);
    d->w_itemthumbs->setAlternatingRowColors(true);
    d->w_itemthumbs->setAllColumnsShowFocus(true);
    d->w_itemthumbs->setUniformRowHeights(true);
    d->w_itemthumbs->setWordWrap(true);
    d->w_itemthumbs->setRootIsDecorated(false);
    d->w_itemthumbs->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_itemthumbs->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_itemthumbs->setItemDelegate(new ItemThumbsDelegate);
    d->w_itemthumbs->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_itemthumbs->installEventFilter(this);

    d->w_thumbs = new QListView(this);
    d->w_thumbs->setMovement(QListView::Static);
    d->w_thumbs->setViewMode(QListView::IconMode);
    d->w_thumbs->setLayoutMode(QListView::Batched);
    d->w_thumbs->setResizeMode(QListView::Adjust);
    d->w_thumbs->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_thumbs->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_thumbs->setTextElideMode(Qt::ElideRight);
    d->w_thumbs->setItemDelegate(new ItemThumbsDelegate);
    d->w_thumbs->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_thumbs->installEventFilter(this);

    d->itemTypeModel = new BrickLink::ItemTypeModel(this);
    d->categoryModel = new BrickLink::CategoryModel(this);
    d->itemModel = new BrickLink::ItemModel(this);

    d->w_item_types->setModel(d->itemTypeModel);
    d->w_categories->setModel(d->categoryModel);
    d->w_categories->sortByColumn(0, Qt::AscendingOrder);
    d->w_items->setModel(d->itemModel);
    d->w_items->sortByColumn(2, Qt::AscendingOrder);
    d->w_items->hideColumn(0);
    d->w_itemthumbs->setModel(d->itemModel);
    d->w_thumbs->setModel(d->itemModel);
    d->w_thumbs->setModelColumn(0);

    d->w_itemthumbs->setSelectionModel(d->w_items->selectionModel());
    d->w_thumbs->setSelectionModel(d->w_items->selectionModel());

    connect(d->w_filter, SIGNAL(textChanged(const QString &)), this, SLOT(applyFilter()));

    connect(d->w_item_types, SIGNAL(currentIndexChanged(int)), this, SLOT(itemTypeChanged()));
    connect(d->w_categories->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(categoryChanged()));

    connect(d->w_items->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(itemChanged()));
    connect(d->w_items, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_itemthumbs, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_items, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_itemthumbs, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_items, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(d->w_itemthumbs, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(d->w_thumbs, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));

    QGridLayout *lay = new QGridLayout(this);
    lay->setMargin(0);
    lay->setColumnStretch(1, 25);
    lay->setColumnStretch(2, 75);
    lay->setRowStretch(0, 0);
    lay->setRowStretch(1, 1);

    lay->addWidget(d->w_item_types_label, 0, 0);
    lay->addWidget(d->w_item_types, 0, 1);

    lay->addWidget(d->w_categories, 1, 0, 1, 2);

    lay->addWidget(d->w_filter, 0, 2);
    lay->addWidget(d->w_viewmode, 0, 3);

    d->m_stack = new QStackedLayout();
    lay->addLayout(d->m_stack, 1, 2, 1, 2);

    d->m_stack->addWidget(d->w_items);
    d->m_stack->addWidget(d->w_itemthumbs);
    d->m_stack->addWidget(d->w_thumbs);

    d->m_stack->setCurrentWidget(d->w_items);

    setFocusProxy(d->w_item_types);
    languageChange();
}

void SelectItem::languageChange()
{
    d->w_item_types_label->setText(tr("Item type:"));
    d->w_filter->setToolTip(tr("Filter the list using this pattern (wildcards allowed: * ? [])"));
    d->w_filter->setPlaceholderText(tr("Filter"));

    d->w_viewmode->setItemText(0, tr("List"));
    d->w_viewmode->setItemText(1, tr("List with Images"));
    d->w_viewmode->setItemText(2, tr("Thumbnails"));
}

bool SelectItem::eventFilter(QObject *o, QEvent *e)
{
    if ((o == d->w_items || o == d->w_itemthumbs || o == d->w_thumbs) &&
        (e->type() == QEvent::KeyPress)) {
        if (static_cast<QKeyEvent *>(e)->key() == Qt::Key_Space) {
            if (!d->m_details)
                d->m_details = new ItemDetailPopup(this);

            if (!d->m_details->isVisible()) {
                d->m_details->setItem(currentItem());
                d->m_details->show();
            } else {
                d->m_details->hide();
                d->m_details->setItem(0);
            }
            e->accept();
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}

bool SelectItem::hasExcludeWithoutInventoryFilter() const
{
    return d->m_inv_only;
}

void SelectItem::setExcludeWithoutInventoryFilter(bool b)
{
    if (b != d->m_inv_only) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        const BrickLink::ItemType *oldType = currentItemType();
        const BrickLink::Item *oldItem = currentItem();
        d->w_categories->clearSelection();
        d->w_items->clearSelection();

        d->itemTypeModel->setFilterWithoutInventory(b);
        d->itemModel->setFilterWithoutInventory(b);

        setCurrentItemType(oldType);
        setCurrentItem(oldItem, true);
        QApplication::restoreOverrideCursor();

        d->m_inv_only = b;
    }
}

void SelectItem::itemTypeChanged()
{
    const BrickLink::ItemType *itemtype = currentItemType();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    const BrickLink::Category *oldCat = currentCategory();
    const BrickLink::Item *oldItem = currentItem();
    d->w_categories->clearSelection();
    d->w_items->clearSelection();

    d->categoryModel->setFilterItemType(itemtype);
    d->itemModel->setFilterItemType(itemtype);

    setCurrentCategory(oldCat);
    setCurrentItem(oldItem);

    emit hasColors(itemtype->hasColors());
    QApplication::restoreOverrideCursor();
}

const BrickLink::ItemType *SelectItem::currentItemType() const
{
    if (d->w_item_types->currentIndex() >= 0) {
        QModelIndex idx = d->itemTypeModel->index(d->w_item_types->currentIndex(), 0);
        return d->itemTypeModel->itemType(idx);
    } else {
        return 0;
    }
}

bool SelectItem::setCurrentItemType(const BrickLink::ItemType *it)
{
    QModelIndex idx = d->itemTypeModel->index(it);
    d->w_item_types->setCurrentIndex(idx.isValid() ? idx.row() : -1);
    return idx.isValid();
}


void SelectItem::categoryChanged()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    const BrickLink::Item *oldItem = currentItem();
    d->w_items->clearSelection();

    d->itemModel->setFilterCategory(currentCategory());

    setCurrentItem(oldItem);
    QApplication::restoreOverrideCursor();
}

const BrickLink::Category *SelectItem::currentCategory() const
{
    QModelIndexList idxlst = d->w_categories->selectionModel()->selectedRows();
    return idxlst.isEmpty() ? 0 : d->categoryModel->category(idxlst.front());
}

bool SelectItem::setCurrentCategory(const BrickLink::Category *cat)
{
    QModelIndex idx = d->categoryModel->index(cat);
    if (idx.isValid())
        d->w_categories->setCurrentIndex(idx);
    else
        d->w_categories->clearSelection();
    return idx.isValid();
}

const BrickLink::Item *SelectItem::currentItem() const
{
    QModelIndexList idxlst = d->w_items->selectionModel()->selectedRows();
    return idxlst.isEmpty() ? 0 : d->itemModel->item(idxlst.front());
}

bool SelectItem::setCurrentItem(const BrickLink::Item *item, bool force_items_category)
{
    QModelIndex idx = d->itemModel->index(item);
    if (idx.isValid()) {
        const BrickLink::ItemType *itt = item->itemType();
        const BrickLink::Category *cat = item->category();

        if (currentItemType() != itt)
            setCurrentItemType(itt);
        if ((currentCategory() != cat) &&
            (force_items_category || currentCategory() != BrickLink::CategoryModel::AllCategories))
            setCurrentCategory(cat);
        d->w_items->setCurrentIndex(idx);
    }
    else
        d->w_items->clearSelection();
    return idx.isValid();
}

void SelectItem::setViewMode(int mode)
{
    QWidget *w = 0;
    switch (mode) {
    case 0 : w = d->w_items; break;
    case 1 : w = d->w_itemthumbs; break;
    case 2 : w = d->w_thumbs; break;
    default: break;
    }
    if (w) {
        d->m_stack->setCurrentWidget(w);
        d->w_viewmode->setCurrentIndex(mode);
    }
}


void SelectItem::findItem()
{
    if (const BrickLink::ItemType *itt = currentItemType()) {
        QString str;

        if (MessageBox::getString(this, tr("Please enter the complete item number:"), str)) {
            if (const BrickLink::Item *item = BrickLink::core()->item(itt->id(), str.toLatin1().constData())) {
                setCurrentItem(item);
                ensureSelectionVisible();
            } else {
                QApplication::beep();
            }
        }
    }
}

void SelectItem::itemChanged()
{
    emit itemSelected(currentItem(), false);
    if (d->m_details && d->m_details->isVisible())
        d->m_details->setItem(currentItem());
}

void SelectItem::itemConfirmed()
{
    const BrickLink::Item *item = currentItem();
    if (item)
        emit itemSelected(item, true);
}



void SelectItem::showEvent(QShowEvent *)
{
    ensureSelectionVisible();
}

void SelectItem::ensureSelectionVisible()
{
    const BrickLink::Category *cat = currentCategory();
    const BrickLink::Item *item = currentItem();

    if (cat)
        d->w_categories->scrollTo(d->categoryModel->index(cat), QAbstractItemView::PositionAtCenter);

    if (item) {
        QModelIndex idx = d->itemModel->index(item);

        d->w_items->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        d->w_itemthumbs->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        d->w_thumbs->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}


void SelectItem::applyFilter()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    const BrickLink::Item *oldItem = currentItem();
    d->w_items->clearSelection();

    d->itemModel->setFilterText(d->w_filter->text());

    setCurrentItem(oldItem);
    QApplication::restoreOverrideCursor();
}


QSize SelectItem::sizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return QSize(120 * fm.width('x'), 20 * fm.height());
}

void SelectItem::showContextMenu(const QPoint &p)
{
    if (QAbstractItemView *iv = qobject_cast<QAbstractItemView *>(sender())) {
        QModelIndex idx = iv->indexAt(p);

        if (idx.isValid()) {
            const BrickLink::Item *item = idx.model()->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();

            if (item && item->category() != currentCategory()) {
                QMenu m(this);
                QAction *gotocat = m.addAction(tr("View item's category"));

                if (m.exec(iv->mapToGlobal(p)) == gotocat) {
                    setCurrentItem(item, true);
                    ensureSelectionVisible();
                }
            }
        }
    }
}
