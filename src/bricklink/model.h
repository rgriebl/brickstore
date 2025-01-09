// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

#include "bricklink/color.h"
#include "bricklink/global.h"
#include "bricklink/item.h"
#include "bricklink/staticpointermodel.h"


namespace BrickLink {

class ColorModel : public StaticPointerModel
{
    Q_OBJECT

public:
    ColorModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

    using StaticPointerModel::index;
    QModelIndex index(const Color *color) const;
    const Color *color(const QModelIndex &index) const;

    bool isFiltered() const override;
    void clearFilters();
    ColorType colorTypeFilter() const;
    void setColorTypeFilter(ColorType type);
    float popularityFilter() const;
    void setPopularityFilter(float p);
    const QVector<const Color *> colorListFilter() const;
    void setColorListFilter(const QVector<const Color *> &colorList);

    void pinId(uint colorId, bool down = true);
    void setPinnedIds(const QSet<uint> &colorIds);
    QSet<uint> pinnedIds() const;

signals:
    void colorTypeFilterChanged();
    void popularityFilterChanged();
    void colorListFilterChanged();
    void pinnedIdsChanged();

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column, Qt::SortOrder order) const override;

private:
    ColorType m_colorTypeFilter {};
    float m_popularityFilter = 0.f;
    QVector<const Color *> m_colorListFilter;
    QSet<uint> m_pinnedColorIds;

    friend class Core;
};


class CategoryModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const BrickLink::ItemType *filterItemType READ filterItemType WRITE setFilterItemType NOTIFY isFilteredChanged FINAL)
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged FINAL)

public:
    CategoryModel(QObject *parent = nullptr);

    static const Category *AllCategories;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
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

    void pinId(uint categoryId, bool down = true);
    void setPinnedIds(const QSet<uint> &colorIds);
    QSet<uint> pinnedIds() const;

signals:
    void pinnedIdsChanged();

protected:
    int pointerCount() const override;
    const void *pointerAt(int index) const override;
    int pointerIndexOf(const void *pointer) const override;

    bool filterAccepts(const void *pointer) const override;
    bool lessThan(const void *pointer1, const void *pointer2, int column, Qt::SortOrder order) const override;

private:
    const ItemType *m_itemtype_filter = nullptr;
    bool m_inv_filter = false;
    QSet<uint> m_pinnedCategoryIds;

    friend class Core;
};


class ItemTypeModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged FINAL)

public:
    ItemTypeModel(QObject *parent = nullptr);

    static const ItemType *AllItemTypes;

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
    bool lessThan(const void *pointer1, const void *pointer2, int column, Qt::SortOrder order) const override;

private:
    bool m_inv_filter = false;

    friend class Core;
};


class ItemModel : public StaticPointerModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const BrickLink::ItemType *filterItemType READ filterItemType WRITE setFilterItemType NOTIFY isFilteredChanged FINAL)
    Q_PROPERTY(const BrickLink::Category *filterCategory READ filterCategory WRITE setFilterCategory NOTIFY isFilteredChanged FINAL)
    Q_PROPERTY(const BrickLink::Color *filterColor READ filterColor WRITE setFilterColor NOTIFY isFilteredChanged FINAL)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY isFilteredChanged FINAL)
    Q_PROPERTY(bool filterWithoutInventory READ filterWithoutInventory WRITE setFilterWithoutInventory NOTIFY isFilteredChanged FINAL)

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
    bool lessThan(const void *pointer1, const void *pointer2, int column, Qt::SortOrder order) const override;

private:
    const ItemType *m_itemtype_filter = nullptr;
    const Category *m_category_filter = nullptr;
    const Color *   m_color_filter = nullptr;
    QString         m_text_filter;

    struct FilterTerm {
        explicit FilterTerm(bool negate, const QString &text) : m_negate(negate), m_text(text) { }

        bool m_negate = false;
        QString m_text;
    };

    QVector<FilterTerm> m_filter_terms;
    QByteArrayList  m_filter_ids;
    bool            m_filter_ids_optional = false;
    bool            m_inv_filter = false;
    int             m_year_min_filter = 0;
    int             m_year_max_filter = 0;

    friend class Core;
};


class InventoryModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(bool hasSections READ hasSections CONSTANT FINAL)

public:
    enum class Mode { AppearsIn, ConsistsOf, CanBuild, Relationships };
    Q_ENUM(Mode)

    struct SimpleLot
    {
        SimpleLot() = default;
        explicit SimpleLot(const BrickLink::Item *item, const BrickLink::Color *color,
                           int quantity = 0)
            : m_item(item), m_color(color), m_quantity(quantity)
        { }

        bool operator==(const SimpleLot &other) const = default;

        const BrickLink::Item *m_item = nullptr;
        const BrickLink::Color *m_color = nullptr;
        int m_quantity = 0;
    };

    enum Columns {
        PictureColumn,
        QuantityColumn,
        ColorColumn,
        ItemIdColumn,
        ItemNameColumn,
        ColumnCount,
    };

    InventoryModel(Mode mode, const QVector<SimpleLot> &simpleLots, QObject *parent);

    int count() const;
    Q_SIGNAL void countChanged(int newCount);
    bool hasSections() const;
    Mode mode() const;

    using QSortFilterProxyModel::index;
    QModelIndex index(const AppearsInItem *const_ai) const;

    void sort(int column, Qt::SortOrder order) override;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

} // namespace BrickLink
