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
#pragma once

#include <QStyledItemDelegate>

#include "bricklink/global.h"
#include "desktop/betteritemdelegate.h"


namespace BrickLink {

class ItemDelegate : public BetterItemDelegate
{
    Q_OBJECT
public:
    ItemDelegate(Options options = None, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

public slots:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
};


class ToolTip : public QObject
{
    Q_OBJECT
public:
    static ToolTip *inst();

    bool show(const Item *item, const Color *color, const QPoint &globalPos, QWidget *parent);

private slots:
    void pictureUpdated(Picture *pic);

private:
    QString createToolTip(const Item *item, Picture *pic) const;

    static ToolTip *s_inst;
    Picture *m_tooltip_pic = nullptr;
};

} // namespace BrickLink
