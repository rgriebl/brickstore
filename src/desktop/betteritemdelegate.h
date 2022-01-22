/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
                       std::function<void()> paintCallback = { }) const;

protected:
    Options m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BetterItemDelegate::Options)
