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
#include <QLayout>
#include <QHeaderView>
#include <QTreeView>
#include <QEvent>

#include "bricklink.h"
#include "cutility.h"
#include "cselectcolor.h"


CSelectColor::CSelectColor(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    w_colors = new QTreeView(this);
    w_colors->setAlternatingRowColors(true);
    w_colors->setAllColumnsShowFocus(true);
    w_colors->setUniformRowHeights(true);
    w_colors->setRootIsDecorated(false);
    w_colors->setSortingEnabled(true);

    w_colors->setModel(new BrickLink::ColorProxyModel(BrickLink::core()->colorModel()));
    w_colors->sortByColumn(0, Qt::AscendingOrder);

    setFocusProxy(w_colors);

    connect(w_colors->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(colorChanged()));
    connect(w_colors, SIGNAL(activated(const QModelIndex &)), this, SLOT(colorConfirmed()));

    QBoxLayout *lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->addWidget(w_colors);

    recalcHighlightPalette();
}

void CSelectColor::setWidthToContents(bool b)
{
    if (b) {
        w_colors->resizeColumnToContents(0);
        w_colors->setFixedWidth(w_colors->columnWidth(0) + 2 * w_colors->frameWidth() + w_colors->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4);
    }
    else {
        w_colors->setMaximumWidth(INT_MAX);
    }
}

const BrickLink::Color *CSelectColor::currentColor() const
{
    if (w_colors->selectionModel()->hasSelection()) {
        QModelIndex idx = w_colors->selectionModel()->selectedIndexes().front();
        return qvariant_cast<const BrickLink::Color *>(w_colors->model()->data(idx, BrickLink::ColorPointerRole));
    }
    else
        return 0;
}

void CSelectColor::setCurrentColor(const BrickLink::Color *color)
{
    BrickLink::ColorProxyModel *model = qobject_cast<BrickLink::ColorProxyModel *>(w_colors->model());

    w_colors->setCurrentIndex(model->index(color));
}

void CSelectColor::colorChanged()
{
    emit colorSelected(currentColor(), false);
}

void CSelectColor::colorConfirmed()
{
    emit colorSelected(currentColor(), true);
}

void CSelectColor::showEvent(QShowEvent *)
{
    const BrickLink::Color *color = currentColor();

    if (color) {
        BrickLink::ColorProxyModel *model = qobject_cast<BrickLink::ColorProxyModel *>(w_colors->model());

        w_colors->scrollTo(model->index(color), QAbstractItemView::PositionAtCenter);
    }
}

void CSelectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            setCurrentColor(BrickLink::core()->color(0));
    }
    else if (e->type() == QEvent::PaletteChange) {
        recalcHighlightPalette();
    }
    QWidget::changeEvent(e);
}

void CSelectColor::recalcHighlightPalette()
{
    QPalette p;
    QColor hc = CUtility::gradientColor(p.color(QPalette::Active, QPalette::Highlight), p.color(QPalette::Inactive, QPalette::Highlight), 0.35f);
    QColor htc = p.color(QPalette::Active, QPalette::HighlightedText);

    p.setColor(QPalette::Inactive, QPalette::Highlight, hc);
    p.setColor(QPalette::Inactive, QPalette::HighlightedText, htc);
    setPalette(p);
}
