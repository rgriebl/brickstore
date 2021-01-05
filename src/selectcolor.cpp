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
#include <QLayout>
#include <QHeaderView>
#include <QTreeView>
#include <QEvent>
#include <QComboBox>

#include "bricklink_model.h"
#include "selectcolor.h"

class ColorTreeView : public QTreeView
{
    Q_OBJECT
public:
    QSize sizeHint() const override
    {
        QSize s = QTreeView::sizeHint();
        s.rheight() *= 2;
        return s;
    }
};

enum {
    KnownColors = 0,
    AllColors = -1,
    PopularColors = -2,
    MostPopularColors = -3,
};


SelectColor::SelectColor(QWidget *parent)
    : QWidget(parent)
{
    w_filter = new QComboBox();
    w_filter->addItem({ }, KnownColors);
    w_filter->insertSeparator(w_filter->count());
    w_filter->addItem({ }, AllColors);
    w_filter->addItem({ }, PopularColors);
    w_filter->addItem({ }, MostPopularColors);
    w_filter->insertSeparator(w_filter->count());

    for (auto ct = BrickLink::Color::Solid; ct & BrickLink::Color::Mask; ct = decltype(ct)(ct << 1)) {
        if (!BrickLink::Color::typeName(ct).isEmpty())
            w_filter->addItem({ }, ct);
    }
    w_filter->setMaxVisibleItems(w_filter->count());

    w_colors = new ColorTreeView();
    w_colors->setAlternatingRowColors(true);
    w_colors->setAllColumnsShowFocus(true);
    w_colors->setUniformRowHeights(true);
    w_colors->setRootIsDecorated(false);
    w_colors->setSortingEnabled(true);
    w_colors->setItemDelegate(new BrickLink::ItemDelegate(this, BrickLink::ItemDelegate::AlwaysShowSelection));

    m_colorModel = new BrickLink::ColorModel(this);
    m_colorModel->setFilterDelayEnabled(true);
    w_colors->setModel(m_colorModel);
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
    w_filter->setItemText(w_filter->findData(KnownColors), tr("Known Colors"));
    w_filter->setItemText(w_filter->findData(AllColors), tr("All Colors"));
    w_filter->setItemText(w_filter->findData(PopularColors), tr("Popular Colors"));
    w_filter->setItemText(w_filter->findData(MostPopularColors), tr("Most Popular Colors"));

    for (auto ct = BrickLink::Color::Solid; ct & BrickLink::Color::Mask; ct = decltype(ct)(ct << 1)) {
        const QString ctName = BrickLink::Color::typeName(ct);
        if (!ctName.isEmpty())
            w_filter->setItemText(w_filter->findData(int(ct)), tr("Only \"%1\" Colors").arg(ctName));
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
    int filter = index < 0 ? -1 : w_filter->itemData(index).toInt();

    m_colorModel->unsetFilter();

    if (filter > 0) {
        m_colorModel->setFilterType(static_cast<BrickLink::Color::TypeFlag>(filter));
        m_colorModel->setFilterPopularity(0);
    } else if (filter < 0){
        qreal popularity = 0;
        if (filter == -2)
            popularity = qreal(0.005);
        else if (filter == -3)
            popularity = qreal(0.05);

        // Modulex colors are fine in their own category, but not in the 'all' lists
        m_colorModel->setFilterType(BrickLink::Color::Type(BrickLink::Color::Mask) & ~BrickLink::Color::Modulex);
        m_colorModel->setFilterPopularity(popularity);
    } else if (filter == 0 && m_item) {
        m_colorModel->setColorListFilter(m_item->knownColors());
    }

    m_colorModel->invalidateFilterNow();
}

const BrickLink::Color *SelectColor::currentColor() const
{
    if (w_colors->selectionModel()->hasSelection()) {
        QModelIndex idx = w_colors->selectionModel()->selectedIndexes().front();
        return qvariant_cast<const BrickLink::Color *>(w_colors->model()->data(idx, BrickLink::ColorPointerRole));
    }
    return nullptr;
}

void SelectColor::setCurrentColor(const BrickLink::Color *color)
{
    setCurrentColorAndItem(color, nullptr);
}

void SelectColor::setCurrentColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item)
{
    m_item = item;

    w_colors->clearSelection();
    updateColorFilter(w_filter->currentIndex());

    auto colorIndex = m_colorModel->index(color);
    w_colors->setCurrentIndex(colorIndex);
    w_colors->scrollTo(colorIndex);
}

void SelectColor::colorChanged()
{
    emit colorSelected(currentColor(), false);
}

void SelectColor::colorConfirmed()
{
    emit colorSelected(currentColor(), true);
}

void SelectColor::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    if (const BrickLink::Color *color = currentColor())
        w_colors->scrollTo(m_colorModel->index(color), QAbstractItemView::PositionAtCenter);
}

void SelectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            setCurrentColor(BrickLink::core()->color(0));
    } else if (e->type() == QEvent::LanguageChange) {
        languageChange();
    }
    QWidget::changeEvent(e);
}

#include "moc_selectcolor.cpp"
#include "selectcolor.moc"
