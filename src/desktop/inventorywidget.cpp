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
#include <QMenu>
#include <QVariant>
#include <QAction>
#include <QActionEvent>
#include <QHeaderView>
#include <QApplication>
#include <QTimer>
#include <QTreeView>
#include <QToolButton>
#include <QGridLayout>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "common/document.h"
#include "importinventorydialog.h"

#include "inventorywidget.h"


class InventoryWidgetPrivate {
public:
    QVector<QPair<const BrickLink::Item *, const BrickLink::Color *>> m_list;
    InventoryWidget::Mode m_mode = InventoryWidget::Mode::AppearsIn;
    QTreeView *m_view;
    QToolButton *m_appearsIn;
    QToolButton *m_consistsOf;
    QTimer *m_resize_timer;
    QMenu *m_contextMenu;
    QAction *m_partOutAction;
    QAction *m_separatorAction;
    QAction *m_catalogAction;
    QAction *m_priceGuideAction;
    QAction *m_lotsForSaleAction;
};

InventoryWidget::InventoryWidget(QWidget *parent)
    : QFrame(parent)
    , d(new InventoryWidgetPrivate())
{

    d->m_resize_timer = new QTimer(this);
    d->m_resize_timer->setSingleShot(true);

    d->m_contextMenu = new QMenu(this);

    d->m_view = new QTreeView();
    setFrameStyle(d->m_view->frameStyle());
    d->m_view->setFrameStyle(QFrame::NoFrame);
    d->m_view->setAlternatingRowColors(true);
    d->m_view->setAllColumnsShowFocus(true);
    d->m_view->setUniformRowHeights(true);
    d->m_view->setRootIsDecorated(false);
    d->m_view->setSortingEnabled(true);
    d->m_view->sortByColumn(0, Qt::DescendingOrder);
    d->m_view->header()->setSortIndicatorShown(false);
    d->m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_view->setItemDelegate(new BrickLink::ItemDelegate(BrickLink::ItemDelegate::None, this));

    d->m_appearsIn = new QToolButton();
    d->m_appearsIn->setCheckable(true);
    d->m_appearsIn->setChecked(true);
    d->m_appearsIn->setAutoExclusive(true);
    d->m_appearsIn->setAutoRaise(true);
    d->m_appearsIn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(d->m_appearsIn, &QToolButton::clicked, this, [this]() { setMode(Mode::AppearsIn); });

    d->m_consistsOf = new QToolButton();
    d->m_consistsOf->setCheckable(true);
    d->m_consistsOf->setAutoExclusive(true);
    d->m_consistsOf->setAutoRaise(true);
    d->m_consistsOf->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(d->m_consistsOf, &QToolButton::clicked, this, [this]() { setMode(Mode::ConsistsOf); });

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->setRowStretch(1, 10);
    grid->addWidget(d->m_appearsIn, 0, 0);
    grid->addWidget(d->m_consistsOf, 0, 1);
    grid->addWidget(d->m_view, 1, 0, 1, 2);

    d->m_partOutAction = new QAction(this);
    d->m_partOutAction->setObjectName(u"appearsin_partoutitems"_qs);
    d->m_partOutAction->setIcon(QIcon::fromTheme(u"edit-partoutitems"_qs));
    connect(d->m_partOutAction, &QAction::triggered,
            this, &InventoryWidget::partOut);

    d->m_catalogAction = new QAction(this);
    d->m_catalogAction->setObjectName(u"appearsin_bl_catalog"_qs);
    d->m_catalogAction->setIcon(QIcon::fromTheme(u"bricklink-catalog"_qs));
    connect(d->m_catalogAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.item)
            BrickLink::core()->openUrl(BrickLink::Url::CatalogInfo, entry.item);
    });

    d->m_priceGuideAction = new QAction(this);
    d->m_priceGuideAction->setObjectName(u"appearsin_bl_priceguide"_qs);
    d->m_priceGuideAction->setIcon(QIcon::fromTheme(u"bricklink-priceguide"_qs));
    connect(d->m_priceGuideAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.item)
            BrickLink::core()->openUrl(BrickLink::Url::PriceGuideInfo, entry.item,
                                       entry.color ? entry.color : BrickLink::core()->color(0));
    });

    d->m_lotsForSaleAction = new QAction(this);
    d->m_lotsForSaleAction->setObjectName(u"appearsin_bl_lotsforsale"_qs);
    d->m_lotsForSaleAction->setIcon(QIcon::fromTheme(u"bricklink-lotsforsale"_qs));
    connect(d->m_lotsForSaleAction, &QAction::triggered, this, [this]() {
        const auto entry = selected();

        if (entry.item)
            BrickLink::core()->openUrl(BrickLink::Url::LotsForSale, entry.item,
                                       entry.color ? entry.color : BrickLink::core()->color(0));
    });

    d->m_contextMenu->addAction(d->m_partOutAction);
    d->m_separatorAction = d->m_contextMenu->addSeparator();
    d->m_contextMenu->addAction(d->m_catalogAction);
    d->m_contextMenu->addAction(d->m_priceGuideAction);
    d->m_contextMenu->addAction(d->m_lotsForSaleAction);

    connect(d->m_resize_timer, &QTimer::timeout,
            this, &InventoryWidget::resizeColumns);

    connect(d->m_view, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        if (auto item = selected().item) {
            d->m_partOutAction->setEnabled(item->hasInventory());
            d->m_contextMenu->popup(d->m_view->viewport()->mapToGlobal(pos));
        }
    });

    connect(d->m_view, &QAbstractItemView::doubleClicked,
            this, &InventoryWidget::partOut);

    languageChange();
    setItem(nullptr, nullptr);
}

InventoryWidget::~InventoryWidget()
{ /* needed to use std::unique_ptr on d */ }

void InventoryWidget::languageChange()
{
    d->m_appearsIn->setText(tr("Appears in"));
    d->m_consistsOf->setText(tr("Consists of"));
    d->m_partOutAction->setText(tr("Part out Item..."));
    d->m_catalogAction->setText(tr("Show BrickLink Catalog Info..."));
    d->m_priceGuideAction->setText(tr("Show BrickLink Price Guide Info..."));
    d->m_lotsForSaleAction->setText(tr("Show Lots for Sale on BrickLink..."));
}

void InventoryWidget::actionEvent(QActionEvent *e)
{
    switch (e->type()) {
    case QEvent::ActionAdded:
        d->m_contextMenu->insertAction(e->before() ? e->before() : d->m_separatorAction, e->action());
        break;
    case QEvent::ActionRemoved:
        d->m_contextMenu->removeAction(e->action());
        break;
    default:
        break;
    }
    QFrame::actionEvent(e);
}

InventoryWidget::Mode InventoryWidget::mode() const
{
    return d->m_mode;
}

void InventoryWidget::setMode(Mode newMode)
{
    if (d->m_mode != newMode) {
        d->m_mode = newMode;
        if (newMode == Mode::AppearsIn)
            d->m_appearsIn->setChecked(true);
        else
            d->m_consistsOf->setChecked(true);
        updateModel(d->m_list);
    }
}

const InventoryWidget::Item InventoryWidget::selected() const
{
    auto *m = qobject_cast<BrickLink::InventoryModel *>(d->m_view->model());

    if (m && !d->m_view->selectionModel()->selectedIndexes().isEmpty()) {
        const auto idx = d->m_view->selectionModel()->selectedIndexes().front();
        auto i = m->data(idx, BrickLink::ItemPointerRole).value<const BrickLink::Item *>();
        auto c = m->data(idx, BrickLink::ColorPointerRole).value<const BrickLink::Color *>();
        auto q = m->data(idx, BrickLink::QuantityRole).toInt();
        return Item { i, c, q };
    } else {
        return { };
    }
}

QCoro::Task<> InventoryWidget::partOut()
{
    auto entry = selected();

    if (entry.item && entry.item->hasInventory()) {
        ImportInventoryDialog dlg(entry.item, 1, BrickLink::Condition::Count, this);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();

        if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
            Document::fromPartInventory(dlg.item(), nullptr, dlg.quantity(), dlg.condition(),
                                        dlg.extraParts(), dlg.includeInstructions(),
                                        dlg.includeAlternates(), dlg.includeCounterParts());
        }
    }
}

QSize InventoryWidget::minimumSizeHint() const
{
    const QFontMetrics &fm = fontMetrics();

    return { fm.horizontalAdvance(QLatin1Char('m')) * 20, fm.height() * 4 };
}

QSize InventoryWidget::sizeHint() const
{
    return minimumSizeHint();
}

void InventoryWidget::setItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    updateModel({ { item, color } });
}

void InventoryWidget::setItems(const LotList &lots)

{
    QVector<QPair<const BrickLink::Item *, const BrickLink::Color *>> list;
    list.reserve(lots.size());
    for (const auto &lot : lots)
        list.append({ lot->item(), lot->color() });

    updateModel(list);
}

void InventoryWidget::updateModel(const QVector<QPair<const BrickLink::Item *, const BrickLink::Color *>> &list)
{
    d->m_list = list;

    QAbstractItemModel *old_model = d->m_view->model();
    auto mmode = (d->m_mode == Mode::AppearsIn) ? BrickLink::InventoryModel::Mode::AppearsIn
                                                : BrickLink::InventoryModel::Mode::ConsistsOf;
    d->m_view->setModel(new BrickLink::InventoryModel(mmode, list, this));
    resizeColumns();

    delete old_model;
}

void InventoryWidget::resizeColumns()
{
    setUpdatesEnabled(false);
    d->m_view->resizeColumnToContents(0);
    d->m_view->resizeColumnToContents(1);
    setUpdatesEnabled(true);
}

void InventoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QFrame::changeEvent(e);
}

#include "moc_inventorywidget.cpp"
