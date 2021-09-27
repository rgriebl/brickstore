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
#include <QStringBuilder>
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

#include "utility/utility.h"
#include "utility/historylineedit.h"
#include "bricklink/bricklink_model.h"
#include "selectitem.h"
#include "utility/messagebox.h"
#include "framework.h"
#include "config.h"

using namespace std::chrono_literals;

class HistoryLineEdit;

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
    HistoryLineEdit *w_filter;
    QToolButton *    w_pcc;
    QToolButton *    w_zoomIn;
    QToolButton *    w_zoomOut;
    QToolButton *    w_zoomLevel;
    QButtonGroup *   w_viewmode;
    bool             m_inv_only;
    QTimer *         m_filter_delay;
    double           m_zoom = 0;
    const BrickLink::Color *m_colorFilter;
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


class ItemThumbsDelegate : public BrickLink::ItemDelegate
{
    Q_OBJECT
public:
    ItemThumbsDelegate(double initialZoom, QObject *parent = nullptr)
        : BrickLink::ItemDelegate(BrickLink::ItemDelegate::AlwaysShowSelection
                                  | BrickLink::ItemDelegate::FirstColumnImageOnly, parent)
        , m_zoom(initialZoom)
    { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setZoomFactor(double zoom)
    {
        m_zoom = zoom;
    }

private:
    double m_zoom;
};

QSize ItemThumbsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == 0) {
        const auto *item = qvariant_cast<const BrickLink::Item *>(index.data(BrickLink::ItemPointerRole));
        return (item ? item->itemType()->rawPictureSize() : QSize(80, 60)) * m_zoom;
    } else {
        return BrickLink::ItemDelegate::sizeHint(option, index);
    }
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

        m_overlay->viewport()->installEventFilter(this);
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
    bool eventFilter(QObject *o, QEvent *e) override;

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

bool CategoryTreeView::eventFilter(QObject *o, QEvent *e)
{
    if ((e->type() == QEvent::Wheel) && (o == m_overlay->viewport()))
        return QCoreApplication::sendEvent(viewport(), e);
    return QTreeView::eventFilter(o, e);
}


SelectItem::SelectItem(QWidget *parent)
    : QWidget(parent)
    , d(new SelectItemPrivate())
{
    d->m_inv_only = false;
    init();
}

SelectItem::~SelectItem()
{ /* needed to use QScopedPointer on d */ }

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
            this, [this]() { d->w_filter->setFocus(); });
    connect(Config::inst(), &Config::filtersInFavoritesModeChanged,
            d->w_filter, &HistoryLineEdit::setToFavoritesMode);
    d->w_filter->setToFavoritesMode(Config::inst()->areFiltersInFavoritesMode());

    d->w_viewmode = new QButtonGroup(this);
    d->w_viewmode->setExclusive(true);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(d->w_viewmode, QOverload<int>::of(&QButtonGroup::buttonClicked),
#else
    connect(d->w_viewmode, &QButtonGroup::idClicked,
#endif
            this, &SelectItem::setViewMode);

    d->w_pcc = new QToolButton();
    d->w_pcc->setIcon(QIcon::fromTheme("edit-find"_l1));
    d->w_pcc->setShortcut(tr("Ctrl+E", "Shortcut for entering PCC"));
    d->w_pcc->setAutoRaise(true);
    connect(d->w_pcc, &QToolButton::clicked, this, [this]() {
        QRect r = { d->w_pcc->mapToGlobal(QPoint()), d->w_pcc->size() };
        QString code;
        if (MessageBox::getString(this, tr("Find element number"),
                                  tr("Enter a 7-digit Lego element number, also known as Part-Color-Code (PCC)"),
                                  code, r)) {
            if (auto *pcc = BrickLink::core()->partColorCode(code.toUInt())) {
                setCurrentItem(pcc->item(), true);
                if (pcc->color())
                    emit showInColor(pcc->color());
            }
        }
    });

    d->w_zoomOut = new QToolButton();
    d->w_zoomOut->setShortcut(QKeySequence::ZoomOut);
    d->w_zoomOut->setIcon(QIcon::fromTheme("zoom-out"_l1));
    d->w_zoomOut->setAutoRaise(true);
    d->w_zoomOut->setAutoRepeat(true);
    connect(d->w_zoomOut, &QToolButton::clicked, this, [this]() {
        setZoomFactor((int(std::round(d->m_zoom * 4)) - 1) / 4.); // 25% steps
    });
    d->w_zoomLevel = new QToolButton();
    d->w_zoomLevel->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d->w_zoomLevel->setAutoRaise(true);
    connect(d->w_zoomLevel, &QToolButton::clicked,
            this, [this]() { setZoomFactor(2); });
    d->w_zoomIn = new QToolButton();
    d->w_zoomIn->setShortcut(QKeySequence::ZoomIn);
    d->w_zoomIn->setIcon(QIcon::fromTheme("zoom-in"_l1));
    d->w_zoomIn->setAutoRaise(true);
    d->w_zoomIn->setAutoRepeat(true);
    connect(d->w_zoomIn, &QToolButton::clicked, this, [this]() {
        setZoomFactor((int(std::round(d->m_zoom * 4)) + 1) / 4.); // 25% steps
    });

    QToolButton *tb;
    tb = new QToolButton();
    tb->setShortcut(tr("Ctrl+1"));
    tb->setIcon(QIcon::fromTheme("view-list-text"_l1));
    tb->setAutoRaise(true);
    tb->setCheckable(true);
    d->w_viewmode->addButton(tb, 0);

    tb = new QToolButton();
    tb->setShortcut(tr("Ctrl+2"));
    tb->setIcon(QIcon::fromTheme("view-list-details"_l1));
    tb->setAutoRaise(true);
    tb->setCheckable(true);
    tb->setChecked(true);
    d->w_viewmode->addButton(tb, 1);

    tb = new QToolButton();
    tb->setShortcut(tr("Ctrl+3"));
    tb->setIcon(QIcon::fromTheme("view-list-icons"_l1));
    tb->setAutoRaise(true);
    tb->setCheckable(true);
    d->w_viewmode->addButton(tb, 2);

    d->w_items = new QTreeView();
    d->w_items->setAlternatingRowColors(true);
    d->w_items->setAllColumnsShowFocus(true);
    d->w_items->setUniformRowHeights(true);
    d->w_items->setRootIsDecorated(false);
    d->w_items->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_items->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_items->setItemDelegate(new BrickLink::ItemDelegate(BrickLink::ItemDelegate::AlwaysShowSelection, this));
    d->w_items->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_items->header()->setSectionsMovable(false);

    d->w_itemthumbs = new QTreeView();
    d->w_itemthumbs->setAlternatingRowColors(true);
    d->w_itemthumbs->setAllColumnsShowFocus(true);
    d->w_itemthumbs->setUniformRowHeights(true);
    d->w_itemthumbs->setWordWrap(true);
    d->w_itemthumbs->setRootIsDecorated(false);
    d->w_itemthumbs->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->w_itemthumbs->setSelectionMode(QAbstractItemView::SingleSelection);
    d->w_itemthumbs->setItemDelegate(new ItemThumbsDelegate(d->m_zoom, this));
    d->w_itemthumbs->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_itemthumbs->viewport()->installEventFilter(this);
    d->w_itemthumbs->header()->setSectionsMovable(false);

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
    d->w_thumbs->setItemDelegate(new ItemThumbsDelegate(d->m_zoom, this));
    d->w_thumbs->setContextMenuPolicy(Qt::CustomContextMenu);
    d->w_thumbs->viewport()->installEventFilter(this);

    d->itemTypeModel = new BrickLink::ItemTypeModel(this);
    d->categoryModel = new BrickLink::CategoryModel(this);
    d->itemModel = new BrickLink::ItemModel(this);

    d->w_item_types->setModel(d->itemTypeModel);
    d->w_categories->setModel(d->categoryModel);
    d->w_items->setModel(d->itemModel);
    d->w_items->hideColumn(0);
    d->w_itemthumbs->setModel(d->itemModel);
    d->w_thumbs->setModel(d->itemModel);
    d->w_thumbs->setModelColumn(0);

    d->w_itemthumbs->setSelectionModel(d->w_items->selectionModel());
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

    d->w_items->header()->setSortIndicatorShown(true);
    d->w_items->header()->setSectionsClickable(true);

    connect(d->w_items->header(), &QHeaderView::sectionClicked,
                     this, [this](int section) {
        sortItems(section, d->w_items->header()->sortIndicatorOrder());
    });

    d->w_itemthumbs->header()->setSortIndicator(2, Qt::AscendingOrder); // the model is sorted already
    d->w_itemthumbs->header()->setSortIndicatorShown(true);
    d->w_itemthumbs->header()->setSectionsClickable(true);

    connect(d->w_itemthumbs->header(), &QHeaderView::sectionClicked,
                     this, [this](int section) {
        sortItems(section, d->w_itemthumbs->header()->sortIndicatorOrder());
    });

    connect(d->m_filter_delay, &QTimer::timeout,
            this, &SelectItem::applyFilter);

    connect(d->w_item_types, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SelectItem::itemTypeUpdated);
    connect(d->w_categories->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SelectItem::categoryUpdated);

    connect(d->w_items->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SelectItem::itemUpdated);
    connect(d->w_items, &QAbstractItemView::doubleClicked,
            this, &SelectItem::itemConfirmed);
    connect(d->w_itemthumbs, &QAbstractItemView::doubleClicked,
            this, &SelectItem::itemConfirmed);
    connect(d->w_thumbs, &QAbstractItemView::doubleClicked,
            this, &SelectItem::itemConfirmed);
    connect(d->w_items, &QAbstractItemView::activated,
            this, &SelectItem::itemConfirmed);
    connect(d->w_itemthumbs, &QAbstractItemView::activated,
            this, &SelectItem::itemConfirmed);
    connect(d->w_thumbs, &QAbstractItemView::activated,
            this, &SelectItem::itemConfirmed);
    connect(d->w_items, &QWidget::customContextMenuRequested,
            this, &SelectItem::showContextMenu);
    connect(d->w_itemthumbs, &QWidget::customContextMenuRequested,
            this, &SelectItem::showContextMenu);
    connect(d->w_thumbs, &QWidget::customContextMenuRequested,
            this, &SelectItem::showContextMenu);

    auto *lay = new QGridLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setColumnStretch(1, 25);
    lay->setColumnStretch(2, 75);
    lay->setRowStretch(0, 0);
    lay->setRowStretch(1, 1);

    lay->addWidget(d->w_item_types_label, 0, 0);
    lay->addWidget(d->w_item_types, 0, 1);

    lay->addWidget(d->w_categories, 1, 0, 1, 2);

    lay->addWidget(d->w_filter, 0, 2);

    auto *viewlay = new QHBoxLayout();
    viewlay->setContentsMargins(0, 0, 0, 0);
    viewlay->setSpacing(0);
    viewlay->addSpacing(5);
    viewlay->addWidget(d->w_pcc);
    viewlay->addSpacing(11);
    viewlay->addWidget(d->w_zoomOut);
    viewlay->addWidget(d->w_zoomLevel);
    viewlay->addWidget(d->w_zoomIn);
    viewlay->addSpacing(11);
    viewlay->addWidget(d->w_viewmode->button(0));
    viewlay->addWidget(d->w_viewmode->button(1));
    viewlay->addWidget(d->w_viewmode->button(2));
    lay->addLayout(viewlay, 0, 3);

    d->m_stack = new QStackedLayout();
    lay->addLayout(d->m_stack, 1, 2, 1, 2);

    d->m_stack->addWidget(d->w_items);
    d->m_stack->addWidget(d->w_itemthumbs);
    d->m_stack->addWidget(d->w_thumbs);

    d->m_stack->setCurrentWidget(d->w_itemthumbs);

    setFocusProxy(d->w_filter);
    setTabOrder(d->w_item_types, d->w_categories);
    setTabOrder(d->w_categories, d->w_filter);
    setTabOrder(d->w_filter, d->w_items);
    setTabOrder(d->w_items, d->w_itemthumbs);
    setTabOrder(d->w_itemthumbs, d->w_thumbs);

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

    d->w_filter->setToolTip(Utility::toolTipLabel(tr("Filter the list using this expression"),
                                                  QKeySequence::Find, filterToolTip));

    auto setToolTipOnButton = [](QAbstractButton *b, const QString &text) {
        b->setToolTip(Utility::toolTipLabel(text, b->shortcut()));
    };
    setToolTipOnButton(d->w_pcc, tr("Find a 7-digit Lego element number"));
    setToolTipOnButton(d->w_viewmode->button(0), tr("List"));
    setToolTipOnButton(d->w_viewmode->button(2), tr("Thumbnails"));
    setToolTipOnButton(d->w_viewmode->button(1), tr("List with Images"));
    setToolTipOnButton(d->w_zoomIn, tr("Zoom in"));
    setToolTipOnButton(d->w_zoomOut, tr("Zoom out"));
}

bool SelectItem::eventFilter(QObject *o, QEvent *e)
{
    if (d->w_itemthumbs) {
        if ((o == d->w_itemthumbs->viewport() || o == d->w_thumbs->viewport())
                && (e->type() == QEvent::Wheel)) {
            const auto *we = static_cast<QWheelEvent *>(e);
            if (we->modifiers() & Qt::ControlModifier) {
                double z = std::pow(1.001, we->angleDelta().y());
                setZoomFactor(d->m_zoom * z);
                e->accept();
                return true;
            }
        }
        if ((o == d->w_itemthumbs->viewport() || o == d->w_thumbs->viewport())
                && (e->type() == QEvent::NativeGesture)) {
            const auto *nge = static_cast<QNativeGestureEvent *>(e);
            if (nge->gestureType() == Qt::ZoomNativeGesture) {
                double z = 1 + nge->value();
                setZoomFactor(d->m_zoom * z);
                e->accept();
                return true;
            }
        }
    }

    return QWidget::eventFilter(o, e);
}

void SelectItem::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void SelectItem::sortItems(int section, Qt::SortOrder order)
{
    d->w_itemthumbs->sortByColumn(section, order);
    d->w_itemthumbs->scrollTo(d->w_itemthumbs->currentIndex());

    d->w_items->header()->setSortIndicator(section ? section : 1, order);
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
            d->w_items->clearSelection();

            d->itemModel->setFilterColor(color);

            if (d->itemModel->index(oldItem).isValid())
                setCurrentItem(oldItem, false);
        }
    }
}

void SelectItem::itemTypeUpdated()
{
    const BrickLink::ItemType *itemtype = currentItemType();

    const BrickLink::Category *oldCat = currentCategory();
    const BrickLink::Item *oldItem = currentItem();
    d->w_categories->clearSelection();
    d->w_items->clearSelection();

    d->categoryModel->setFilterItemType(itemtype);
    d->itemModel->setFilterItemType(itemtype);

    setCurrentCategory(oldCat);
    setCurrentItem(oldItem);

    d->w_itemthumbs->resizeColumnToContents(0);

    if (!itemtype || !itemtype->hasColors())
        d->itemModel->setFilterColor(nullptr);

    emit currentItemTypeChanged(itemtype);
    emit hasColors(itemtype ? itemtype->hasColors() : false);
    emit hasSubConditions(itemtype ? itemtype->hasSubConditions() : false);
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


void SelectItem::categoryUpdated()
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
    return idxlst.isEmpty() ? nullptr : d->categoryModel->category(idxlst.front());
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
    zoom = qBound(.5, zoom, 5.);

    if (!qFuzzyCompare(zoom, d->m_zoom)) {
        d->m_zoom = zoom;

        auto d1 = static_cast<ItemThumbsDelegate *>(d->w_thumbs->itemDelegate());
        auto d2 = static_cast<ItemThumbsDelegate *>(d->w_itemthumbs->itemDelegate());

        d1->setZoomFactor(zoom);
        d2->setZoomFactor(zoom);

        emit d1->sizeHintChanged(d->itemModel->index(0, 0));
        emit d2->sizeHintChanged(d->itemModel->index(0, 0));
        
        d->w_zoomLevel->setText(QString::fromLatin1("%1 %").arg(int(zoom * 100), 3));
        d->w_itemthumbs->resizeColumnToContents(0);
    }
}

void SelectItem::clearFilter()
{
    d->w_filter->clear();
}

QByteArray SelectItem::saveState() const
{
    auto itt = currentItemType();
    auto cat = currentCategory();
    auto item = currentItem();

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SI") << qint32(3)
       << qint8(itt ? itt->id() : char(-1))
       << (cat ? (cat == BrickLink::CategoryModel::AllCategories ? uint(-2) : cat->id()) : uint(-1))
       << (item ? QString::fromLatin1(item->id()) : QString())
       << d->w_filter->saveState()
       << zoomFactor()
       << d->w_viewmode->checkedId()
       << (d->w_categories->header()->sortIndicatorOrder() == Qt::AscendingOrder)
       << (d->w_itemthumbs->header()->sortIndicatorOrder() == Qt::AscendingOrder)
       << d->w_itemthumbs->header()->sortIndicatorSection();

    return ba;
}

bool SelectItem::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SI") || (version != 3))
        return false;

    qint8 itt;
    uint cat;
    QString itemid;
    QByteArray filterState;
    double zoom;
    int viewMode;
    bool catSortAsc;
    bool itemSortAsc;
    int itemSortColumn;

    ds >> itt >> cat >> itemid >> filterState >> zoom >> viewMode
            >> catSortAsc >> itemSortAsc >> itemSortColumn;

    if (ds.status() != QDataStream::Ok)
        return false;

    if (itt != -1)
        setCurrentItemType(BrickLink::core()->itemType(itt));
    if (cat != uint(-1)) {
        setCurrentCategory(cat == uint(-2) ? BrickLink::CategoryModel::AllCategories
                                           : BrickLink::core()->category(cat));
    }
    if (!itemid.isEmpty())
        setCurrentItem(BrickLink::core()->item(itt, itemid.toLatin1()), false);
    setZoomFactor(zoom);
    setViewMode(viewMode);
    d->w_categories->sortByColumn(0, catSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    sortItems(itemSortColumn, itemSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    if (!filterState.isEmpty() && d->w_filter->restoreState(filterState))
        applyFilter();
    return true;
}

QByteArray SelectItem::defaultState()
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SI") << qint32(1)
       << qint8('P')
       << uint(-2)
       << QString()
       << QString()
       << false
       << false
       << double(2)
       << int(1)
       << true
       << true
       << int(2);
    return ba;
}

void SelectItem::setViewMode(int mode)
{
    QWidget *w = nullptr;
    switch (mode) {
    case 0 : w = d->w_items; break;
    case 1 : w = d->w_itemthumbs; break;
    case 2 : w = d->w_thumbs; break;
    default: return;
    }
    d->m_stack->setCurrentWidget(w);
    d->w_viewmode->button(mode)->setChecked(true);
}


void SelectItem::itemUpdated()
{
    emit itemSelected(currentItem(), false);
}

void SelectItem::itemConfirmed()
{
    const BrickLink::Item *item = currentItem();
    if (item)
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
    if (!currentItem() && d->itemModel->rowCount() == 1)
        setCurrentItem(d->itemModel->item(d->itemModel->index(0, 0)));

    QApplication::restoreOverrideCursor();
}


QSize SelectItem::sizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return { 120 * fm.horizontalAdvance('x'_l1), 20 * fm.height() };
}

QSize SelectItem::minimumSizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return { 80 * fm.horizontalAdvance('x'_l1), 12 * fm.height() };
}

void SelectItem::showContextMenu(const QPoint &p)
{
    if (auto *iv = qobject_cast<QAbstractItemView *>(sender())) {
        QMenu m(this);

        const BrickLink::Item *item = nullptr;
        QModelIndex idx = iv->indexAt(p);
        if (idx.isValid())
            item = idx.model()->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();

        if (item && item->category() != currentCategory()) {
            connect(m.addAction(tr("Switch to the item's category")), &QAction::triggered,
                    this, [this, item]() { setCurrentItem(item, true); });
        }

        if (currentCategory() != BrickLink::CategoryModel::AllCategories) {
            QString allCatName = d->categoryModel->index(BrickLink::CategoryModel::AllCategories)
                    .data(Qt::DisplayRole).toString();
            connect(m.addAction(tr("Switch to the \"%1\" category").arg(allCatName)),
                    &QAction::triggered, this, [this]() {
                setCurrentCategory(BrickLink::CategoryModel::AllCategories);
            });
        }

        // mini-fig special
        if (item && item->itemType() && (item->itemTypeId() == 'M') && item->hasInventory()) {
            auto minifigParts = item->consistsOf();

            for (const BrickLink::Item::ConsistsOf &part : minifigParts) {
                auto partItem = part.item();
                if (!partItem)
                    continue;
                auto partColor = part.color();
                auto partPicture = BrickLink::core()->picture(partItem, partColor, true);


                QString filter = BrickLink::ItemModel::tr("consists-of:") % QLatin1String(partItem->id());
                if (partItem->itemType()->hasColors() && partColor)
                    filter = filter % u'@' % QString::number(partColor->id());
                QIcon icon;
                if (partPicture->isValid())
                    icon = QPixmap::fromImage(partPicture->image());

                m.addSeparator();
                QString section;
                if (partColor && partColor->id())
                    section = partColor->name() % u' ';
                section = section % partItem->name() % u" [" % QLatin1String(partItem->id()) % u']';
                m.addAction(icon, section)->setEnabled(false);

                connect(m.addAction(tr("Set filter to Minifigs consisting of this part")),
                                    &QAction::triggered, this, [this, filter]() {
                    d->w_filter->setText(filter);
                });
                if (!d->w_filter->text().isEmpty()) {
                    connect(m.addAction(tr("Narrow filter to Minifigs consisting of this part")),
                                        &QAction::triggered, this, [this, filter]() {
                        d->w_filter->setText(d->w_filter->text() % u' ' % filter);
                    });
                }
            }
        }

        if (!m.isEmpty())
            m.exec(iv->mapToGlobal(p));
    }
}

#include "selectitem.moc"
#include "moc_selectitem.cpp"
