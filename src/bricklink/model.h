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

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include "bricklink/color.h"
#include "bricklink/itemtype.h"
#include "bricklink/color.h"
#include "utility/staticpointermodel.h"


namespace BrickLink {

class ColorModel : public StaticPointerModel
{
    Q_OBJECT
public:
    ColorModel(QObject *parent);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    using StaticPointerModel::index;
    QModelIndex index(const Color *color) const;
    const Color *color(const QModelIndex &index) const;

    bool isFiltered() const override;
    void unsetFilter();
    void setFilterItemType(const ItemType *it);
    void setFilterType(Color::Type type);
    void setFilterPopularity(qreal p);
    void setColorListFilter(const QVector<const Color *> &colorList);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    const ItemType *m_itemtype_filter = nullptr;
    Color::Type m_type_filter {};
    qreal m_popularity_filter = 0;
    QVector<const Color *> m_color_filter;

    friend class Core;
};


class CategoryModel : public StaticPointerModel
{
    Q_OBJECT
public:
    CategoryModel(QObject *parent);

    static const Category *AllCategories;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    using StaticPointerModel::index;
    QModelIndex index(const Category *category) const;
    const Category *category(const QModelIndex &index) const;

    bool isFiltered() const override;
    void setFilterItemType(const ItemType *it);
    void setFilterAllCategories(bool);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    const ItemType *m_itemtype_filter;
    bool m_all_filter;

    friend class Core;
};


class ItemTypeModel : public StaticPointerModel
{
    Q_OBJECT
public:
    ItemTypeModel(QObject *parent);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    using StaticPointerModel::index;
    QModelIndex index(const ItemType *itemtype) const;
    const ItemType *itemType(const QModelIndex &index) const;

    bool isFiltered() const override;
    void setFilterWithoutInventory(bool on);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    bool m_inv_filter;

    friend class Core;
};


class ItemModel : public StaticPointerModel
{
    Q_OBJECT
public:
    ItemModel(QObject *parent);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    using StaticPointerModel::index;
    QModelIndex index(const Item *item) const;
    const Item *item(const QModelIndex &index) const;

    bool isFiltered() const override;
    void setFilterItemType(const ItemType *it);
    void setFilterCategory(const Category *cat);
    void setFilterColor(const Color *col);
    void setFilterText(const QString &filter);
    void setFilterWithoutInventory(bool on);

protected slots:
    void pictureUpdated(BrickLink::Picture *);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    const ItemType *m_itemtype_filter = nullptr;
    const Category *m_category_filter = nullptr;
    const Color *   m_color_filter = nullptr;
    QString         m_text_filter;
    QVector<QPair<bool, QString>> m_filter_text;
    QVector<QPair<bool, QPair<const Item *, const Color *>>> m_filter_consistsOf;
    QVector<QPair<bool, const Item *>> m_filter_appearsIn;
    QPair<bool, QVector<const Item *>> m_filter_ids;
    bool            m_inv_filter = false;
    static QString  s_consistsOfPrefix;
    static QString  s_appearsInPrefix;
    static QString  s_idPrefix;

    friend class Core;
};


class AppearsInModel;

class InternalAppearsInModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    ~InternalAppearsInModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    const AppearsInItem *appearsIn(const QModelIndex &idx) const;
    QModelIndex index(const AppearsInItem *const_ai) const;

protected:
    InternalAppearsInModel(const QVector<QPair<const Item *, const Color *> > &list, QObject *parent);
    InternalAppearsInModel(const Item *item, const Color *color, QObject *parent);

    AppearsIn m_appearsin;
    QVector<AppearsInItem *> m_items;

    friend class AppearsInModel;
};

class AppearsInModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AppearsInModel(const QVector<QPair<const Item *, const Color *> > &list, QObject *parent);
    AppearsInModel(const Item *item, const Color *color, QObject *parent);

    using QSortFilterProxyModel::index;
    const AppearsInItem *appearsIn(const QModelIndex &idx) const;
    QModelIndex index(const AppearsInItem *const_ai) const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

} // namespace BrickLink
