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

#if defined( MODELTEST )
#include <modeltest.h>
#define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
#else
#define MODELTEST_ATTACH(x)   ;
#endif


/////////////////////////////////////////////////////////////
// COLORMODEL
/////////////////////////////////////////////////////////////

BrickLink::ColorModel::ColorModel(QObject *parent)
    : QAbstractListModel(parent)
{
    MODELTEST_ATTACH(this)

    m_colors = core()->colors().values();
}

QModelIndex BrickLink::ColorModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Color *>(m_colors.at(row)));
    return QModelIndex();
}

const BrickLink::Color *BrickLink::ColorModel::color(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Color *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::ColorModel::index(const Color *color) const
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
        if (const QPixmap *pix = core()->colorImage(c, fm.height(), fm.height()))
            res = *pix;
    }
    else if (role == Qt::ToolTipRole) {
        res = QString("<table width=\"100%\" border=\"0\" bgcolor=\"%3\"><tr><td><br><br></td></tr></table><br />%1: %2").arg(tr("RGB"), c->color().name(), c->color().name()); //%
    }
    else if (role == ColorPointerRole) {
        res.setValue(c);
    }
    return res;
}

QVariant BrickLink::ColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color");
    return QVariant();
}

BrickLink::ColorProxyModel::ColorProxyModel(ColorModel *attach)
    : QSortFilterProxyModel(attach->QObject::parent()), m_itemtype_filter(0), m_order(Qt::AscendingOrder)
{
    setSourceModel(attach);
}

void BrickLink::ColorProxyModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::ColorProxyModel::sort(int column, Qt::SortOrder order)
{
    m_order = order;
    QSortFilterProxyModel::sort(column, order);
}

bool BrickLink::ColorProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Color* via data()/QVariant marshalling
    const ColorModel *cm = static_cast<const ColorModel *>(sourceModel());
    const Color *c1 = cm->color(left);
    const Color *c2 = cm->color(right);

    if (!c1)
        return true;
    else if (!c2)
        return false;
    else {
        if (m_order == Qt::AscendingOrder) {
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
}

bool BrickLink::ColorProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_itemtype_filter && !m_itemtype_filter->hasColors()) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Color* via data()/QVariant marshalling
        const ColorModel *cm = static_cast<const ColorModel *>(sourceModel());
        const Color *c = cm->color(cm->index(source_row, 0));

        return (c && c->id() == 0);
    }
    else
        return true;
}


const BrickLink::Color *BrickLink::ColorProxyModel::color(const QModelIndex &index) const
{
    ColorModel *cm = static_cast<ColorModel *>(sourceModel());

    if (cm && index.isValid())
        return cm->color(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ColorProxyModel::index(const Color *color) const
{
    ColorModel *cm = static_cast<ColorModel *>(sourceModel());

    if (cm && color)
        return mapFromSource(cm->index(color));
    return QModelIndex();
}



/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::CategoryModel::CategoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    MODELTEST_ATTACH(this)

    m_categories = core()->categories().values();
}

QModelIndex BrickLink::CategoryModel::index(int row, int column, const QModelIndex &parent) const
{
    bool isall = (row == 0);

    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Category *>(isall ? AllCategories : m_categories.at(row - 1)));
    return QModelIndex();
}

const BrickLink::Category *BrickLink::CategoryModel::category(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Category *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::CategoryModel::index(const Category *category) const
{
    bool isall = (category == AllCategories);

    return category ? createIndex(isall ? 0 : m_categories.indexOf(category) + 1, 0, const_cast<Category *>(category)) : QModelIndex();
}

int BrickLink::CategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_categories.count() + 1;
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
        res.setValue(c);
    }
    return res;
}

QVariant BrickLink::CategoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Category");
    return QVariant();
}

BrickLink::CategoryProxyModel::CategoryProxyModel(CategoryModel *attach)
    : QSortFilterProxyModel(attach->QObject::parent()), m_itemtype_filter(0), m_all_filter(false)
{
    setSourceModel(attach);
}

void BrickLink::CategoryProxyModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::CategoryProxyModel::setFilterAllCategories(bool b)
{
    if (b == m_all_filter)
        return;

    m_all_filter = b;
    invalidateFilter();
}

bool BrickLink::CategoryProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Categorsety* via data()/QVariant marshalling
    const CategoryModel *cm = static_cast<const CategoryModel *>(left.model());
    const Category *c1 = cm->category(left);
    const Category *c2 = cm->category(right);

    if (!c1 || c1 == CategoryModel::AllCategories)
        return true;
    else if (!c2 || c2 == CategoryModel::AllCategories)
        return false;
    else
        return (qstrcmp(c1->name(), c2->name()) < 0);
}

bool BrickLink::CategoryProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_itemtype_filter || m_all_filter) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Category* via data()/QVariant marshalling
        const CategoryModel *cm = static_cast<const CategoryModel *>(sourceModel());
        const Category *c = cm->category(cm->index(source_row, 0));

        if (!c)
            return false;
        else if (c == CategoryModel::AllCategories)
            return !m_all_filter;
        else if (m_itemtype_filter) {
            for (const Category **cp = m_itemtype_filter->categories(); *cp; cp++) {
                if (c == *cp)
                    return true;
            }
            return false;
        }
    }
    return true;
}

const BrickLink::Category *BrickLink::CategoryProxyModel::category(const QModelIndex &index) const
{
    CategoryModel *cm = static_cast<CategoryModel *>(sourceModel());

    if (cm && index.isValid())
        return cm->category(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::CategoryProxyModel::index(const Category *category) const
{
    CategoryModel *cm = static_cast<CategoryModel *>(sourceModel());

    if (cm && category)
        return mapFromSource(cm->index(category));
    return QModelIndex();
}









/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemTypeModel::ItemTypeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    MODELTEST_ATTACH(this)

    m_itemtypes = core()->itemTypes().values();
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
        res.setValue(i);
    }
    return res;
}

QVariant BrickLink::ItemTypeModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}



BrickLink::ItemTypeProxyModel::ItemTypeProxyModel(ItemTypeModel *attach)
    : QSortFilterProxyModel(attach->QObject::parent()), m_inv_filter(false)
{
    setSourceModel(attach);
}

void BrickLink::ItemTypeProxyModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    invalidateFilter();
}

bool BrickLink::ItemTypeProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const ItemTypeModel *im = static_cast<const ItemTypeModel *>(sourceModel());
    const ItemType *i1 = im->itemType(left);
    const ItemType *i2 = im->itemType(right);

    if (!i1)
        return true;
    else if (!i2)
        return false;
    else
        return (qstrcmp(i1->name(), i2->name()) < 0);
}

bool BrickLink::ItemTypeProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_inv_filter) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Category* via data()/QVariant marshalling
        const ItemTypeModel *im = static_cast<const ItemTypeModel *>(sourceModel());
        const ItemType *i = im->itemType(im->index(source_row, 0));

        if (!i || !i->hasInventories())
            return false;
    }
    return true;
}

const BrickLink::ItemType *BrickLink::ItemTypeProxyModel::itemType(const QModelIndex &index) const
{
    ItemTypeModel *im = static_cast<ItemTypeModel *>(sourceModel());

    if (im && index.isValid())
        return im->itemType(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ItemTypeProxyModel::index(const ItemType *itemtype) const
{
    ItemTypeModel *im = static_cast<ItemTypeModel *>(sourceModel());

    if (im && itemtype)
        return mapFromSource(im->index(itemtype));
    return QModelIndex();
}








/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////

BrickLink::ItemModel::ItemModel(QObject *parent)
    : QAbstractTableModel(parent), m_items(core()->items())
{
    MODELTEST_ATTACH(this)

    connect(core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));
}

QModelIndex BrickLink::ItemModel::index(int row, int column, const QModelIndex &parent) const
{
    // if (hasIndex(row, column, parent)) // too expensive
    if (!parent.isValid() && row >= 0 && column >= 0 && row < m_items.count() && column < 3)
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
            Picture *pic = core()->picture(i, i->defaultColor());

            if (pic && pic->valid()) {
                return pic->image();
            }
            else {
                QSize s = core()->pictureSize(i->itemType());
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
        res.setValue(i);
    }
    else if (role == ItemTypePointerRole) {
        res.setValue(i->itemType());
    }
    else if (role == CategoryPointerRole) {
        res.setValue(i->category());
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

void BrickLink::ItemModel::pictureUpdated(Picture *pic)
{
    if (!pic || !pic->item() || pic->color() != pic->item()->defaultColor())
        return;

    QModelIndex idx = index(pic->item());
    if (idx.isValid())
        emit dataChanged(idx, idx);
}




BrickLink::ItemProxyModel::ItemProxyModel(ItemModel *attach)
    : QSortFilterProxyModel(attach->QObject::parent()), m_itemtype_filter(0), m_category_filter(0), m_inv_filter(false)
{
    setSourceModel(attach);
}

void BrickLink::ItemProxyModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::ItemProxyModel::setFilterCategory(const Category *cat)
{
    if (cat == m_category_filter)
        return;
    m_category_filter = cat;
    invalidateFilter();
}

void BrickLink::ItemProxyModel::setFilterText(const QString &str)
{
    if (str == m_text_filter.pattern())
        return;
    m_text_filter = QRegExp(str, Qt::CaseInsensitive, QRegExp::Wildcard);
    invalidateFilter();
}

void BrickLink::ItemProxyModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    invalidateFilter();
}

const BrickLink::Item *BrickLink::ItemProxyModel::item(const QModelIndex &index) const
{
    ItemModel *im = static_cast<ItemModel *>(sourceModel());

    if (im && index.isValid())
        return im->item(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ItemProxyModel::index(const Item *item) const
{
    ItemModel *im = static_cast<ItemModel *>(sourceModel());

    if (im && item)
        return mapFromSource(im->index(item));
    return QModelIndex();
}

bool BrickLink::ItemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const ItemModel *im = static_cast<const ItemModel *>(sourceModel());
    const Item *i1 = im->item(left);
    const Item *i2 = im->item(right);

    bool byname = (left.column() == 2);

    if (!i1)
        return true;
    else if (!i2)
        return false;
    else
        return (qstrcmp(byname ? i1->name() : i1->id(), byname ? i2->name() : i2->id()) < 0);
}

bool BrickLink::ItemProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const ItemModel *cm = static_cast<const ItemModel *>(sourceModel());
    const Item *item = cm->itemAtRow(source_row);

    if (source_row == 0)
        qWarning("FILTERING ROW 0");

    if (!item)
        return false;
    else if (m_itemtype_filter && item->itemType() != m_itemtype_filter)
        return false;
    else if (m_category_filter && (m_category_filter != BrickLink::CategoryModel::AllCategories) && !item->hasCategory(m_category_filter))
        return false;
    else if (m_inv_filter && !item->hasInventory())
        return false;
    else {
        QRegExp rx = filterRegExp();
        if (!rx.isEmpty())
           return ((rx.indexIn(QLatin1String(item->id())) >= 0) ||
                   (rx.indexIn(QLatin1String(item->name())) >= 0));
    }
    return true;
}


/////////////////////////////////////////////////////////////
// APPEARSINMODEL
/////////////////////////////////////////////////////////////


BrickLink::AppearsInModel::AppearsInModel(const Item *item, const Color *color)
{
    MODELTEST_ATTACH(this)

    m_item = item;
    m_color = color;

    if (item)
        m_appearsin = item->appearsIn(color);

   foreach(const Item::AppearsInColor &vec, m_appearsin) {
        foreach(const Item::AppearsInItem &item, vec)
            m_items.append(const_cast<Item::AppearsInItem *>(&item));
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
    return idx.isValid() ? static_cast<const Item::AppearsInItem *>(idx.internalPointer()) : 0;
}

QModelIndex BrickLink::AppearsInModel::index(const Item::AppearsInItem *const_ai) const
{
    Item::AppearsInItem *ai = const_cast<Item::AppearsInItem *>(const_ai);

    return ai ? createIndex(m_items.indexOf(ai), 0, ai) : QModelIndex();
}

int BrickLink::AppearsInModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int BrickLink::AppearsInModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant BrickLink::AppearsInModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    const Item::AppearsInItem *appears = appearsIn(index);
    int col = index.column();

    if (!appears)
        return res;

    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: res = QString::number(appears->first); break;
        case 1: res = appears->second->id(); break;
        case 2: res = appears->second->name(); break;
        }
    }
    else if (role == BrickLink::AppearsInItemPointerRole) {
        res.setValue(appears);
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


BrickLink::AppearsInProxyModel::AppearsInProxyModel(AppearsInModel *attach)
    : QSortFilterProxyModel(attach->QObject::parent())
{
    setSourceModel(attach);
}

const BrickLink::Item::AppearsInItem *BrickLink::AppearsInProxyModel::appearsIn(const QModelIndex &index) const
{
    AppearsInModel *aim = static_cast<AppearsInModel *>(sourceModel());

    if (aim && index.isValid())
        return aim->appearsIn(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::AppearsInProxyModel::index(const Item::AppearsInItem *item) const
{
    AppearsInModel *aim = static_cast<AppearsInModel *>(sourceModel());

    if (aim && item)
        return mapFromSource(aim->index(item));
    return QModelIndex();
}

bool BrickLink::AppearsInProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const AppearsInModel *aim = static_cast<const AppearsInModel *>(sourceModel());
    const Item::AppearsInItem *ai1 = aim->appearsIn(left);
    const Item::AppearsInItem *ai2 = aim->appearsIn(right);

    if (!ai1)
        return true;
    else if (!ai2)
        return false;
    else {
        switch (left.column()) {
        default:
        case  0: return ai1->first < ai2->first;
        case  1: return (qstrcmp(ai1->second->id(), ai2->second->id()) < 0);
        case  2: return (qstrcmp(ai1->second->name(), ai2->second->name()) < 0 );
        }
    }
}
