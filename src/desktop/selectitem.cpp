/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <cmath>

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QComboBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QApplication>
#include <QCursor>
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
#include <QLineEdit>
#include <QPainter>
#include <QShortcut>
#include <QTimer>
#include <QInputDialog>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "bricklink/partcolorcode.h"
#include "bricklink/picture.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/eventfilter.h"
#include "desktopuihelpers.h"
#include "historylineedit.h"
#include "selectitem.h"

using namespace std::chrono_literals;
using namespace std::placeholders;


class HistoryLineEdit;

class SelectItemPrivate {
public:
    BrickLink::ItemTypeModel *itemTypeModel;
    BrickLink::CategoryModel *categoryModel;
    BrickLink::ItemModel *    itemModel;

    BrickLink::ItemThumbsDelegate *m_itemsDelegate;
    BrickLink::ItemThumbsDelegate *m_thumbsDelegate;

    QLabel *         w_item_types_label;
    QComboBox *      w_item_types;
    QTreeView *      w_categories;
    QStackedLayout * m_stack;
    QTreeView *      w_items;
    QListView *      w_thumbs;
    HistoryLineEdit *w_filter;
    QToolButton *    w_pcc;
    QToolButton *    w_dateFilter;
    QToolButton *    w_zoomLevel;
    QButtonGroup *   w_viewmode;
    bool             m_inv_only = false;
    QTimer *         m_filter_delay;
    double           m_zoom = 0;
    const BrickLink::Color *m_colorFilter = nullptr;
    const BrickLink::Item *m_colorFilterLastItem = nullptr;
};


class CategoryDelegate : public BrickLink::ItemDelegate
{
    Q_OBJECT
public:
    CategoryDelegate(QObject *parent = nullptr)
        : BrickLink::ItemDelegate(BrickLink::ItemDelegate::AlwaysShowSelection, parent)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

void CategoryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem myoption(option);
    if (index.isValid() && qvariant_cast<const BrickLink::Category *>(index.data(BrickLink::CategoryPointerRole)) == BrickLink::CategoryModel::AllCategories)
        myoption.font.setBold(true);
    BrickLink::ItemDelegate::paint(painter, myoption, index);
}


class CategoryTreeView : public QTreeView
{
public:
    CategoryTreeView(QWidget *parent)
        : QTreeView(parent)
    {
        setAlternatingRowColors(true);
        setAllColumnsShowFocus(true);
        setUniformRowHeights(true);
        setRootIsDecorated(false);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setItemDelegate(new CategoryDelegate(this));

        m_overlay = new QTreeView(this);
        m_overlay->setFrameStyle(QFrame::NoFrame);
        m_overlay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_overlay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_overlay->setHeaderHidden(true);

        m_overlay->setAllColumnsShowFocus(true);
        m_overlay->setUniformRowHeights(true);
        m_overlay->setRootIsDecorated(false);
        m_overlay->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_overlay->setSelectionMode(QAbstractItemView::SingleSelection);
        m_overlay->setItemDelegate(new CategoryDelegate(this));

        new EventFilter(m_overlay->viewport(), { QEvent::Wheel }, [this](QObject *, QEvent *e) {
            return QCoreApplication::sendEvent(viewport(), e) ? EventFilter::StopEventProcessing
                                                              : EventFilter::ContinueEventProcessing;
        });
    }

    void setModel(QAbstractItemModel *model) override
    {
        if (m_overlay->model()) {
            disconnect(m_overlay->model(), &QAbstractItemModel::layoutChanged,
                       this, &CategoryTreeView::hideRowsInOverlay);
            disconnect(m_overlay->model(), &QAbstractItemModel::modelReset,
                       this, &CategoryTreeView::hideRowsInOverlay);
        }

        QTreeView::setModel(model);
        m_overlay->setModel(model);

        if (m_overlay->model()) {
            connect(m_overlay->model(), &QAbstractItemModel::layoutChanged,
                    this, &CategoryTreeView::hideRowsInOverlay);
            connect(m_overlay->model(), &QAbstractItemModel::modelReset,
                    this, &CategoryTreeView::hideRowsInOverlay);
        }

        hideRowsInOverlay();

        m_overlay->setSelectionModel(selectionModel());
    }

protected:
    void hideRowsInOverlay()
    {
        if (m_overlay->model()) {
            for (int r = 1; r < m_overlay->model()->rowCount(); ++r)
                m_overlay->setRowHidden(r, QModelIndex(), true);
        }
    }

    bool viewportEvent(QEvent *e) override;

private:
    QTreeView *m_overlay;
};


bool CategoryTreeView::viewportEvent(QEvent *e)
{
    auto result = QTreeView::viewportEvent(e);

    switch (e->type()) {
    case QEvent::Resize:
    case QEvent::FontChange:
    case QEvent::StyleChange:
        if (viewport()) {
            m_overlay->move(viewport()->mapTo(this, QPoint(0, 0)));
            m_overlay->resize(contentsRect().width(), rowHeight(indexAt({ 0, 0 })));
        }
        break;
    default:
        break;
    }
    return result;
}



SelectItem::SelectItem(QWidget *parent)
    : QWidget(parent)
    , d(new SelectItemPrivate())
{
    init();
}

SelectItem::~SelectItem()
{ /* needed to use std::unique_ptr on d */ }

void SelectItem::init()
{
    d->w_item_types_label = new QLabel(this);
    d->w_item_types = new QComboBox(this);
    d->w_item_types->setEditable(false);

    d->w_categories = new CategoryTreeView(this);

    d->m_filter_delay = new QTimer(this);
    d->m_filter_delay->setInterval(400ms);
    d->m_filter_delay->setSingleShot(true);

    d->w_filter = new HistoryLineEdit(Config::MaxFilterHistory, this);
    connect(d->w_filter, &QLineEdit::textChanged,
            this, [this]() { d->m_filter_delay->start(); });
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { d->w_filter->setFocus(Qt::ShortcutFocusReason); });
    d->w_filter->setToFavoritesMode(true);

    new EventFilter(d->w_filter, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

    d->w_viewmode = new QButtonGroup(this);
    d->w_viewmode->setExclusive(true);

    connect(d->w_viewmode, &QButtonGroup::idClicked,
            this, &SelectItem::setViewMode);

    d->w_pcc = new QToolButton();
    d->w_pcc->setIcon(QIcon::fromTheme(u"edit-find"_qs));
    d->w_pcc->setShortcut(tr("Ctrl+E", "Shortcut for entering PCC"));
    d->w_pcc->setAutoRaise(true);
    connect(d->w_pcc, &QToolButton::clicked, this, [this]() {
        QString code = QInputDialog::getText(this, tr("Find element number"),
                                             tr("Enter a 7-digit Lego element number, also known as Part-Color-Code (PCC)"));
        if (!code.isEmpty()) {
            if (auto *pcc = BrickLink::core()->partColorCode(code.toUInt())) {
                setCurrentItem(pcc->item(), true);
                if (pcc->color())
                    emit showInColor(pcc->color());
            }
        }
    });

    d->w_dateFilter = new QToolButton();
    d->w_dateFilter->setVisible(false);
    d->w_dateFilter->setIcon(QIcon::fromTheme(u"appointment-new"_qs));
    d->w_dateFilter->setAutoRaise(true);
    connect(d->w_dateFilter, &QToolButton::clicked, this, [this]() {
        int minYear = QInputDialog::getInt(this, u"Min Year"_qs, u"Min Year"_qs, 1950);
        int maxYear = QInputDialog::getInt(this, u"Max Year"_qs, u"Max Year"_qs, 2050);
        d->itemModel->setFilterYearRange(minYear, maxYear);
    });

    connect(new QShortcut(QKeySequence::ZoomOut, this), &QShortcut::activated,
            this, [this]() {
        setZoomFactor((int(std::round(d->m_zoom * 4)) - 1) / 4.); // 25% steps
    });
    connect(new QShortcut(QKeySequence::ZoomIn, this), &QShortcut::activated,
            this, [this]() {
        setZoomFactor((int(std::round(d->m_zoom * 4)) + 1) / 4.); // 25% steps
    });
    d->w_zoomLevel = new QToolButton();
    d->w_zoomLevel->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d->w_zoomLevel->setPopupMode(QToolButton::MenuButtonPopup);
    d->w_zoomLevel->setAutoRaise(true);
    connect(d->w_zoomLevel, &QToolButton::clicked,
            this, [this]() { setZoomFactor(2); });
    auto *zoomMenu = new QMenu(d->w_zoomLevel);
    for (int zl : { 50, 75, 100, 125, 150, 175, 200, 250, 300, 350, 400, 500 }) {
        zoomMenu->addAction(QString::number(zl) + u'%')->setData(zl);
    }
    connect(zoomMenu, &QMenu::triggered, this, [this](QAction *a) {
        setZoomFactor(double(a->data().toInt()) / 100.);
    });
    d->w_zoomLevel->setMenu(zoomMenu);

    QToolButton *tb;
    tb = new QToolButton();
    tb->setShortcut(tr("Ctrl+1"));
    tb->setIcon(QIcon::fromTheme(u"view-list-details"_qs));
    tb->setAutoRaise(true);
    tb->setCheckable(true);
    tb->setChecked(true);
    d->w_viewmode->addButton(tb, 1);

    tb = new QToolButton();
    tb->setShortcut(tr("Ctrl+2"));
    tb->setIcon(QIcon::fromTheme(u"view-list-icons"_qs));
    tb->setAutoRaise(true);
    tb->setCheckable(true);
    d->w_viewmode->addButton(tb, 2);

    d->w_items = new QTreeView();
    d->w_items->setAlternatingRowColors(true);
    d->w_items->setAllColumnsShowFocus(true);
    d->w_items->setUniformRowHeights(true);
    d->w_items->setWordWrap(true);
    d->w_items->setRootIsDecorated(false);
    d->w_items->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_items->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_items->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_items->header()->setSectionsMovable(false);
    d->m_itemsDelegate = new BrickLink::ItemThumbsDelegate(d->m_zoom, d->w_items);
    d->w_items->setItemDelegate(d->m_itemsDelegate);

    d->w_thumbs = new QListView();
    d->w_thumbs->setUniformItemSizes(true);
    d->w_thumbs->setMovement(QListView::Static);
    d->w_thumbs->setViewMode(QListView::IconMode);
    d->w_thumbs->setLayoutMode(QListView::SinglePass);
    d->w_thumbs->setResizeMode(QListView::Adjust);
    d->w_thumbs->setSpacing(5);
    d->w_thumbs->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_thumbs->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_thumbs->setTextElideMode(Qt::ElideRight);
    d->w_thumbs->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_thumbsDelegate = new BrickLink::ItemThumbsDelegate(d->m_zoom, d->w_thumbs);
    d->w_thumbs->setItemDelegate(d->m_thumbsDelegate);
    connect(d->m_itemsDelegate, &BrickLink::ItemThumbsDelegate::zoomFactorChanged,
            this, &SelectItem::setZoomFactor);
    connect(d->m_thumbsDelegate, &BrickLink::ItemThumbsDelegate::zoomFactorChanged,
            this, &SelectItem::setZoomFactor);

    d->itemTypeModel = new BrickLink::ItemTypeModel(this);
    d->categoryModel = new BrickLink::CategoryModel(this);
    d->itemModel = new BrickLink::ItemModel(this);

    d->w_item_types->setModel(d->itemTypeModel);
    d->w_categories->setModel(d->categoryModel);
    d->w_items->setModel(d->itemModel);
    d->w_thumbs->setModel(d->itemModel);
    d->w_thumbs->setModelColumn(0);

    d->w_thumbs->setSelectionModel(d->w_items->selectionModel());

    // setSortingEnabled(true) is a bit weird: it defaults to descending, (re)sorts on activation
    // and doesn't keep the current item in view. Plus it cannot deal with dependencies between the
    // items and itemthumbs view.

    d->w_categories->header()->setSortIndicatorShown(true);
    d->w_categories->header()->setSectionsClickable(true);

    connect(d->w_categories->header(), &QHeaderView::sectionClicked,
            this, [this](int section) {
        d->w_categories->sortByColumn(section, d->w_categories->header()->sortIndicatorOrder());
        d->w_categories->scrollTo(d->w_categories->currentIndex());
    });

    d->w_items->header()->setSortIndicator(2, Qt::AscendingOrder); // the model is sorted already
    d->w_items->header()->setSortIndicatorShown(true);
    d->w_items->header()->setSectionsClickable(true);

    connect(d->w_items->header(), &QHeaderView::sectionClicked,
                     this, [this](int section) {
        sortItems(section, d->w_items->header()->sortIndicatorOrder());
    });

    connect(d->m_filter_delay, &QTimer::timeout,
            this, &SelectItem::applyFilter);

    connect(d->w_item_types, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        const BrickLink::ItemType *itemtype = currentItemType();
        const BrickLink::Category *oldCat = currentCategory();
        const BrickLink::Item *oldItem = currentItem();

        d->w_categories->selectionModel()->blockSignals(true);
        d->w_items->selectionModel()->blockSignals(true);

        d->categoryModel->setFilterItemType(itemtype);
        // try to stick to same category or switch to AllCats if it is not available anymore
        setCurrentCategory(oldCat);

        d->itemModel->setFilterItemType(itemtype);
        d->itemModel->setFilterCategory(currentCategory());
        if (!itemtype || !itemtype->hasColors())
            d->itemModel->setFilterColor(nullptr);

        d->w_categories->selectionModel()->blockSignals(false);
        d->w_items->selectionModel()->blockSignals(false);

        // we switch itemtypes, so the same item cannot exist, but we may have the same id
        // (e.g. keeping the same item id, when switching between sets and instructions)
        if (oldItem) {
            if (auto newItem = BrickLink::core()->item(currentItemType()->id(), oldItem->id()))
                setCurrentItem(newItem);
        }
        if (!currentItem()) // a model reset doesn't emit selectionChanged
            emit itemSelected(nullptr, false);

        ensureSelectionVisible();

        emit hasColors(itemtype ? itemtype->hasColors() : false);
        emit hasSubConditions(itemtype ? itemtype->hasSubConditions() : false);
    });

    connect(d->w_categories->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        const BrickLink::Item *oldItem = currentItem();
        d->itemModel->setFilterCategory(currentCategory());
        setCurrentItem(oldItem);
    });

    connect(d->itemModel, &QAbstractItemModel::modelReset, this, [this]() {
        emit itemSelected(nullptr, false);
    });

    connect(d->w_items->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        emit itemSelected(currentItem(), false);
    });

    connect(d->w_items, &QAbstractItemView::doubleClicked,
            this, &SelectItem::itemConfirmed);
    connect(d->w_thumbs, &QAbstractItemView::doubleClicked,
            this, &SelectItem::itemConfirmed);
    connect(d->w_items, &QAbstractItemView::activated,
            this, &SelectItem::itemConfirmed);
    connect(d->w_thumbs, &QAbstractItemView::activated,
            this, &SelectItem::itemConfirmed);
    connect(d->w_items, &QWidget::customContextMenuRequested,
            this, &SelectItem::showContextMenu);
    connect(d->w_thumbs, &QWidget::customContextMenuRequested,
            this, &SelectItem::showContextMenu);

    auto *lay = new QGridLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setColumnStretch(0, 25);
    lay->setColumnStretch(1, 75);
    lay->setRowStretch(0, 0);
    lay->setRowStretch(1, 1);

    auto *ittlay = new QHBoxLayout();
    ittlay->addWidget(d->w_item_types_label);
    ittlay->addWidget(d->w_item_types, 10);
    ittlay->addWidget(d->w_dateFilter);
    lay->addLayout(ittlay, 0, 0);

    lay->addWidget(d->w_categories, 1, 0, 1, 1);

    auto *viewlay = new QHBoxLayout();
    viewlay->setContentsMargins(0, 0, 0, 0);
    viewlay->setSpacing(0);
    viewlay->addWidget(d->w_filter);
    viewlay->addWidget(d->w_pcc);
    viewlay->addSpacing(11);
    viewlay->addWidget(d->w_zoomLevel);
    viewlay->addSpacing(11);
    viewlay->addWidget(d->w_viewmode->button(1));
    viewlay->addWidget(d->w_viewmode->button(2));
    lay->addLayout(viewlay, 0, 1);

    d->m_stack = new QStackedLayout();
    lay->addLayout(d->m_stack, 1, 1, 1, 1);

    d->m_stack->addWidget(d->w_items);
    d->m_stack->addWidget(d->w_thumbs);

    d->m_stack->setCurrentWidget(d->w_items);

    setFocusProxy(d->w_filter);
    setTabOrder(d->w_item_types, d->w_categories);
    setTabOrder(d->w_categories, d->w_filter);
    setTabOrder(d->w_filter, d->w_items);
    setTabOrder(d->w_items, d->w_thumbs);

    setZoomFactor(2);

    languageChange();
}

void SelectItem::languageChange()
{
    d->w_item_types_label->setText(tr("Item type:"));
    d->w_filter->setPlaceholderText(tr("Filter"));

    QString filterToolTip = tr("<p>" \
        "Only show items that contain all the entered words - regardless of case - in " \
        "either the name or the part number. This works much like a web search engine:<ul>" \
        "<li>to exclude words, prefix them with <tt>-</tt>. (e.g. <tt>-pattern</tt>)</li>" \
        "<li>to match on a phrase, put it inside quotes. (e.g. <tt>\"1 x 1\"</tt>)</li>" \
        "<li>to filter parts appearing in a specific set, put <tt>appears-in:</tt> in front" \
        " of the set name. (e.g. <tt>appears-in:8868-1</tt>)</li>" \
        "<li>to filter sets or minifigs consisting of a specific part, put <tt>consists-of:</tt>" \
        " in front of the part id. (e.g. <tt>consists-of:3001</tt>)</li>" \
        "</ul></p>") + d->w_filter->instructionToolTip();

    d->w_filter->setToolTip(ActionManager::toolTipLabel(tr("Filter the list using this expression"),
                                                        QKeySequence::Find, filterToolTip));

    auto setToolTipOnButton = [](QAbstractButton *b, const QString &text) {
        b->setToolTip(ActionManager::toolTipLabel(text, b->shortcut()));
    };
    setToolTipOnButton(d->w_pcc, tr("Find a 7-digit Lego element number"));
    setToolTipOnButton(d->w_viewmode->button(1), tr("List with Images"));
    setToolTipOnButton(d->w_viewmode->button(2), tr("Thumbnails"));
}

void SelectItem::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void SelectItem::sortItems(int section, Qt::SortOrder order)
{
    d->w_items->sortByColumn(section, order);
    d->w_items->scrollTo(d->w_items->currentIndex());

    d->w_thumbs->scrollTo(d->w_thumbs->currentIndex());
}

bool SelectItem::hasExcludeWithoutInventoryFilter() const
{
    return d->m_inv_only;
}

void SelectItem::setExcludeWithoutInventoryFilter(bool b)
{
    if (b != d->m_inv_only) {
        const BrickLink::ItemType *oldType = currentItemType();
        const BrickLink::Category *oldCategory = currentCategory();
        const BrickLink::Item *oldItem = currentItem();

        d->w_categories->clearSelection();
        d->w_items->clearSelection();

        d->categoryModel->setFilterWithoutInventory(b);
        d->itemTypeModel->setFilterWithoutInventory(b);
        d->itemModel->setFilterWithoutInventory(b);

        setCurrentItemType(oldType);
        if (!d->categoryModel->index(oldCategory).isValid())
            oldCategory = BrickLink::CategoryModel::AllCategories;
        setCurrentCategory(oldCategory);
        if (d->itemModel->index(oldItem).isValid())
            setCurrentItem(oldItem, false);

        d->m_inv_only = b;
    }
}

const BrickLink::Color *SelectItem::colorFilter() const
{
    return d->m_colorFilter;
}

void SelectItem::setColorFilter(const BrickLink::Color *color)
{
    if (color != d->m_colorFilter) {
        d->m_colorFilter = color;

        if (currentItemType() && currentItemType()->hasColors()) {
            const BrickLink::Item *oldItem = currentItem();
            if (oldItem)
                d->m_colorFilterLastItem = oldItem;
            else
                oldItem = d->m_colorFilterLastItem;

            d->w_items->clearSelection();
            d->itemModel->setFilterColor(color);

            if (d->itemModel->index(oldItem).isValid()) {
                d->m_colorFilterLastItem = nullptr;
                setCurrentItem(oldItem, false);
            }
        }
    }
}


const BrickLink::ItemType *SelectItem::currentItemType() const
{
    if (d->w_item_types->currentIndex() >= 0) {
        QModelIndex idx = d->itemTypeModel->index(d->w_item_types->currentIndex(), 0);
        return d->itemTypeModel->itemType(idx);
    } else {
        return nullptr;
    }
}

bool SelectItem::setCurrentItemType(const BrickLink::ItemType *it)
{
    QModelIndex idx = d->itemTypeModel->index(it);
    d->w_item_types->setCurrentIndex(idx.isValid() ? idx.row() : -1);
    return idx.isValid();
}

const BrickLink::Category *SelectItem::currentCategory() const
{
    QModelIndexList idxlst = d->w_categories->selectionModel()->selectedRows();
    return idxlst.isEmpty() ? nullptr : d->categoryModel->category(idxlst.front());
}

void SelectItem::setCurrentCategory(const BrickLink::Category *cat)
{
    QModelIndex idx = d->categoryModel->index(cat);
    if (!idx.isValid())
        idx = d->categoryModel->index(BrickLink::CategoryModel::AllCategories);
    d->w_categories->setCurrentIndex(idx);
}

const BrickLink::Item *SelectItem::currentItem() const
{
    QModelIndexList idxlst = d->w_items->selectionModel()->selectedRows();
    return idxlst.isEmpty() ? nullptr : d->itemModel->item(idxlst.front());
}

bool SelectItem::setCurrentItem(const BrickLink::Item *item, bool force_items_category)
{
    if (item) {
        if (force_items_category) {
            const BrickLink::ItemType *itt = item->itemType();
            const BrickLink::Category *cat = item->category();

            if (currentItemType() != itt)
                setCurrentItemType(itt);
            if (currentCategory() != cat)
                setCurrentCategory(cat);
        }
        QModelIndex idx = d->itemModel->index(item);
        if (idx.isValid()) {
            d->w_items->setCurrentIndex(idx);
            ensureSelectionVisible();
        } else {
            d->w_items->clearSelection();
        }
    } else {
        d->w_items->clearSelection();
    }

    return (item);
}

double SelectItem::zoomFactor() const
{
    return d->m_zoom;
}

void SelectItem::setZoomFactor(double zoom)
{
    d->m_itemsDelegate->setZoomFactor(zoom);
    d->m_thumbsDelegate->setZoomFactor(zoom);

    zoom = d->m_itemsDelegate->zoomFactor();

    if (!qFuzzyCompare(zoom, d->m_zoom)) {
        d->m_zoom = zoom;
        d->w_zoomLevel->setText(u"%1 %"_qs.arg(int(zoom * 100), 3));
    }
}

void SelectItem::clearFilter()
{
    d->w_filter->clear();
    applyFilter();
}

void SelectItem::setFilterFavoritesModel(QStringListModel *model)
{
    d->w_filter->setModel(model);
}

QByteArray SelectItem::saveState() const
{
    auto itt = currentItemType();
    auto cat = currentCategory();
    auto item = currentItem();
    auto color = colorFilter();

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SI") << qint32(4)
       << qint8(itt ? itt->id() : BrickLink::ItemType::InvalidId)
       << (cat ? (cat == BrickLink::CategoryModel::AllCategories ? uint(-2) : cat->id())
               : BrickLink::Category::InvalidId)
       << (item ? QString::fromLatin1(item->id()) : QString())
       << d->w_filter->text()
       << (color ? color->id() : BrickLink::Color::InvalidId)
       << zoomFactor()
       << d->w_viewmode->checkedId()
       << (d->w_categories->header()->sortIndicatorOrder() == Qt::AscendingOrder)
       << (d->w_items->header()->sortIndicatorOrder() == Qt::AscendingOrder)
       << d->w_items->header()->sortIndicatorSection();

    return ba;
}

bool SelectItem::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SI") || (version < 3) || (version > 4))
        return false;

    qint8 itt;
    uint cat;
    QString itemid;
    QByteArray filterState;
    QString filterText;
    uint colorFilterId = BrickLink::Color::InvalidId;
    double zoom;
    int viewMode;
    bool catSortAsc;
    bool itemSortAsc;
    int itemSortColumn;

    ds >> itt >> cat >> itemid;
    if (version == 3)
        ds >> filterState;
    else
        ds >> filterText >> colorFilterId;
    ds >> zoom >> viewMode >> catSortAsc >> itemSortAsc >> itemSortColumn;

    if (ds.status() != QDataStream::Ok)
        return false;

    // we need to reset the filter before setCurrentItem ... otherwise we might not be able to
    // find the item in the model
    if (version == 3) {
        if (!filterState.isEmpty() && d->w_filter->restoreState(filterState))
            applyFilter();
        // v3 forgot to save the color filter
        // v3 also saved the completer history in the filter edit
    } else {
        d->w_filter->setText(filterText);
        applyFilter();
        setColorFilter((colorFilterId == BrickLink::Color::InvalidId)
                       ? nullptr : BrickLink::core()->color(colorFilterId));
    }

    if ((itt != BrickLink::ItemType::InvalidId) && (itt != -1 /*v3*/))
        setCurrentItemType(BrickLink::core()->itemType(itt));
    if (cat != BrickLink::Category::InvalidId) {
        setCurrentCategory(cat == uint(-2) ? BrickLink::CategoryModel::AllCategories
                                           : BrickLink::core()->category(cat));
    }
    if (!itemid.isEmpty())
        setCurrentItem(BrickLink::core()->item(itt, itemid.toLatin1()), false);
    setZoomFactor(zoom);
    setViewMode(viewMode);
    d->w_categories->sortByColumn(0, catSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    sortItems(itemSortColumn, itemSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    return true;
}

QByteArray SelectItem::defaultState()
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SI") << qint32(4)
       << qint8('P')  // parts
       << uint(-2)    // all categories
       << QString()   // no item id
       << QString()   // no text filter
       << BrickLink::Color::InvalidId // no color filter
       << double(2)   // zoom 200%
       << int(1)      // listview
       << true        // cat sorted asc
       << true        // items sorted asc
       << int(2);     // items sorted by name
    return ba;
}

void SelectItem::setViewMode(int mode)
{
    if (mode == 0) // legacy
        mode = 1;

    QWidget *w = nullptr;
    switch (mode) {
    case 1: w = d->w_items; break;
    case 2: w = d->w_thumbs; break;
    default: return;
    }
    d->m_stack->setCurrentWidget(w);
    d->w_viewmode->button(mode)->setChecked(true);
}


void SelectItem::itemConfirmed()
{
    if (const BrickLink::Item *item = currentItem())
        emit itemSelected(item, true);
}

void SelectItem::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
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
        d->w_thumbs->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}


void SelectItem::applyFilter()
{
    d->m_filter_delay->stop();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    const BrickLink::Item *oldItem = currentItem();
    d->w_items->clearSelection();

    d->itemModel->setFilterText(d->w_filter->text());

    setCurrentItem(oldItem);
    if (!currentItem() && d->itemModel->rowCount() == 1)
        setCurrentItem(d->itemModel->item(d->itemModel->index(0, 0)));

    QApplication::restoreOverrideCursor();
}


QSize SelectItem::sizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return { 120 * fm.horizontalAdvance(u'x'), 20 * fm.height() };
}

QSize SelectItem::minimumSizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return { 80 * fm.horizontalAdvance(u'x'), 12 * fm.height() };
}

void SelectItem::showContextMenu(const QPoint &p)
{
    if (auto *iv = qobject_cast<QAbstractItemView *>(sender())) {
        auto *m = new QMenu(this);
        m->setAttribute(Qt::WA_DeleteOnClose);

        const BrickLink::Item *item = nullptr;
        QModelIndex idx = iv->indexAt(p);
        if (idx.isValid())
            item = idx.model()->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();

        if (currentCategory() != BrickLink::CategoryModel::AllCategories) {
            QString allCatName = d->categoryModel->index(BrickLink::CategoryModel::AllCategories)
                    .data(Qt::DisplayRole).toString();
            connect(m->addAction(tr("Switch to the \"%1\" category").arg(allCatName)),
                    &QAction::triggered, this, [this]() {
                setCurrentCategory(BrickLink::CategoryModel::AllCategories);
            });
        }

        if (item) {
            const auto cats = item->additionalCategories(true);
            for (const auto &cat : cats) {
                if (cat != currentCategory()) {
                    connect(m->addAction(tr("Switch to the item's \"%1\" category").arg(cat->name())),
                            &QAction::triggered, this, [this, cat]() {
                        setCurrentCategory(cat);
                    });
                }
            }
        }

        // mini-fig special
        if (item && item->itemType() && (item->itemTypeId() == 'M') && item->hasInventory()) {
            auto minifigParts = item->consistsOf();

            for (const BrickLink::Item::ConsistsOf &part : minifigParts) {
                auto partItem = part.item();
                if (!partItem)
                    continue;
                auto partColor = part.color();
                auto partPicture = BrickLink::core()->pictureCache()->picture(partItem, partColor, true);


                QString filter = BrickLink::ItemModel::tr("consists-of:") + QLatin1String(partItem->id());
                if (partItem->itemType()->hasColors() && partColor)
                    filter = filter + u'@' + QString::number(partColor->id());
                QIcon icon;
                if (partPicture->isValid())
                    icon = QPixmap::fromImage(partPicture->image());

                m->addSeparator();
                QString section;
                if (partColor && partColor->id())
                    section = partColor->name() + u' ';
                section = section + partItem->name() + u" [" + QLatin1String(partItem->id()) + u']';
                m->addAction(icon, section)->setEnabled(false);

                connect(m->addAction(tr("Set filter to Minifigs consisting of this part")),
                                    &QAction::triggered, this, [this, filter]() {
                    d->w_filter->setText(filter);
                });
                if (!d->w_filter->text().isEmpty()) {
                    connect(m->addAction(tr("Narrow filter to Minifigs consisting of this part")),
                                        &QAction::triggered, this, [this, filter]() {
                        d->w_filter->setText(d->w_filter->text() + u' ' + filter);
                    });
                }
            }
        }

        if (!m->isEmpty())
            m->popup(iv->mapToGlobal(p));
        else
            delete m;
    }
}

#include "selectitem.moc"
#include "moc_selectitem.cpp"
