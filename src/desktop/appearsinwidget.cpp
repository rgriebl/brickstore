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
#include <QHeaderView>
#include <QApplication>
#include <QTimer>

#include "bricklink/core.h"
#include "bricklink/delegate.h"
#include "bricklink/model.h"
#include "utility/utility.h"
#include "common/document.h"
#include "importinventorydialog.h"
#include "picturewidget.h"

#include "appearsinwidget.h"


class AppearsInWidgetPrivate {
public:
    QTimer *m_resize_timer;
    QMenu *m_contextMenu;
    QAction *m_partOutAction;
    QAction *m_catalogAction;
    QAction *m_priceGuideAction;
    QAction *m_lotsForSaleAction;
};

AppearsInWidget::AppearsInWidget(QWidget *parent)
    : QTreeView(parent)
    , d(new AppearsInWidgetPrivate())
{
    d->m_resize_timer = new QTimer(this);
    d->m_resize_timer->setSingleShot(true);

    d->m_contextMenu = new QMenu(this);

    setAlternatingRowColors(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    sortByColumn(0, Qt::DescendingOrder);
    header()->setSortIndicatorShown(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new BrickLink::ItemDelegate(BrickLink::ItemDelegate::None, this));

    d->m_partOutAction = new QAction(this);
    d->m_partOutAction->setObjectName(u"appearsin_partoutitems"_qs);
    d->m_partOutAction->setIcon(QIcon::fromTheme(u"edit-partoutitems"_qs));
    connect(d->m_partOutAction, &QAction::triggered,
            this, &AppearsInWidget::partOut);

    d->m_catalogAction = new QAction(this);
    d->m_catalogAction->setObjectName(u"appearsin_bl_catalog"_qs);
    d->m_catalogAction->setIcon(QIcon::fromTheme(u"bricklink-catalog"_qs));
    connect(d->m_catalogAction, &QAction::triggered, this, [this]() {
        const BrickLink::AppearsInItem *ai = appearsIn();

        if (ai && ai->second)
            BrickLink::core()->openUrl(BrickLink::Url::CatalogInfo, ai->second);
    });

    d->m_priceGuideAction = new QAction(this);
    d->m_priceGuideAction->setObjectName(u"appearsin_bl_priceguide"_qs);
    d->m_priceGuideAction->setIcon(QIcon::fromTheme(u"bricklink-priceguide"_qs));
    connect(d->m_priceGuideAction, &QAction::triggered, this, [this]() {
        const BrickLink::AppearsInItem *ai = appearsIn();

        if (ai && ai->second)
            BrickLink::core()->openUrl(BrickLink::Url::PriceGuideInfo, ai->second,
                                       BrickLink::core()->color(0));
    });

    d->m_lotsForSaleAction = new QAction(this);
    d->m_lotsForSaleAction->setObjectName(u"appearsin_bl_lotsforsale"_qs);
    d->m_lotsForSaleAction->setIcon(QIcon::fromTheme(u"bricklink-lotsforsale"_qs));
    connect(d->m_lotsForSaleAction, &QAction::triggered, this, [this]() {
        const BrickLink::AppearsInItem *ai = appearsIn();

        if (ai && ai->second)
            BrickLink::core()->openUrl(BrickLink::Url::LotsForSale, ai->second,
                                       BrickLink::core()->color(0));
    });

    d->m_contextMenu->addAction(d->m_partOutAction);
    d->m_contextMenu->addSeparator();
    d->m_contextMenu->addAction(d->m_catalogAction);
    d->m_contextMenu->addAction(d->m_priceGuideAction);
    d->m_contextMenu->addAction(d->m_lotsForSaleAction);

    connect(d->m_resize_timer, &QTimer::timeout,
            this, &AppearsInWidget::resizeColumns);

    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        if (appearsIn())
            d->m_contextMenu->popup(viewport()->mapToGlobal(pos));
    });

    connect(this, &QAbstractItemView::doubleClicked,
            this, &AppearsInWidget::partOut);

    languageChange();
    setItem(nullptr, nullptr);
}

AppearsInWidget::~AppearsInWidget()
{ /* needed to use std::unique_ptr on d */ }

void AppearsInWidget::languageChange()
{
    d->m_partOutAction->setText(tr("Part out Item..."));
    d->m_catalogAction->setText(tr("Show BrickLink Catalog Info..."));
    d->m_priceGuideAction->setText(tr("Show BrickLink Price Guide Info..."));
    d->m_lotsForSaleAction->setText(tr("Show Lots for Sale on BrickLink..."));
}


const BrickLink::AppearsInItem *AppearsInWidget::appearsIn() const
{
    auto *m = qobject_cast<BrickLink::AppearsInModel *>(model());

    if (m && !selectionModel()->selectedIndexes().isEmpty())
        return m->appearsIn(selectionModel()->selectedIndexes().front());
    else
        return nullptr;
}

QCoro::Task<> AppearsInWidget::partOut()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second) {
        ImportInventoryDialog dlg(ai->second, 1, BrickLink::Condition::Count, this);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();

        if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
            Document::fromPartInventory(dlg.item(), nullptr, dlg.quantity(), dlg.condition(),
                                        dlg.extraParts(), dlg.includeInstructions(),
                                        dlg.includeAlternates(), dlg.includeCounterParts());
        }
    }
}

QSize AppearsInWidget::minimumSizeHint() const
{
    const QFontMetrics &fm = fontMetrics();

    return { fm.horizontalAdvance(QLatin1Char('m')) * 20, fm.height() * 4 };
}

QSize AppearsInWidget::sizeHint() const
{
    return minimumSizeHint();
}

void AppearsInWidget::setItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    QAbstractItemModel *old_model = model();

    setModel(new BrickLink::AppearsInModel(item, color, this));
    resizeColumns();

    delete old_model;
}

void AppearsInWidget::setItems(const LotList &lots)
{
    QAbstractItemModel *old_model = model();

    QVector<QPair<const BrickLink::Item *, const BrickLink::Color *>> list;
    list.reserve(lots.size());
    for (const auto &lot : lots)
        list.append({ lot->item(), lot->color() });

    setModel(new BrickLink::AppearsInModel(list, this));
    resizeColumns();

    delete old_model;
}

void AppearsInWidget::resizeColumns()
{
    setUpdatesEnabled(false);
    resizeColumnToContents(0);
    resizeColumnToContents(1);
    setUpdatesEnabled(true);
}

void AppearsInWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QTreeView::changeEvent(e);
}

#include "moc_appearsinwidget.cpp"
