// Copyright (C) 2004-2023 Robert Griebl
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
    };
    Q_DECLARE_FLAGS(Options, Option)

    BetterItemDelegate(Options options = None, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    void extendedPaint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
                       const std::function<void ()> &paintCallback = { }) const;

protected:
    Options m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BetterItemDelegate::Options)
