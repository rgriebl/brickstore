// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractItemModel>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QTimer)


class StaticPointerModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(bool isFiltered READ isFiltered NOTIFY isFilteredChanged FINAL)

public:
    StaticPointerModel(QObject *parent);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    Q_INVOKABLE void sort(int column, Qt::SortOrder order) override;
    void forceSort();

    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    virtual bool isFiltered() const;

    bool isFilterDelayEnabled() const;
    void setFilterDelayEnabled(bool enabled);


public slots:
    void invalidateFilter();
    void invalidateFilterNow();

signals:
    void isFilteredChanged();

protected:
    virtual int pointerCount() const = 0;
    virtual const void *pointerAt(int index) const = 0;
    virtual int pointerIndexOf(const void *pointer) const = 0;

    virtual bool filterAccepts(const void *pointer) const;
    virtual bool lessThan(const void *pointer1, const void *pointer2, int column, Qt::SortOrder order) const;

    QModelIndex index(const void *pointer, int column = 0) const;
    const void *pointer(const QModelIndex &index) const;

    void setFixedSortOrder(const QVector<const void *> &fixedOrder);

private:
    void init() const;
    void invalidateFilterDelayed();
    void invalidateFilterInternal();

    mutable QVector<int> sorted; // this needs to initialized in the first init() call
    QVector<int> filtered;
    int lastSortColumn = -1;
    Qt::SortOrder lastSortOrder = Qt::AscendingOrder;
    bool fixedSortOrder = false;
    bool filterDelayEnabled = false;
    QTimer *filterDelayTimer = nullptr;
};
