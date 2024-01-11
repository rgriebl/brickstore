// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>
#include <QStyledItemDelegate>

class BetterItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Option {
        None                  = 0x0,
        AlwaysShowSelection   = 0x1,
        FirstColumnImageOnly  = 0x2,
        MoreSpacing           = 0x4,
        Pinnable              = 0x8,
    };
    Q_DECLARE_FLAGS(Options, Option)

    BetterItemDelegate(Options options = None, QAbstractItemView *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setSectionHeaderRole(int role);
    int sectionHeaderRole() const;

    void setPinnedRole(int role);
    int pinnedRole() const;

protected:
    void extendedPaint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
                       const std::function<void ()> &paintCallback = { }) const;

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

    QRect pinRect(const QStyleOptionViewItem &option) const;

protected:
    Options m_options;
    int m_sectionHeaderRole = 0;
    int m_pinnedRole = 0;
    mutable QIcon m_pinIcon;
    mutable QIcon m_unpinIcon;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BetterItemDelegate::Options)
