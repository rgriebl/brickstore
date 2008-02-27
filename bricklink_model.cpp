/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <QFontMetrics>
#include <QApplication>
#include <QPixmap>
#include <QImage>

#include "bricklink.h"


/////////////////////////////////////////////////////////////
// COLORMODEL
/////////////////////////////////////////////////////////////

BrickLink::ColorModel::ColorModel()
{
    m_sorted = Qt::AscendingOrder;
    m_filter = 0;

    rebuildColorList();
}

QModelIndex BrickLink::ColorModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Color *>(m_colors.at(row)));
    return QModelIndex();
}

const BrickLink::Color *BrickLink::ColorModel::color(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const BrickLink::Color *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::ColorModel::index(const BrickLink::Color *color) const
{
    return color ? createIndex(m_colors.indexOf(color), 0, const_cast<Color *>(color)) : QModelIndex();
}

int BrickLink::ColorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_colors.count();
}

int BrickLink::ColorModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::ColorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !color(index))
        return QVariant();

    QVariant res;
    const Color *c = color(index);

    if (role == Qt:: DisplayRole) {
        res = c->name();
    }
    else if (role == Qt::DecorationRole) {
        QFontMetrics fm = QApplication::fontMetrics();
        if (const QPixmap *pix = BrickLink::core()->colorImage(c, fm.height(), fm.height()))
            res = *pix;
    }
    else if (role == Qt::ToolTipRole) {
        res = QString("<table width=\"100%\" border=\"0\" bgcolor=\"%3\"><tr><td><br><br></td></tr></table><br />%1: %2").arg(tr("RGB"), c->color().name(), c->color().name());
    }
    else if (role == ColorPointerRole) {
        res = c;
    }
    return res;
}

QVariant BrickLink::ColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color");
    return QVariant();
}

void BrickLink::ColorModel::sort(int column, Qt::SortOrder so)
{
    if (column == 0) {
        m_sorted = so;
        emit layoutAboutToBeChanged();
        qStableSort(m_colors.begin(), m_colors.end(), Compare(so == Qt::AscendingOrder));
        emit layoutChanged();
    }
}

BrickLink::ColorModel::Compare::Compare(bool asc) : m_asc(asc) { }

bool BrickLink::ColorModel::Compare::operator()(const Color *c1, const Color *c2)
{
    if (!m_asc) {
        return (qstrcmp(c1->name(), c2->name()) < 0);
    }
    else {
        int lh, rh, ls, rs, lv, rv, d;

        c1->color().getHsv(&lh, &ls, &lv);
        c2->color().getHsv(&rh, &rs, &rv);

        if (lh != rh)
            d = lh - rh;
        else if (ls != rs)
            d = ls - rs;
        else
            d = lv - rv;

        return d < 0;
    }
}

void BrickLink::ColorModel::rebuildColorList()
{
    if (!m_filter || m_filter->hasColors()) {
        m_colors = BrickLink::core()->colors().values();
        qStableSort(m_colors.begin(), m_colors.end(), Compare(m_sorted == Qt::AscendingOrder));
    }
    else
        m_colors.clear();
}

void BrickLink::ColorModel::setItemTypeFilter(const ItemType *it)
{
    if (it == m_filter)
        return;

    emit layoutAboutToBeChanged();

    m_filter = it;
    rebuildColorList();

    emit layoutChanged();
}

void BrickLink::ColorModel::clearItemTypeFilter()
{
    setItemTypeFilter(0);
}




/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::CategoryModel::CategoryModel(Features f)
{
    m_sorted = Qt::AscendingOrder;
    m_filter = 0;
    m_hasall = (f == IncludeAllCategoriesItem);

    rebuildCategoryList();
}

QModelIndex BrickLink::CategoryModel::index(int row, int column, const QModelIndex &parent) const
{
    bool isall = (row == 0) && m_hasall;

    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Category *>(isall ? AllCategories : m_categories.at(row - (m_hasall ? 1 : 0))));
    return QModelIndex();
}

const BrickLink::Category *BrickLink::CategoryModel::category(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Category *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::CategoryModel::index(const Category *category) const
{
    bool isall = (category == AllCategories) && m_hasall;

    return category ? createIndex(isall ? 0 : m_categories.indexOf(category) + (m_hasall ? 1 : 0), 0, const_cast<Category *>(category)) : QModelIndex();
}

int BrickLink::CategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_categories.count() + (m_hasall ? 1 : 0);
}

int BrickLink::CategoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::CategoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !category(index))
        return QVariant();

    QVariant res;
    const Category *c = category(index);

    if (role == Qt::DisplayRole) {
        res = c != AllCategories ? c->name() : QString("[%1]").arg(tr("All Items"));
    }
    else if (role == CategoryPointerRole) {
        res = c;
    }
    return res;
}

QVariant BrickLink::CategoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Category");
    return QVariant();
}

void BrickLink::CategoryModel::sort(int column, Qt::SortOrder so)
{
    if (column == 0) {
        emit layoutAboutToBeChanged();
        m_sorted = so;
        qStableSort(m_categories.begin(), m_categories.end(), Compare(m_sorted == Qt::AscendingOrder));
        emit layoutChanged();
    }
}

BrickLink::CategoryModel::Compare::Compare(bool asc) : m_asc(asc) { }

bool BrickLink::CategoryModel::Compare::operator()(const Category *c1, const Category *c2)
{
    bool b;

    if (c1 == AllCategories)
        b = true;
    else if (c2 == AllCategories)
        b = false;
    else
        b = (qstrcmp(c1->name(), c2->name()) >= 0);

    return m_asc ^ b;
}

void BrickLink::CategoryModel::rebuildCategoryList()
{
    if (!m_filter) {
        m_categories = BrickLink::core()->categories().values();
    }
    else {
        m_categories.clear();
        for (const Category **cp = m_filter->categories(); *cp; cp++)
            m_categories << *cp;
    }
    qStableSort(m_categories.begin(), m_categories.end(), Compare(m_sorted == Qt::AscendingOrder));
}

void BrickLink::CategoryModel::setItemTypeFilter(const ItemType *it)
{
    if (it == m_filter)
        return;

    emit layoutAboutToBeChanged();

    m_filter = it;
    rebuildCategoryList();

    emit layoutChanged();
}

void BrickLink::CategoryModel::clearItemTypeFilter()
{
    setItemTypeFilter(0);
}




/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemTypeModel::ItemTypeModel(Features)
{
    m_inv_filter = false;
    m_sorted = Qt::AscendingOrder;

    rebuildItemList();
}

QModelIndex BrickLink::ItemTypeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<ItemType *>(m_itemtypes.at(row)));
    return QModelIndex();
}

const BrickLink::ItemType *BrickLink::ItemTypeModel::itemType(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const ItemType *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::ItemTypeModel::index(const ItemType *itemtype) const
{
    return itemtype ? createIndex(m_itemtypes.indexOf(itemtype), 0, const_cast<ItemType *>(itemtype)) : QModelIndex();
}

int BrickLink::ItemTypeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_itemtypes.count();
}

int BrickLink::ItemTypeModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::ItemTypeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !itemType(index))
        return QVariant();

    QVariant res;
    const ItemType *i = itemType(index);

    if (role == Qt::DisplayRole) {
        res = i->name();
    }
    else if (role == ItemTypePointerRole) {
        res = i;
    }
    return res;
}

QVariant BrickLink::ItemTypeModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}

void BrickLink::ItemTypeModel::setExcludeWithoutInventoryFilter(bool on)
{
    if (on == m_inv_filter)
        return;

    emit layoutAboutToBeChanged();

    m_inv_filter = on;
    rebuildItemList();

    emit layoutChanged();
}

void BrickLink::ItemTypeModel::sort(int column, Qt::SortOrder so)
{
    if (column == 0) {
        emit layoutAboutToBeChanged();
        Compare cmp(so == Qt::AscendingOrder);
        m_sorted = so;
        qStableSort(m_itemtypes.begin(), m_itemtypes.end(), cmp);
        emit layoutChanged();
    }
}

void BrickLink::ItemTypeModel::rebuildItemList()
{
    if (!m_inv_filter) {
        m_itemtypes = BrickLink::core()->itemTypes().values();
    }
    else {
        m_itemtypes.clear();
        foreach (const ItemType *itemtype, BrickLink::core()->itemTypes().values()) {
            if (m_inv_filter && !itemtype->hasInventories())
                continue;

            m_itemtypes << itemtype;
        }
    }
    qStableSort(m_itemtypes.begin(), m_itemtypes.end(), Compare(m_sorted == Qt::AscendingOrder));
}

BrickLink::ItemTypeModel::Compare::Compare(bool asc) : m_asc(asc) { }

bool BrickLink::ItemTypeModel::Compare::operator()(const ItemType *c1, const ItemType *c2)
{
    bool b = (qstrcmp(c1->name(), c2->name()) >= 0);
    return m_asc ^ b;
}




/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemModel::ItemModel(Features)
{
    m_cat_filter = 0;
    m_type_filter = 0;
    m_inv_filter = false;
    m_sorted = Qt::AscendingOrder;
    m_sortcol = 1;
    m_items.reserve(BrickLink::core()->items().count());

    connect(BrickLink::core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));

    rebuildItemList();
}

QModelIndex BrickLink::ItemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Item *>(m_items.at(row)));
    return QModelIndex();
}

const BrickLink::Item *BrickLink::ItemModel::item(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Item *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::ItemModel::index(const Item *item) const
{
    return item ? createIndex(m_items.indexOf(item), 0, const_cast<Item *>(item)) : QModelIndex();
}

int BrickLink::ItemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.count();
}

int BrickLink::ItemModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant BrickLink::ItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !item(index))
        return QVariant();

    QVariant res;
    const Item *i = item(index);

    if (role == Qt::DisplayRole) {
        switch(index.column()) {
        case 1: res = QString::fromLatin1(i->id()); break;
        case 2: res = QString::fromLatin1(i->name()); break;
        }
    }
    else if (role == Qt::DecorationRole) {
        switch(index.column()) {
        case 0: {
            Picture *pic = BrickLink::core()->picture(i, i->defaultColor());

            if (pic && pic->valid()) {
                return pic->image();
            }
            else {
                QSize s = BrickLink::core()->pictureSize(i->itemType());
                QImage img(s, QImage::Format_Mono);
                img.fill(Qt::white);
                return img;
            }
            break;
        }
        }
    }
    else if (role == Qt::ToolTipRole) {
        switch(index.column()) {
        case 0:  res = QString::fromLatin1(i->name()); break;
        }
    }
    else if (role == ItemPointerRole) {
        res = i;
    }
    return res;
}

QVariant BrickLink::ItemModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch(section) {
        case 1: return tr("Part #");
        case 2: return tr("Description");
        }
    }
    return QVariant();
}

void BrickLink::ItemModel::sort(int column, Qt::SortOrder so)
{
    if (column != 0) {
        emit layoutAboutToBeChanged();
        Compare cmp(so == Qt::AscendingOrder, column == 2);
        m_sorted = so;
        m_sortcol = column;
        qStableSort(m_items.begin(), m_items.end(), cmp);
        emit layoutChanged();
    }
}

BrickLink::ItemModel::Compare::Compare(bool asc, bool byname) : m_asc(asc), m_byname(byname) { }

bool BrickLink::ItemModel::Compare::operator()(const Item *c1, const Item *c2)
{
    bool b = (qstrcmp(m_byname ? c1->name() : c1->id(), m_byname ? c2->name() : c2->id()) >= 0);
    return m_asc ^ b;
}

void BrickLink::ItemModel::rebuildItemList()
{
    if (!m_type_filter && !m_cat_filter && m_text_filter.isEmpty() && !m_inv_filter) {
        m_items = BrickLink::core()->items();
    }
    else {
        m_items.clear();
        foreach (const Item *item, BrickLink::core()->items()) {
            if (m_type_filter && item->itemType() != m_type_filter)
                continue;
            if (m_cat_filter && !item->hasCategory(m_cat_filter))
                continue;
            if (m_inv_filter && !item->hasInventory())
                continue;
            if (!m_text_filter.isEmpty() && m_text_filter.isValid()) {
                if ((m_text_filter.indexIn(QLatin1String(item->id())) < 0) &&
                    (m_text_filter.indexIn(QLatin1String(item->name())) < 0))
                    continue;
            }

            m_items << item;
        }
    }
    qStableSort(m_items.begin(), m_items.end(), Compare(m_sorted == Qt::AscendingOrder, m_sortcol == 1));
}

void BrickLink::ItemModel::setItemTypeFilter(const ItemType *it)
{
    if (it == m_type_filter)
        return;

    emit layoutAboutToBeChanged();

    m_type_filter = it;
    rebuildItemList();

    emit layoutChanged();
}

void BrickLink::ItemModel::clearItemTypeFilter()
{
    setItemTypeFilter(0);
}

void BrickLink::ItemModel::setCategoryFilter(const Category *cat)
{
    if (cat == BrickLink::CategoryModel::AllCategories)
        cat = 0;

    if (cat == m_cat_filter)
        return;

    emit layoutAboutToBeChanged();

    m_cat_filter = cat;
    rebuildItemList();

    emit layoutChanged();
}

void BrickLink::ItemModel::clearCategoryFilter()
{
    setCategoryFilter(0);
}

void BrickLink::ItemModel::setTextFilter(const QRegExp &rx)
{
    if (rx == m_text_filter)
        return;

    emit layoutAboutToBeChanged();

    m_text_filter = rx;
    rebuildItemList();

    emit layoutChanged();
}

void BrickLink::ItemModel::clearTextFilter()
{
    setTextFilter(QRegExp());
}

void BrickLink::ItemModel::setExcludeWithoutInventoryFilter(bool on)
{
    if (on == m_inv_filter)
        return;

    emit layoutAboutToBeChanged();

    m_inv_filter = on;
    rebuildItemList();

    emit layoutChanged();
}

void BrickLink::ItemModel::pictureUpdated(BrickLink::Picture *pic)
{
    if (!pic || !pic->item() || pic->color() != pic->item()->defaultColor())
        return;

    QModelIndex idx = index(pic->item());
    if (idx.isValid())
        emit dataChanged(idx, idx);
}



/////////////////////////////////////////////////////////////
// APPEARSINMODEL
/////////////////////////////////////////////////////////////


BrickLink::AppearsInModel::AppearsInModel(const BrickLink::Item *item, const BrickLink::Color *color)
{
    m_item = item;
    m_color = color;

    if (item)
        m_appearsin = item->appearsIn(color);

   foreach(const BrickLink::Item::AppearsInColor &vec, m_appearsin) {
        foreach(const BrickLink::Item::AppearsInItem &item, vec)
            m_items.append(const_cast<BrickLink::Item::AppearsInItem *>(&item));
   }
}

BrickLink::AppearsInModel::~AppearsInModel()
{
}

QModelIndex BrickLink::AppearsInModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent) && !parent.isValid())
        return createIndex(row, column, m_items.at(row));
    return QModelIndex();
}

const BrickLink::Item::AppearsInItem *BrickLink::AppearsInModel::appearsIn(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<const BrickLink::Item::AppearsInItem *>(idx.internalPointer()) : 0;
}

QModelIndex BrickLink::AppearsInModel::index(const BrickLink::Item::AppearsInItem *const_ai) const
{
    BrickLink::Item::AppearsInItem *ai = const_cast<BrickLink::Item::AppearsInItem *>(const_ai);

    return ai ? createIndex(m_items.indexOf(ai), 0, ai) : QModelIndex();
}

int BrickLink::AppearsInModel::rowCount(const QModelIndex & /*parent*/) const
{ return m_items.size(); }

int BrickLink::AppearsInModel::columnCount(const QModelIndex & /*parent*/) const
{ return 3; }

QVariant BrickLink::AppearsInModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    const BrickLink::Item::AppearsInItem *appears = appearsIn(index);
    int col = index.column();

    if (!appears)
        return res;

    switch (role) {
    case Qt::DisplayRole:
        switch (col) {
        case 0: res = QString::number(appears->first); break;
        case 1: res = appears->second->id(); break;
        case 2: res = appears->second->name(); break;
        }
        break;
    }

    return res;
}

QVariant BrickLink::AppearsInModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch (section) {
        case 0: return tr("#");
        case 1: return tr("Set");
        case 2: return tr("Name");
        }
    }
    return QVariant();
}

