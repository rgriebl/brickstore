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
#include <QMenu>
#include <QVariant>
#include <QAction>
#include <QHeaderView>
#include <QDesktopServices>
#include <QApplication>
#include <QTimer>

#include "framework.h"
#include "picturewidget.h"
#include "bricklink.h"

#include "appearsinwidget.h"


class AppearsInWidgetPrivate {
public:
    QTimer *                m_resize_timer;
};

AppearsInWidget::AppearsInWidget(QWidget *parent)
    : QTreeView(parent)
{
    d = new AppearsInWidgetPrivate();
    d->m_resize_timer = new QTimer(this);
    d->m_resize_timer->setSingleShot(true);

    setAlternatingRowColors(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
    header()->setSortIndicatorShown(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new BrickLink::ItemDelegate(this));

    QAction *a;
    a = new QAction(this);
    a->setObjectName("appearsin_partoutitems");
    a->setIcon(QIcon(":/images/22x22/edit_partoutitems"));
    connect(a, SIGNAL(triggered()), this, SLOT(partOut()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("appearsin_magnify");
    a->setIcon(QIcon(":/images/22x22/viewmagp"));
    connect(a, SIGNAL(triggered()), this, SLOT(viewLargeImage()));
    addAction(a);

    a = new QAction(this);
    a->setSeparator(true);
    addAction(a);

    a = new QAction(this);
    a->setObjectName("appearsin_bl_catalog");
    a->setIcon(QIcon(":/images/22x22/edit_bl_catalog"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLCatalogInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("appearsin_bl_priceguide");
    a->setIcon(QIcon(":/images/22x22/edit_bl_priceguide"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLPriceGuideInfo()));
    addAction(a);
    a = new QAction(this);
    a->setObjectName("appearsin_bl_lotsforsale");
    a->setIcon(QIcon(":/images/22x22/edit_bl_lotsforsale"));
    connect(a, SIGNAL(triggered()), this, SLOT(showBLLotsForSale()));
    addAction(a);

    connect(d->m_resize_timer, SIGNAL(timeout()), this, SLOT(resizeColumns()));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(partOut()));

    languageChange();
    setItem(0, 0);
}

void AppearsInWidget::languageChange()
{
    findChild<QAction *>("appearsin_partoutitems")->setText(tr("Part out Item..."));
    findChild<QAction *>("appearsin_magnify")->setText(tr("View large image..."));
    findChild<QAction *>("appearsin_bl_catalog")->setText(tr("Show BrickLink Catalog Info..."));
    findChild<QAction *>("appearsin_bl_priceguide")->setText(tr("Show BrickLink Price Guide Info..."));
    findChild<QAction *>("appearsin_bl_lotsforsale")->setText(tr("Show Lots for Sale on BrickLink..."));
}

AppearsInWidget::~AppearsInWidget()
{
    delete d;
}

void AppearsInWidget::showContextMenu(const QPoint &pos)
{
    if (appearsIn())
        QMenu::exec(actions(), viewport()->mapToGlobal(pos));
}


const BrickLink::AppearsInItem *AppearsInWidget::appearsIn() const
{
    BrickLink::AppearsInModel *m = qobject_cast<BrickLink::AppearsInModel *>(model());

    if (m && !selectionModel()->selectedIndexes().isEmpty())
        return m->appearsIn(selectionModel()->selectedIndexes().front());
    else
        return 0;
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

    return QSize(fm.width('m') * 20, fm.height() * 6);
}

QSize AppearsInWidget::sizeHint() const
{
    return minimumSizeHint() * 2;
}

void AppearsInWidget::setItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    QAbstractItemModel *old_model = model();

    setModel(new BrickLink::AppearsInModel(item , color, this));
    triggerColumnResize();

    delete old_model;
}

void AppearsInWidget::setItems(const BrickLink::InvItemList &list)
{
    QAbstractItemModel *old_model = model();

    setModel(new BrickLink::AppearsInModel(list, this));
    triggerColumnResize();

    delete old_model;
}

void AppearsInWidget::triggerColumnResize()
{
/*    if (model()->rowCount() > 0) {
        d->m_resize_timer->start(100);
    } else {
        d->m_resize_timer->stop();*/
        resizeColumns();
    //}
}

void AppearsInWidget::resizeColumns()
{
    setUpdatesEnabled(false);
    resizeColumnToContents(0);
    resizeColumnToContents(1);
    setUpdatesEnabled(true);
}

void AppearsInWidget::viewLargeImage()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second) {
        BrickLink::Picture *lpic = BrickLink::core()->largePicture(ai->second, true);

        if (lpic) {
            LargePictureWidget *l = new LargePictureWidget(lpic, this);
            l->show();
            l->raise();
            l->activateWindow();
            l->setFocus();
        }
    }
}

void AppearsInWidget::showBLCatalogInfo()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo, ai->second));
}

void AppearsInWidget::showBLPriceGuideInfo()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, ai->second, BrickLink::core()->color(0)));
}

void AppearsInWidget::showBLLotsForSale()
{
    const BrickLink::AppearsInItem *ai = appearsIn();

    if (ai && ai->second)
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale, ai->second, BrickLink::core()->color(0)));
}
