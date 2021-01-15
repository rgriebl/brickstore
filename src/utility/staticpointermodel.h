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

QT_FORWARD_DECLARE_CLASS(QTimer)


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

    bool isFilterDelayEnabled() const;
    void setFilterDelayEnabled(bool enabled);

public slots:
    void invalidateFilter();
    void invalidateFilterNow();

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
    void invalidateFilterDelayed();
    void invalidateFilterInternal();

    mutable QVector<int> sorted; // this needs to initialized in the first init() call
    QVector<int> filtered;
    int lastSortColumn = -1;
    Qt::SortOrder lastSortOrder = Qt::AscendingOrder;
    bool filterDelayEnabled = false;
    QTimer *filterDelayTimer = nullptr;
};
