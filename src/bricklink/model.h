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
#include "qqmlregistration.h"

#include "bricklink/color.h"
#include "bricklink/global.h"
#include "bricklink/item.h"
#include "bricklink/staticpointermodel.h"


namespace BrickLink {

class ColorModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    ColorModel(QObject *parent = nullptr);

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
    void setFilterPopularity(float p);
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
    float m_popularity_filter = 0.f;
    QVector<const Color *> m_color_filter;

    friend class Core;
};


class CategoryModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const BrickLink::ItemType *filterItemType READ filterItemType WRITE setFilterItemType NOTIFY isFilteredChanged)
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged)

public:
    CategoryModel(QObject *parent = nullptr);

    static const Category *AllCategories;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    using StaticPointerModel::index;
    QModelIndex index(const Category *category) const;
    const Category *category(const QModelIndex &index) const;

    bool isFiltered() const override;
    const ItemType *filterItemType() const;
    void setFilterItemType(const ItemType *it);
    bool filterWithoutInventory() const;
    void setFilterWithoutInventory(bool on);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    const ItemType *m_itemtype_filter = nullptr;
    bool m_inv_filter = false;

    friend class Core;
};


class ItemTypeModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged)

public:
    ItemTypeModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    using StaticPointerModel::index;
    QModelIndex index(const ItemType *itemtype) const;
    const ItemType *itemType(const QModelIndex &index) const;

    bool isFiltered() const override;
    bool filterWithoutInventory() const;
    void setFilterWithoutInventory(bool on);

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column) const override;

private:
    bool m_inv_filter = false;

    friend class Core;
};


class ItemModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const BrickLink::ItemType *filterItemType READ filterItemType WRITE setFilterItemType NOTIFY isFilteredChanged)
    Q_PROPERTY(const BrickLink::Category *filterCategory READ filterCategory WRITE setFilterCategory NOTIFY isFilteredChanged)
    Q_PROPERTY(const BrickLink::Color *filterColor READ filterColor WRITE setFilterColor NOTIFY isFilteredChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY isFilteredChanged)
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged)

public:
    ItemModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    using StaticPointerModel::index;
    QModelIndex index(const Item *item) const;
    const Item *item(const QModelIndex &index) const;

    bool isFiltered() const override;
    const ItemType *filterItemType() const;
    void setFilterItemType(const ItemType *it);
    const Category *filterCategory() const;
    void setFilterCategory(const Category *cat);
    const BrickLink::Color *filterColor() const;
    void setFilterColor(const Color *col);
    QString filterText() const;
    void setFilterText(const QString &filter);
    bool filterWithoutInventory() const;
    void setFilterWithoutInventory(bool on);

    void setFilterYearRange(int minYear, int maxYear);

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
    int             m_year_min_filter = 0;
    int             m_year_max_filter = 0;
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
    QHash<int, QByteArray> roleNames() const override;

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
    QML_ELEMENT
    QML_UNCREATABLE("")

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
