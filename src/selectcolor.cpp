/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QComboBox>

#include "bricklink.h"
#include "selectcolor.h"


SelectColor::SelectColor(QWidget *parent)
    : QWidget(parent)
{
    w_filter = new QComboBox();
    w_filter->addItem(QString(), 0);
    w_filter->addItem(QString(), -1);
    w_filter->addItem(QString(), -2);
    w_filter->insertSeparator(w_filter->count());

    for (int i = 0; (1 << i) & BrickLink::Color::Mask; ++i) {
        if (!BrickLink::Color::typeName(static_cast<BrickLink::Color::TypeFlag>(1 << i)).isEmpty())
            w_filter->addItem(QString(), 1 << i);
    }

    w_colors = new QTreeView();
    w_colors->setAlternatingRowColors(true);
    w_colors->setAllColumnsShowFocus(true);
    w_colors->setUniformRowHeights(true);
    w_colors->setRootIsDecorated(false);
    w_colors->setSortingEnabled(true);
    w_colors->setItemDelegate(new BrickLink::ItemDelegate(this, BrickLink::ItemDelegate::AlwaysShowSelection));

    w_colors->setModel(new BrickLink::ColorModel(this));
    w_colors->sortByColumn(0, Qt::AscendingOrder);

    setFocusProxy(w_colors);

    connect(w_colors->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SelectColor::colorChanged);
    connect(w_colors, &QAbstractItemView::activated,
            this, &SelectColor::colorConfirmed);
    connect(w_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SelectColor::updateColorFilter);

    auto *lay = new QGridLayout(this);
    lay->setMargin(0);
    lay->setRowStretch(0, 0);
    lay->setRowStretch(1, 1);
    lay->addWidget(w_filter, 0, 0);
    lay->addWidget(w_colors, 1, 0);

    languageChange();
}

void SelectColor::languageChange()
{
    w_filter->setItemText(0, tr("All Colors"));
    w_filter->setItemText(1, tr("Popular Colors"));
    w_filter->setItemText(2, tr("Most Popular Colors"));

    for (int i = 0, j = 4; (1 << i) & BrickLink::Color::Mask; ++i) {
        const QString type = BrickLink::Color::typeName(static_cast<BrickLink::Color::TypeFlag>(1 << i));
        if (!type.isEmpty())
            w_filter->setItemText(j++, tr("Only \"%1\" Colors").arg(type));
    }
}

void SelectColor::setWidthToContents(bool b)
{
    if (b) {
        int w1 = w_colors->QAbstractItemView::sizeHintForColumn(0)
                + 2 * w_colors->frameWidth()
                + w_colors->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4;
        int w2 = w_filter->minimumSizeHint().width();
        w_filter->setFixedWidth(qMax(w1, w2));
        w_colors->setFixedWidth(qMax(w1, w2));
    }
    else {
        w_colors->setMaximumWidth(INT_MAX);
        w_filter->setMaximumWidth(INT_MAX);
    }
}

void SelectColor::updateColorFilter(int index)
{
    int filter = w_filter->itemData(index < 0 ? 0 : index).toInt();
    auto *model = qobject_cast<BrickLink::ColorModel *>(w_colors->model());

    if (filter > 0) {
        model->setFilterType(static_cast<BrickLink::Color::TypeFlag>(filter));
        model->setFilterPopularity(0);
    } else {
        qreal popularity = 0;
        if (filter == -1)
            popularity = qreal(0.005);
        else if (filter == -2)
            popularity = qreal(0.05);

        model->setFilterType(nullptr);
        model->setFilterPopularity(popularity);
    }
}

const BrickLink::Color *SelectColor::currentColor() const
{
    if (w_colors->selectionModel()->hasSelection()) {
        QModelIndex idx = w_colors->selectionModel()->selectedIndexes().front();
        return qvariant_cast<const BrickLink::Color *>(w_colors->model()->data(idx, BrickLink::ColorPointerRole));
    }
    else
        return nullptr;
}

void SelectColor::setCurrentColor(const BrickLink::Color *color)
{
    auto *model = qobject_cast<BrickLink::ColorModel *>(w_colors->model());

    w_colors->setCurrentIndex(model->index(color));
}

void SelectColor::colorChanged()
{
    emit colorSelected(currentColor(), false);
}

void SelectColor::colorConfirmed()
{
    emit colorSelected(currentColor(), true);
}

void SelectColor::showEvent(QShowEvent *)
{
    const BrickLink::Color *color = currentColor();

    if (color) {
        auto *model = qobject_cast<BrickLink::ColorModel *>(w_colors->model());

        w_colors->scrollTo(model->index(color), QAbstractItemView::PositionAtCenter);
    }
}

void SelectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            setCurrentColor(BrickLink::core()->color(0));
    }
    QWidget::changeEvent(e);
}

#include "moc_selectcolor.cpp"
