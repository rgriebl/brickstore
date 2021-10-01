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


FlowLayout::FlowLayout(QWidget *parent, FlowMode mode, int margin, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_mode(mode)
    , m_hspace(hSpacing)
    , m_vspace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(FlowMode mode, int margin, int hSpacing, int vSpacing)
    : FlowLayout(nullptr, mode, margin, hSpacing, vSpacing)
{ }

FlowLayout::~FlowLayout()
{
    qDeleteAll(m_items);
}

int FlowLayout::horizontalSpacing() const
{
    return (m_hspace >= 0) ? m_hspace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    return (m_vspace >= 0) ? m_vspace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    auto *p = parent();
    if (!p) {
        return -1;
    } else if (p->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(p);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout *>(p)->spacing();
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
    return (m_mode != VerticalOnly);
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout({ 0, 0, width, 0 }, true);
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
    if (hasHeightForWidth())
        return minimumSize();

    QSize s;
    for (const auto &item : m_items) {
        QSize sh = item->sizeHint();
        if (m_mode == VerticalOnly) {
            s.rwidth() = qMax(s.width(), sh.width());
            s.rheight() += sh.height();
        }
    }
    if (m_mode == VerticalOnly)
        s.rheight() += (m_items.count() - 1) * verticalSpacing();

    const auto margins = contentsMargins();
    return s + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    QRect r = rect.marginsRemoved(contentsMargins());
    int x = r.x();
    int y = r.y();
    int lineHeight = 0;
    bool tooBig = false;
    QLayoutItem *lastItem = nullptr;

    for (auto item : m_items) {
        const QSize s = item->sizeHint();
        int hspace = horizontalSpacing();
        if (hspace == -1) {
            hspace = item->widget()->style()->layoutSpacing(QSizePolicy::PushButton,
                                                            QSizePolicy::PushButton,
                                                            Qt::Horizontal);
        }
        int vspace = verticalSpacing();
        if (vspace == -1) {
            vspace = item->widget()->style()->layoutSpacing(QSizePolicy::PushButton,
                                                            QSizePolicy::PushButton,
                                                            Qt::Vertical);
        }
        switch (m_mode) {
        case VerticalOnly: {
            if (!tooBig && ((y + s.height() - 1) > r.bottom()))
                tooBig = true;
            if (tooBig)
                y = QWIDGETSIZE_MAX - s.height() - 1;

            int w = r.width();
            int h = s.height();
            if (item->hasHeightForWidth())
                h = item->heightForWidth(w);

            if (!testOnly)
                item->setGeometry({ x, y, w, h });

            if (!tooBig)
                y += (h + vspace);
            break;
        }
        case HorizontalFirst: {
            int w = s.width();
            int h = s.height();
            int nextX = x + w + hspace;

            if (nextX - hspace > r.right() && lineHeight > 0) {
                x = r.x();
                y = y + lineHeight + vspace;
                w = std::min(w, r.width());
                nextX = x + w + hspace;
                lineHeight = 0;

                if (!testOnly && lastItem && (lastItem->expandingDirections() & Qt::Horizontal)) {
                    QRect lr = lastItem->geometry();
                    lr.setRight(r.width());
                    lastItem->setGeometry(lr);
                }
            }
            if ((item == m_items.constLast()) && (item->expandingDirections() & Qt::Horizontal))
                w = r.width() - x + 1;

            if (!testOnly)
                item->setGeometry({ x, y, w, h });

            x = nextX;
            break;
        }
        }
        lineHeight = std::max(lineHeight, s.height());
        lastItem = item;
    }
    return y + lineHeight - rect.y() + contentsMargins().bottom();
}
