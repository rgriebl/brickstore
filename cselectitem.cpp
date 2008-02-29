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
#include <qlineedit.h>
#include <qlayout.h>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <qapplication.h>
#include <qcursor.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <QTreeView>
#include <QListView>
#include <QTableView>
#include <QHeaderView>
#include <QStackedLayout>
#include <QMenu>
#include <QStyledItemDelegate>

//#include "citemtypecombo.h"
//#include "cresource.h"
#include "cfilteredit.h"
#include "cselectitem.h"
#include "cutility.h"
#include "cmessagebox.h"








class CSelectItemPrivate {
public:
    QLabel *      w_item_types_label;
    QComboBox *   w_item_types;
    QTreeView *   w_categories;
    QStackedLayout *m_stack;
    QTreeView *   w_items;
    QTreeView *   w_itemthumbs;
    QListView *   w_thumbs;
    CFilterEdit * w_filter;
    QToolButton * w_viewbutton;
    QAction *     m_viewaction[3];
    QMenu *       w_viewmenu;
    CSelectItem::ViewMode m_viewmode;

    bool                   m_filter_active;
    bool                   m_inv_only;
    const BrickLink::Item *m_selected;
};


class CategoryDelegate : public QStyledItemDelegate {
public:
    CategoryDelegate(QObject *parent = 0)
            : QStyledItemDelegate(parent)
    { }

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (index.isValid() && qvariant_cast<const BrickLink::Category *>(index.data(BrickLink::CategoryPointerRole)) == BrickLink::CategoryModel::AllCategories) {
            QStyleOptionViewItem myoption(option);

            myoption.font.setBold(true);
            QStyledItemDelegate::paint(painter, myoption, index);
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
};


class ItemThumbsDelegate : public QStyledItemDelegate {
public:
    ItemThumbsDelegate(QObject *parent = 0)
            : QStyledItemDelegate(parent)
    { }

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (index.column() == 0) {
            const BrickLink::Item *item = qvariant_cast<const BrickLink::Item *>(index.data(BrickLink::ItemPointerRole));

            return item ? item->itemType()->pictureSize() : QSize(80, 60);
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }
};



CSelectItem::CSelectItem(QWidget *parent)
        : QWidget(parent)
{
    d = new CSelectItemPrivate();
    d->m_inv_only = false;
    init();
}

void CSelectItem::init()
{
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

    d->w_filter = new CFilterEdit(this);
    d->w_filter->setMenuIcon(QIcon(":/images/22x22/filter_menu"));
    d->w_filter->setClearIcon(QIcon(":/images/22x22/filter_clear"));

    d->w_viewmenu = new QMenu(this);
    QActionGroup *ag = new QActionGroup(this);
    ag->setExclusive(true);

    d->m_viewaction[ListMode] = new QAction(ag);
    d->m_viewaction[ListMode]->setIcon(QIcon(":/images/22x22/viewmode_list"));
    d->m_viewaction[ListMode]->setCheckable(true);
    connect(d->m_viewaction[ListMode], SIGNAL(triggered()), this, SLOT(showAsList()));
    d->m_viewaction[TableMode] = new QAction(ag);
    d->m_viewaction[TableMode]->setIcon(QIcon(":/images/22x22/viewmode_images"));
    d->m_viewaction[TableMode]->setCheckable(true);
    connect(d->m_viewaction[TableMode], SIGNAL(triggered()), this, SLOT(showAsTable()));
    d->m_viewaction[ThumbsMode] = new QAction(ag);
    d->m_viewaction[ThumbsMode]->setIcon(QIcon(":/images/22x22/viewmode_thumbs"));
    d->m_viewaction[ThumbsMode]->setCheckable(true);
    connect(d->m_viewaction[ThumbsMode], SIGNAL(triggered()), this, SLOT(showAsThumbs()));

    d->w_viewmenu->addActions(ag->actions());

    d->w_viewbutton = new QToolButton(this);
    d->w_viewbutton->setAutoRaise(true);
    d->w_viewbutton->setPopupMode(QToolButton::InstantPopup);
    d->w_viewbutton->setMenu(d->w_viewmenu);
    d->w_viewbutton->setIcon(QIcon(":/images/22x22/viewmode_list"));

    d->w_items = new QTreeView(this);
    d->w_items->setSortingEnabled(true);
    d->w_items->setAlternatingRowColors(true);
    d->w_items->setAllColumnsShowFocus(true);
    d->w_items->setUniformRowHeights(true);
    d->w_items->setRootIsDecorated(false);
    d->w_items->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_items->setSelectionMode(QAbstractItemView::SingleSelection);

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

    d->w_thumbs = new QListView(this);
    d->w_thumbs->setMovement(QListView::Static);
    d->w_thumbs->setViewMode(QListView::IconMode);
    d->w_thumbs->setLayoutMode(QListView::Batched);
    d->w_thumbs->setResizeMode(QListView::Adjust);
    d->w_thumbs->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_thumbs->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_thumbs->setTextElideMode(Qt::ElideRight);
    d->w_thumbs->setItemDelegate(new ItemThumbsDelegate);

    d->w_item_types->setModel(new BrickLink::ItemTypeProxyModel(BrickLink::core()->itemTypeModel()));
    d->w_categories->setModel(new BrickLink::CategoryProxyModel(BrickLink::core()->categoryModel()));
    d->w_items->setModel(new BrickLink::ItemProxyModel(BrickLink::core()->itemModel()));
    d->w_itemthumbs->setModel(d->w_items->model());
    d->w_thumbs->setModel(d->w_items->model());
    d->w_itemthumbs->setSelectionModel(d->w_items->selectionModel());
    d->w_thumbs->setSelectionModel(d->w_items->selectionModel());
    d->w_items->hideColumn(0);
    d->w_thumbs->setModelColumn(0);
    d->w_categories->sortByColumn(0, Qt::AscendingOrder);
    d->w_items->sortByColumn(2, Qt::AscendingOrder);

    connect(d->w_filter, SIGNAL(filterTextChanged(const QString &)), this, SLOT(applyFilter()));

    connect(d->w_item_types, SIGNAL(currentIndexChanged(int)), this, SLOT(itemTypeChanged()));
    connect(d->w_categories->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(categoryChanged()));

    connect(d->w_items->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(itemChanged()));
    connect(d->w_items, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_itemthumbs, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_items, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_itemthumbs, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemConfirmed()));

/*    connect(d->w_items, SIGNAL(contextMenuRequested(QListViewItem *, const QPoint &, int)), this, SLOT(itemContextList(QListViewItem *, const QPoint &)));
    connect(d->w_thumbs, SIGNAL(selectionChanged()), this, SLOT(itemChangedIcon()));
    connect(d->w_thumbs, SIGNAL(doubleClicked(QIconViewItem *)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(returnPressed(QIconViewItem *)), this, SLOT(itemConfirmed()));
    connect(d->w_thumbs, SIGNAL(contextMenuRequested(QIconViewItem *, const QPoint &)), this, SLOT(itemContextIcon(QIconViewItem *, const QPoint &)));*/
// connect ( d->w_viewpopup, SIGNAL( activated ( int )), this, SLOT( viewModeChanged ( int )));

    QGridLayout *toplay = new QGridLayout(this);
    toplay->setContentsMargins(0, 0, 0, 0);
    toplay->setColumnStretch(0, 25);
    toplay->setColumnStretch(1, 75);
    toplay->setRowStretch(0, 0);
    toplay->setRowStretch(1, 100);

    QBoxLayout *lay = new QHBoxLayout();
    lay->addWidget(d->w_item_types_label, 0);
    lay->addWidget(d->w_item_types, 1);

    toplay->addLayout(lay, 0, 0);
    toplay->addWidget(d->w_categories, 1, 0);

    lay = new QHBoxLayout();
    //lay->addWidget(d->w_goto, 0);
    //lay->addSpacing(6);
    //lay->addWidget(d->w_filter_clear, 0);
    //lay->addWidget(d->w_filter_label, 0);
    //lay->addWidget(d->w_filter_expression, 15);
    lay->addWidget(d->w_filter);
    lay->addSpacing(6);
    lay->addWidget(d->w_viewbutton);

    toplay->addLayout(lay, 0, 1);
    d->m_stack = new QStackedLayout();
    d->m_stack->addWidget(d->w_items);
    d->m_stack->addWidget(d->w_itemthumbs);
    d->m_stack->addWidget(d->w_thumbs);
    toplay->addLayout(d->m_stack, 1, 1);

    d->m_filter_active = false;

    d->m_stack->setCurrentWidget(d->w_items);
    d->m_viewmode = ListMode;

    d->m_selected = 0;

    languageChange();
    recalcHighlightPalette();

    setCurrentCategory(BrickLink::CategoryModel::AllCategories);
    setFocusProxy(d->w_item_types);
}


void CSelectItem::recalcHighlightPalette()
{
    QPalette p = qApp->palette();
    QColor hc = CUtility::gradientColor(p.color(QPalette::Active, QPalette::Highlight), p.color(QPalette::Inactive, QPalette::Highlight), 0.35f);
    QColor htc = p.color(QPalette::Active, QPalette::HighlightedText);

    p.setColor(QPalette::Inactive, QPalette::Highlight, hc);
    p.setColor(QPalette::Inactive, QPalette::HighlightedText, htc);
    setPalette(p);
}

void CSelectItem::languageChange()
{
    d->w_item_types_label->setText(tr("Item type:"));
//    d->w_filter_label->setText(tr("Filter:"));

//    d->w_goto->setShortcut(tr("Ctrl+F", "Find Item"));
//    d->w_goto->setToolTip(tr("Find Item...") + " (" + QString(d->w_goto->shortcut()) + ")");

//    d->w_filter_expression->setToolTip(tr("Filter the list using this pattern (wildcards allowed: * ? [])"));
//    d->w_filter_clear->setToolTip(tr("Reset an active filter"));
//    d->w_viewbutton->setToolTip(tr("View"));
    d->w_filter->setToolTip(tr("Filter the list using this pattern (wildcards allowed: * ? [])"));
    d->w_filter->setIdleText(tr("Filter"));

    d->m_viewaction[ListMode]->setText(tr("List"));
    d->m_viewaction[TableMode]->setText(tr("List with images"));
    d->m_viewaction[ThumbsMode]->setText(tr("Thumbnails"));
}

bool CSelectItem::hasExcludeWithoutInventoryFilter() const
{
    return d->m_inv_only;
}

void CSelectItem::setExcludeWithoutInventoryFilter(bool b)
{
    if (b != d->m_inv_only) {
        BrickLink::ItemTypeProxyModel *model1 = qobject_cast<BrickLink::ItemTypeProxyModel *>(d->w_item_types->model());
        BrickLink::ItemProxyModel *model2 = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());

        model1->setFilterWithoutInventory(b);
        model2->setFilterWithoutInventory(b);
    }
}

void CSelectItem::itemTypeChanged()
{
    const BrickLink::Category *oldcat = currentCategory();
    const BrickLink::Item *olditem = currentItem();
    const BrickLink::ItemType *itemtype = currentItemType();

    BrickLink::CategoryProxyModel *model = qobject_cast<BrickLink::CategoryProxyModel *>(d->w_categories->model());
    model->setFilterItemType(itemtype);
    setCurrentCategory(oldcat);

    BrickLink::ItemProxyModel *model2 = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());
    model2->setFilterItemType(itemtype);
    model2->setFilterCategory(currentCategory());
    setCurrentItem(olditem, true);

    emit hasColors(itemtype->hasColors());
}

const BrickLink::ItemType *CSelectItem::currentItemType() const
{
    BrickLink::ItemTypeProxyModel *model = qobject_cast<BrickLink::ItemTypeProxyModel *>(d->w_item_types->model());

    if (model && d->w_item_types->currentIndex() >= 0) {
        QModelIndex idx = model->index(d->w_item_types->currentIndex(), 0);
        return model->itemType(idx);
    }
    else
        return 0;
}

void CSelectItem::setCurrentItemType(const BrickLink::ItemType *it)
{
    BrickLink::ItemTypeProxyModel *model = qobject_cast<BrickLink::ItemTypeProxyModel *>(d->w_item_types->model());

    if (model) {
        QModelIndex idx = model->index(it);
        if (idx.isValid())
            d->w_item_types->setCurrentIndex(idx.row());
        else
            d->w_item_types->setCurrentIndex(-1);
    }
}


void CSelectItem::categoryChanged()
{
    const BrickLink::Item *olditem = currentItem();

    BrickLink::ItemProxyModel *model = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());
    model->setFilterCategory(currentCategory());
    setCurrentItem(olditem, true);
}

const BrickLink::Category *CSelectItem::currentCategory() const
{
    BrickLink::CategoryProxyModel *model = qobject_cast<BrickLink::CategoryProxyModel *>(d->w_categories->model());
    QModelIndexList idxlst = d->w_categories->selectionModel()->selectedRows();

    if (model && !idxlst.isEmpty())
        return model->category(idxlst.front());
    else
        return 0;
}

void CSelectItem::setCurrentCategory(const BrickLink::Category *cat)
{
    BrickLink::CategoryProxyModel *model = qobject_cast<BrickLink::CategoryProxyModel *>(d->w_categories->model());

    if (model) {
        QModelIndex idx = model->index(cat);
        if (idx.isValid())
            d->w_categories->selectionModel()->select(idx, QItemSelectionModel::SelectCurrent);
        else
            d->w_categories->clearSelection();
    }
}

const BrickLink::Item *CSelectItem::currentItem() const
{
    BrickLink::ItemProxyModel *model = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());
    QModelIndexList idxlst = d->w_items->selectionModel()->selectedRows();

    if (model && !idxlst.isEmpty())
        return model->item(idxlst.front());
    else
        return 0;
}

bool CSelectItem::setCurrentItem(const BrickLink::Item *item, bool dont_force_category)
{
    bool found = false;
    const BrickLink::ItemType *itt = item ? item->itemType() : 0;
    const BrickLink::Category *cat = item ? item->category() : 0;

    if (!dont_force_category) {
        setCurrentItemType(itt ? itt : BrickLink::core()->itemType('P'));
        setCurrentCategory(cat ? cat : BrickLink::CategoryModel::AllCategories);
    }

    BrickLink::ItemProxyModel *model = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());

    if (model) {
        QModelIndex idx = model->index(item);
        if (idx.isValid()) {
            d->w_items->selectionModel()->select(idx, QItemSelectionModel::SelectCurrent);
            found = true;
        }
        else
            d->w_items->clearSelection();
    }

    //TODO: check filter-> return true/false (found)

    return found;
}

void CSelectItem::showAsList()
{
    if (d->m_viewmode != ListMode) {
        d->m_stack->setCurrentWidget(d->w_items);
        d->m_viewmode = ListMode;
    }
}

void CSelectItem::showAsTable()
{
    if (d->m_viewmode != TableMode) {
        d->m_stack->setCurrentWidget(d->w_itemthumbs);
        d->m_viewmode = TableMode;
    }
}

void CSelectItem::showAsThumbs()
{
    if (d->m_viewmode != ThumbsMode) {
        d->m_stack->setCurrentWidget(d->w_thumbs);
        d->m_viewmode = ThumbsMode;
    }
}


void CSelectItem::findItem()
{
    const BrickLink::ItemType *itt = currentItemType();

    if (!itt)
        return;

    QString str;

    if (CMessageBox::getString(this, tr("Please enter the complete item number:"), str)) {
        const BrickLink::Item *item = BrickLink::core()->item(itt->id(), str.toAscii().constData());

        if (item) {
            setCurrentItem(item);
            ensureSelectionVisible();
        }
        else
            QApplication::beep();
    }
}

void CSelectItem::itemChanged()
{
    const BrickLink::Item *item = currentItem();

    emit itemSelected(item, false);
}

void CSelectItem::itemConfirmed()
{
    const BrickLink::Item *item = currentItem();

    if (item)
        emit itemSelected(item, true);
}



void CSelectItem::showEvent(QShowEvent *)
{
    ensureSelectionVisible();
}

void CSelectItem::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::PaletteChange)
        recalcHighlightPalette();
}


void CSelectItem::ensureSelectionVisible()
{
    const BrickLink::Category *cat = currentCategory();
    const BrickLink::Item *item = currentItem();

    if (cat) {
        BrickLink::CategoryProxyModel *model = qobject_cast<BrickLink::CategoryProxyModel *>(d->w_categories->model());

        d->w_categories->scrollTo(model->index(cat));
    }

    if (item) {
        /*  BrickLink::ItemModel *model = qobject_cast<BrickLink::ItemModel *>(d->w_items->model());

          d->w_items->scrollTo(model->index(item));
          d->w_thumbs->scrollTo(model->index(item));*/
    }
}


void CSelectItem::applyFilter()
{
    QRegExp regexp(d->w_filter->text(), Qt::CaseInsensitive, QRegExp::Wildcard);

    bool regexp_ok = !regexp.isEmpty() && regexp.isValid();

    if (!regexp_ok && !d->m_filter_active) {
        //qDebug ( "ignoring filter...." );
        return;
    }

    BrickLink::ItemProxyModel *model = qobject_cast<BrickLink::ItemProxyModel *>(d->w_items->model());

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    model->setFilterRegExp(regexp);
    QApplication::restoreOverrideCursor();

    d->m_filter_active = regexp_ok;
}


QSize CSelectItem::sizeHint() const
{
    QFontMetrics fm = fontMetrics();

    return QSize(120 * fm.width('x'), 20 * fm.height());
}

/*
void CSelectItem::itemContextList ( QListViewItem *lvi, const QPoint &pos )
{
 const BrickLink::Item *item = lvi ? static_cast <ItemListItem *> ( lvi )->item ( ) : 0;

 itemContext ( item, pos );
}

void CSelectItem::itemContextIcon ( QIconViewItem *ivi, const QPoint &pos )
{
 const BrickLink::Item *item = ivi ? static_cast <ItemIconItem *> ( ivi )->item ( ) : 0;

 itemContext ( item, pos );
}

void CSelectItem::itemContext ( const BrickLink::Item *item, const QPoint &pos )
{
 CatListItem *cli = static_cast <CatListItem *> ( d->w_categories->selectedItem ( ));
 const BrickLink::Category *cat = cli ? cli->category ( ) : 0;

 if ( !item || !item->category ( ) || ( item->category ( ) == cat ))
  return;

 QPopupMenu pop ( this );
 pop.insertItem ( tr( "View item's category" ), 0 );

 if ( pop.exec ( pos ) == 0 ) {
  setItem ( item );
  ensureSelectionVisible ( );
 }
}
*/
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

DSelectItem::DSelectItem(bool only_with_inventory, QWidget *parent)
        : QDialog(parent)
{
    w_si = new CSelectItem(this);
    w_si->setExcludeWithoutInventoryFilter(only_with_inventory);

    w_ok = new QPushButton(tr("&OK"), this);
    w_ok->setAutoDefault(true);
    w_ok->setDefault(true);

    w_cancel = new QPushButton(tr("&Cancel"), this);
    w_cancel->setAutoDefault(true);

    QFrame *hline = new QFrame(this);
    hline->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    QBoxLayout *toplay = new QVBoxLayout(this);
    toplay->addWidget(w_si);
    toplay->addWidget(hline);

    QBoxLayout *butlay = new QHBoxLayout();
    butlay->addStretch(60);
    butlay->addWidget(w_ok, 15);
    butlay->addWidget(w_cancel, 15);
    toplay->addLayout(butlay);

    setSizeGripEnabled(true);
    setMinimumSize(minimumSizeHint());

    connect(w_ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(w_cancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(w_si, SIGNAL(itemSelected(const BrickLink::Item *, bool)), this, SLOT(checkItem(const BrickLink::Item *, bool)));

    w_ok->setEnabled(false);
}

void DSelectItem::setItemType(const BrickLink::ItemType *itt)
{
    w_si->setCurrentItemType(itt);
}

void DSelectItem::setItem(const BrickLink::Item *item)
{
    w_si->setCurrentItem(item);
}
/*
bool DSelectItem::setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter )
{
 return w_si->setItemTypeCategoryAndFilter ( itt, cat, filter );
}
*/
const BrickLink::Item *DSelectItem::item() const
{
    return w_si->currentItem();
}

void DSelectItem::checkItem(const BrickLink::Item *it, bool ok)
{
    bool b = w_si->hasExcludeWithoutInventoryFilter() ? it->hasInventory() : true;

    w_ok->setEnabled((it) && b);

    if (it && b && ok)
        w_ok->animateClick();
}

int DSelectItem::exec(const QRect &pos)
{
    if (pos.isValid())
        CUtility::setPopupPos(this, pos);

    return QDialog::exec();
}
