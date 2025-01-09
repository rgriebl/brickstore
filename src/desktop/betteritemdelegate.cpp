// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPainter>
#include <QMouseEvent>
#include <QAbstractItemView>
#include <QApplication>
#include <QStyle>

#include "betteritemdelegate.h"


BetterItemDelegate::BetterItemDelegate(Options options, QAbstractItemView *parent)
    : QStyledItemDelegate(parent)
    , m_options(options)
{
    if (m_options.testFlag(Pinnable)) {
        Q_ASSERT(parent);
        parent->setProperty("pinnableItems", true);
    }
}

void BetterItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    extendedPaint(painter, option, index);
}

QSize BetterItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize ext(0, 2); // height + 2 looks better
    if (m_options & MoreSpacing)
        ext.rwidth() += option.fontMetrics.height();

    return QStyledItemDelegate::sizeHint(option, index) + ext;
}

void BetterItemDelegate::setSectionHeaderRole(int role)
{
    m_sectionHeaderRole = role;
}

int BetterItemDelegate::sectionHeaderRole() const
{
    return m_sectionHeaderRole;
}

void BetterItemDelegate::setPinnedRole(int role)
{
    m_pinnedRole = role;
}

int BetterItemDelegate::pinnedRole() const
{
    return m_pinnedRole;
}

void BetterItemDelegate::extendedPaint(QPainter *painter, const QStyleOptionViewItem &option,
                                       const QModelIndex &index,
                                       const std::function<void()> &paintCallback) const
{
    QStyleOptionViewItem myoption(option);
    initStyleOption(&myoption, index);

    bool isSectionHeader = m_sectionHeaderRole ? index.data(m_sectionHeaderRole).toBool() : false;
    bool firstColumnImageOnly = (m_options & FirstColumnImageOnly) && (index.column() == 0)
                                && !isSectionHeader;
    bool pinnable = false;
    bool pinned = false;
    if ((m_options & Pinnable) && m_pinnedRole) {
        QVariant v = index.data(m_pinnedRole);
        pinnable = !v.isNull();
        pinned = pinnable && v.toBool();
    }

    bool useFrameHover = false;
    bool useFrameSelection = false;

    if (firstColumnImageOnly) {
        useFrameSelection = (option.state & QStyle::State_Selected);
        useFrameHover = (option.state & QStyle::State_MouseOver);
        if (useFrameSelection)
            myoption.state &= ~QStyle::State_Selected;
    } else if (isSectionHeader) {
        myoption.state.setFlag(QStyle::State_MouseOver, false);
        myoption.state.setFlag(QStyle::State_HasFocus, false);

        myoption.font.setBold(true);
        myoption.font.setItalic(true);
    }

    if ((m_options & AlwaysShowSelection) && (option.state & QStyle::State_Enabled))
        myoption.state |= QStyle::State_Active;

    QStyledItemDelegate::paint(painter, myoption, index);

    if (paintCallback)
        paintCallback();

    if (pinnable) {
        if (m_pinIcon.isNull())
            m_pinIcon = QIcon::fromTheme(u"window-pin"_qs);
        if (m_unpinIcon.isNull())
            m_unpinIcon = QIcon::fromTheme(u"window-unpin"_qs);

        bool hovering = myoption.state.testFlag(QStyle::State_MouseOver);

        if (hovering || pinned) {
            auto oldOpacity = painter->opacity();
            painter->setOpacity(hovering ? 0.25 : 1);

            auto &icon = (hovering && pinned) ? m_unpinIcon : m_pinIcon;
            icon.paint(painter, pinRect(myoption));

            painter->setOpacity(oldOpacity);
        }
    }

    if (firstColumnImageOnly && (useFrameSelection || useFrameHover)) {
        painter->save();
        QColor c = option.palette.color(QPalette::Highlight);
        for (int i = 0; i < 6; ++i) {
            c.setAlphaF((useFrameHover && !useFrameSelection ? 0.6f : 1.f) - float(i) * .1f);
            painter->setPen(c);
            painter->drawRect(option.rect.adjusted(i, i, -i - 1, -i - 1));
        }
        painter->restore();
    }
}

bool BetterItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if ((event->type() == QEvent::MouseButtonPress) || (event->type() == QEvent::MouseButtonRelease)) {
        if ((m_options & Pinnable) && m_pinnedRole) {
            QRect r = pinRect(option);
            QPoint pos = static_cast<QMouseEvent *>(event)->pos();

            if (r.contains(pos)) {
                QVariant v = index.data(m_pinnedRole);
                if (!v.isNull()) {
                    if (event->type() == QEvent::MouseButtonRelease) {
                        bool pinned = v.toBool();
                        QMetaObject::invokeMethod(model, [=, role = m_pinnedRole]() {
                            model->setData(index, !pinned, role);
                        }, Qt::QueuedConnection);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

QRect BetterItemDelegate::pinRect(const QStyleOptionViewItem &option) const
{
    QRect r = option.rect;
    r.setLeft(r.left() + r.width() - option.decorationSize.width() - 4);
    int dy = (r.height() - r.width()) / 2;
    r.adjust(2, dy + 2, -2, -dy - 2);
    return r;
}

#include "moc_betteritemdelegate.cpp"
