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

#include <QAbstractItemModel>
#include <QVector>
#include <QList>

class QTimer;


class StaticPointerModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    StaticPointerModel(QObject *parent);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void sort(int column, Qt::SortOrder order) override;

    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    virtual bool isFiltered() const;

public slots:
    void invalidateFilter();

protected:
    virtual int pointerCount() const = 0;
    virtual const void *pointerAt(int index) const = 0;
    virtual int pointerIndexOf(const void *pointer) const = 0;

    virtual bool filterAccepts(const void *pointer) const;
    virtual bool lessThan(const void *pointer1, const void *pointer2, int column) const;

    QModelIndex index(const void *pointer, int column = 0) const;
    const void *pointer(const QModelIndex &index) const;

private:
    void init() const;

    struct StaticPointerModelFilter
    {
        inline StaticPointerModelFilter(StaticPointerModel *model)
            : pointerModel(model) { }

        inline bool operator()(int row)
        { return pointerModel->filterAccepts(pointerModel->pointerAt(row)); }

        StaticPointerModel *pointerModel;
    };
    friend struct StaticPointerModelFilter;

    void invalidateFilterInternal();

    struct StaticPointerModelCompare
    {
        inline StaticPointerModelCompare(int column, Qt::SortOrder order, const StaticPointerModel *model)
            : sortColumn(column), sortOrder(order), pointerModel(model) { }

        inline bool operator()(int r1, int r2) const
        {
            const void *pointer1 = pointerModel->pointerAt(sortOrder == Qt::AscendingOrder ? r1 : r2);
            const void *pointer2 = pointerModel->pointerAt(sortOrder == Qt::AscendingOrder ? r2 : r1);
            return pointerModel->lessThan(pointer1, pointer2, sortColumn);
        }

    private:
        const int sortColumn;
        const Qt::SortOrder sortOrder;
        const StaticPointerModel *pointerModel;
    };
    friend struct StaticPointerModelCompare;

    mutable QVector<int> sorted; // this needs to initialized in the first init() call
    QList<int> filtered;
    int lastSortColumn = -1;
    Qt::SortOrder lastSortOrder = Qt::AscendingOrder;
};
