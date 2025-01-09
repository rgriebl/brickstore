// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtConcurrentFilter>
#include <QtAlgorithms>
#include <QTimer>

#include "utility/qparallelsort.h"
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
        return createIndex(row, column, const_cast<void *>(pointer));
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
        return int(filtered.count());
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

    auto row = pointer ? pointerIndexOf(pointer) : -1;
    if (row >= 0) {
        if (isFiltered())
            row = int(filtered.indexOf(row));
        else
            row = int(sorted.indexOf(row));
    }
    return row >= 0 ? createIndex(row, column, const_cast<void *>(pointer)) : QModelIndex();
}

bool StaticPointerModel::isFiltered() const
{
    return false;
}

bool StaticPointerModel::isFilterDelayEnabled() const
{
    return filterDelayEnabled;
}

void StaticPointerModel::setFilterDelayEnabled(bool enabled)
{
    filterDelayEnabled = enabled;
}

bool StaticPointerModel::filterAccepts(const void *) const
{
    return true;
}

bool StaticPointerModel::lessThan(const void *, const void *, int, Qt::SortOrder) const
{
    return true;
}

void StaticPointerModel::invalidateFilter()
{
    if (!filterDelayEnabled) {
        invalidateFilterNow();
    } else {
        if (!filterDelayTimer) {
            filterDelayTimer = new QTimer(this);
            filterDelayTimer->setSingleShot(true);
            connect(filterDelayTimer, &QTimer::timeout,
                    this, &StaticPointerModel::invalidateFilterDelayed);
        }
        filterDelayTimer->start();
    }
}

void StaticPointerModel::invalidateFilterNow()
{
    if (filterDelayTimer && filterDelayTimer->isActive())
        filterDelayTimer->stop();
    invalidateFilterDelayed();
}

void StaticPointerModel::invalidateFilterDelayed()
{
    init();

    beginResetModel();
    invalidateFilterInternal();
    endResetModel();

// Using layoutChanged would be faster, but it leads to weird crashes in QListView in debug builds
//
//    emit layoutAboutToBeChanged({ }, VerticalSortHint);
//    const QModelIndexList before = persistentIndexList();
//
//    invalidateFilterInternal();
//
//    QModelIndexList after;
//    for (const QModelIndex &idx : before)
//        after.append(index(pointer(idx), idx.column()));
//    changePersistentIndexList(before, after);
//    emit layoutChanged({ }, VerticalSortHint);
}

void StaticPointerModel::invalidateFilterInternal()
{
    if (isFiltered()) {
        filtered = QtConcurrent::blockingFiltered(sorted, [this](int row) {
            return filterAccepts(pointerAt(row));
        });
    } else {
        filtered.clear();
    }
}

void StaticPointerModel::sort(int column, Qt::SortOrder order)
{
    init();

    if ((column == lastSortColumn) && (order == lastSortOrder) && !fixedSortOrder)
        return;

    lastSortColumn = column;
    lastSortOrder = order;
    fixedSortOrder = false;

    int n = pointerCount();
    if (n < 2)
        return;

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    const QModelIndexList before = persistentIndexList();

    if (column >= 0 && column < columnCount()) {
        qParallelSort(sorted.begin(), sorted.end(),
                      [column, order, this](int r1, int r2) {
            const void *pointer1 = pointerAt(order == Qt::AscendingOrder ? r1 : r2);
            const void *pointer2 = pointerAt(order == Qt::AscendingOrder ? r2 : r1);
            return lessThan(pointer1, pointer2, column, order);
        });
    } else { // restore the source model order
        for (int i = 0; i < n; ++i)
            sorted[i] = i;
    }

    if (filterDelayTimer && filterDelayTimer->isActive())
        filterDelayTimer->stop();
    invalidateFilterInternal();

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(pointer(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);
}

void StaticPointerModel::forceSort()
{
    if (fixedSortOrder)
        return;
    fixedSortOrder = true; // force a re-sort
    sort(lastSortColumn, lastSortOrder);
}

int StaticPointerModel::sortColumn() const
{
    return lastSortColumn;
}

Qt::SortOrder StaticPointerModel::sortOrder() const
{
    return lastSortOrder;
}

void StaticPointerModel::setFixedSortOrder(const QVector<const void *> &fixedOrder)
{
    if (fixedSortOrder && fixedOrder.isEmpty()) {
        sort(lastSortColumn, lastSortOrder);
        return;
    }

    init();

    fixedSortOrder = true;

    int n = pointerCount();
    if (n < 2)
        return;

    QHash<const void *, qsizetype> hash;
    qsizetype nextIndex = 0;
    for (const auto &ptr : fixedOrder)
        hash.insert(ptr, nextIndex++);

    emit layoutAboutToBeChanged({ }, VerticalSortHint);
    const QModelIndexList before = persistentIndexList();

    qParallelSort(sorted.begin(), sorted.end(),
                  [hash, this](int r1, int r2) {
                      const void *pointer1 = pointerAt(r1);
                      const void *pointer2 = pointerAt(r2);
                      auto pos1 = hash.value(pointer1, -1);
                      auto pos2 = hash.value(pointer2, -1);

                      if (pos1 >= 0 && pos2 >= 0)
                          return (pos1 < pos2);
                      else if (pos1 >= 0)
                          return true;
                      else if (pos2 >= 0)
                          return false;
                      else
                          return lessThan(pointer1, pointer2, lastSortColumn, lastSortOrder);
                  });

    if (filterDelayTimer && filterDelayTimer->isActive())
        filterDelayTimer->stop();
    invalidateFilterInternal();

    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &idx : before)
        after.append(index(pointer(idx), idx.column()));
    changePersistentIndexList(before, after);
    emit layoutChanged({ }, VerticalSortHint);
}

#include "moc_staticpointermodel.cpp"
