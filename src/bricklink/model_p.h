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

#include <QAbstractTableModel>

#include "bricklink/item.h"

namespace BrickLink {

class InventoryModel;


class InternalInventoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Mode { AppearsIn, ConsistsOf };

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct Entry
    {
        Entry() = default;
        Entry(const BrickLink::Item *item, const BrickLink::Color *color, int quantity);

        const BrickLink::Item *m_item = nullptr;
        const BrickLink::Color *m_color = nullptr;
        int m_quantity = 0;
    };
    Entry entry(const QModelIndex &idx) const;

protected:
    InternalInventoryModel(Mode mode, const QVector<QPair<const Item *, const Color *>> &list,
                           QObject *parent);

    QVector<Entry> m_items;
    Mode m_mode;

    friend class InventoryModel;
};

} // namespace BrickLink
