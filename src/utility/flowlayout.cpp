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

#include <QStyle>
#include <QWidget>

#include "flowlayout.h"


FlowLayout::FlowLayout(QWidget *parent, int margin, int spacing)
    : QLayout(parent)
    , m_space(spacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int spacing)
    : FlowLayout(nullptr, margin, spacing)
{ }

FlowLayout::~FlowLayout()
{
    qDeleteAll(m_items);
}

int FlowLayout::spacing() const
{
    if (m_space >= 0) {
        return m_space;
    } else if (!parent()) {
        return -1;
    } else if (parent()->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent());
        return pw->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, pw);
    } else {
        return static_cast<QLayout *>(parent())->spacing();
    }
}

int FlowLayout::count() const
{
    return m_items.count();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return ((index >= 0) && (index < m_items.size())) ? m_items.at(index) : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    return ((index >= 0) && (index < m_items.size())) ? m_items.takeAt(index) : nullptr;
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
}

bool FlowLayout::hasHeightForWidth() const
{
    return false;
}

QSize FlowLayout::minimumSize() const
{
    QSize s;
    for (const auto &item : m_items)
        s = s.expandedTo(item->minimumSize());

    const auto margins = contentsMargins();
    return s + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize FlowLayout::sizeHint() const
{
    QSize s;
    for (const auto &item : m_items) {
        QSize sh = item->sizeHint();
        s.rwidth() = qMax(s.width(), sh.width());
        s.rheight() += sh.height();
    }
    s.rheight() += (m_items.count() - 1) * spacing();

    const auto margins = contentsMargins();
    return s + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);

    QRect r = rect.marginsRemoved(contentsMargins());
    int y = r.y();
    int space = spacing();
    bool tooBig = false;

    for (auto item : qAsConst(m_items)) {
        const QSize s = item->sizeHint();

        if (!tooBig && ((y + s.height() - 1) > r.bottom()))
            tooBig = true;
        if (tooBig)
            y = QWIDGETSIZE_MAX - s.height() - 1;

        int w = r.width();
        int h = s.height();
        if (item->hasHeightForWidth())
            h = item->heightForWidth(w);

        item->setGeometry({ r.x(), y, w, h });

        if (!tooBig)
            y += (h + space);
    }
}
