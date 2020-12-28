/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

#include <QtConcurrentFilter>
#include <QtAlgorithms>

#include "qparallelsort.h"
#include "staticpointermodel.h"


StaticPointerModel::StaticPointerModel(QObject *parent)
    : QAbstractItemModel(parent)
{ }

void StaticPointerModel::init() const
{
    if (sorted.isEmpty()) {
        sorted.resize(pointerCount());
        std::iota(sorted.begin(), sorted.end(), 0);
    }
}

QModelIndex StaticPointerModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid() && row >= 0 && column >= 0 && row < rowCount() && column < columnCount()) {
        const void *pointer = pointerAt(isFiltered() ? filtered.at(row) : sorted.at(row));
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<void *>(pointer));
    }
    return {};
}

QModelIndex StaticPointerModel::parent(const QModelIndex &) const
{
    return {};
}


int StaticPointerModel::rowCount(const QModelIndex &parent) const
{
    init();

    if (parent.isValid())
        return 0;
    else if (isFiltered())
        return filtered.count();
    else
        return pointerCount();
}

const void *StaticPointerModel::pointer(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const void *>(index.internalPointer()) : nullptr;
}

QModelIndex StaticPointerModel::index(const void *pointer, int column) const
{
    init();

    int row = pointer ? pointerIndexOf(pointer) : -1;
    if (row >= 0) {
        if (isFiltered())
            row = filtered.indexOf(row);
        else
            row = sorted.indexOf(row);
    }
    return row >= 0 ? createIndex(row, column, const_cast<void *>(pointer)) : QModelIndex();
}

bool StaticPointerModel::isFiltered() const
{
    return false;
}

bool StaticPointerModel::filterAccepts(const void *) const
{
    return true;
}

bool StaticPointerModel::lessThan(const void *, const void *, int) const
{
    return true;
}

void StaticPointerModel::invalidateFilter()
{
    init();

    emit layoutAboutToBeChanged();
    QModelIndexList before = persistentIndexList();

    invalidateFilterInternal();

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(pointer(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged();
}

void StaticPointerModel::invalidateFilterInternal()
{
    if (isFiltered())
        filtered = QtConcurrent::filtered(sorted, StaticPointerModelFilter(this)).results();
    else
        filtered.clear();
}

void StaticPointerModel::sort(int column, Qt::SortOrder order)
{
    init();

    lastSortColumn = column;
    lastSortOrder = order;

    int n = pointerCount();
    if (n < 2)
        return;

    emit layoutAboutToBeChanged();
    QModelIndexList before = persistentIndexList();

    if (column >= 0 && column < columnCount()) {
        auto sorter = StaticPointerModelCompare(column, order, this);

        qParallelSort(sorted.begin(), sorted.end(), sorter);

        // c++17 alternatives, but not supported everywhere yet:
        //std::stable_sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);
        //std::sort(std::execution::par_unseq, sorted.begin(), sorted.end(), sorter);

    } else { // restore the source model order
        for (int i = 0; i < n; ++i)
            sorted[i] = i;
    }

    invalidateFilterInternal();

    QModelIndexList after;
    foreach (const QModelIndex &idx, before)
        after.append(index(pointer(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged();
}

int StaticPointerModel::sortColumn() const
{
    return lastSortColumn;
}

Qt::SortOrder StaticPointerModel::sortOrder() const
{
    return lastSortOrder;
}


#include "moc_staticpointermodel.cpp"
