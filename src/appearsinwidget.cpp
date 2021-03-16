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
#include <QMenu>
#include <QVariant>
#include <QAction>
#include <QHeaderView>
#include <QApplication>
#include <QTimer>

#include "utility.h"
#include "framework.h"
#include "picturewidget.h"
#include "bricklink_model.h"

#include "appearsinwidget.h"


class AppearsInWidgetPrivate {
public:
    QTimer *                m_resize_timer;
};

AppearsInWidget::AppearsInWidget(QWidget *parent)
    : QTreeView(parent)
    , d(new AppearsInWidgetPrivate())
{
    d->m_resize_timer = new QTimer(this);
    d->m_resize_timer->setSingleShot(true);

    setAlternatingRowColors(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    sortByColumn(0, Qt::DescendingOrder);
    header()->setSortIndicatorShown(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new BrickLink::ItemDelegate(BrickLink::ItemDelegate::None, this));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("appearsin_partoutitems"_l1);
    a->setIcon(QIcon::fromTheme("edit-partoutitems"_l1));
    connect(a, &QAction::triggered,
            this, &AppearsInWidget::partOut);
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("appearsin_bl_catalog"_l1);
    a->setIcon(QIcon::fromTheme("bricklink-catalog"_l1));
    connect(a, &QAction::triggered,
            this, &AppearsInWidget::showBLCatalogInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("appearsin_bl_priceguide"_l1);
    a->setIcon(QIcon::fromTheme("bricklink-priceguide"_l1));
    connect(a, &QAction::triggered,
            this, &AppearsInWidget::showBLPriceGuideInfo);
    addAction(a);
    a = new QAction(this);
    a->setObjectName("appearsin_bl_lotsforsale"_l1);
    a->setIcon(QIcon::fromTheme("bricklink-lotsforsale"_l1));
    connect(a, &QAction::triggered,
            this, &AppearsInWidget::showBLLotsForSale);
    addAction(a);

    connect(d->m_resize_timer, &QTimer::timeout,
            this, &AppearsInWidget::resizeColumns);
    connect(this, &QWidget::customContextMenuRequested,
            this, &AppearsInWidget::showContextMenu);
    connect(this, &QAbstractItemView::activated,
            this, &AppearsInWidget::partOut);

    languageChange();
    setItem(nullptr, nullptr);
}

AppearsInWidget::~AppearsInWidget()
{ /* needed to use QScopedPointer on d */ }

void AppearsInWidget::languageChange()
{
    findChild<QAction *>("appearsin_partoutitems"_l1)->setText(tr("Part out Item..."));
    findChild<QAction *>("appearsin_bl_catalog"_l1)->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *>("appearsin_bl_priceguide"_l1)->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *>("appearsin_bl_lotsforsale"_l1)->setText(tr("Show Lots for Sale on BrickLink..."));
}

void AppearsInWidget::showContextMenu(const QPoint &pos)
{
    if (appearsIn())
        QMenu::exec(actions(), viewport()->mapToGlobal(pos));
}


const BrickLink::AppearsInItem *AppearsInWidget::appearsIn() const
{
    auto *m = qobject_cast<BrickLink::AppearsInModel *>(model());

    if (m && !selectionModel()->selectedIndexes().isEmpty())
        return m->appearsIn(selectionModel()->selectedIndexes().front());
    else
        return nullptr;
}

void AppearsInWidget::partOut()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        FrameWork::inst()->fileImportBrickLinkInventory(ai->second);
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

void AppearsInWidget::showBLCatalogInfo()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, ai->second);
}

void AppearsInWidget::showBLPriceGuideInfo()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, ai->second, BrickLink::core()->color(0));
}

void AppearsInWidget::showBLLotsForSale()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, ai->second, BrickLink::core()->color(0));
}

#include "moc_appearsinwidget.cpp"
