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
#include <QIcon>

#include "bricklink.h"

#if defined(MODELTEST)
#  include "modeltest.h"
#  define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif


/////////////////////////////////////////////////////////////
// COLORMODEL
/////////////////////////////////////////////////////////////

BrickLink::InternalColorModel::InternalColorModel()
    : QAbstractListModel(core())
{
    MODELTEST_ATTACH(this)
}

QModelIndex BrickLink::InternalColorModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Color *>(*(core()->colors().begin() + row)));
    return QModelIndex();
}

const BrickLink::Color *BrickLink::InternalColorModel::color(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Color *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalColorModel::index(const Color *color) const
{
    int row = 0;
    for (QMap<int, const Color *>::const_iterator it = core()->colors().begin(); it != core()->colors().end(); ++it, ++row) {
        if (*it == color)
            break;
    }

    return (color && row < rowCount()) ? createIndex(row, 0, const_cast<Color *>(color)) : QModelIndex();
}

int BrickLink::InternalColorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : core()->colors().count();
}

int BrickLink::InternalColorModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::InternalColorModel::data(const QModelIndex &index, int role) const
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
        if (const QPixmap *pix = core()->colorImage(c, fm.height(), fm.height())) {
            QIcon ico;
            ico.addPixmap(*pix, QIcon::Normal, QIcon::Off);
            ico.addPixmap(*pix, QIcon::Selected, QIcon::Off);
            res.setValue(ico);
        }
    }
    else if (role == Qt::ToolTipRole) {
        res = QString("<table width=\"100%\" border=\"0\" bgcolor=\"%3\"><tr><td><br><br></td></tr></table><br />%1: %2").arg(tr("RGB"), c->color().name(), c->color().name()); //%
    }
    else if (role == ColorPointerRole) {
        res.setValue(c);
    }
    return res;
}

QVariant BrickLink::InternalColorModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Color");
    return QVariant();
}

BrickLink::ColorModel::ColorModel(QObject *parent)
    : QSortFilterProxyModel(parent), m_itemtype_filter(0), m_order(Qt::AscendingOrder)
{
    setSourceModel(core()->colorModel());
}

void BrickLink::ColorModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::ColorModel::sort(int column, Qt::SortOrder order)
{
    m_order = order;
    QSortFilterProxyModel::sort(column, order);
}

bool BrickLink::ColorModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Color* via data()/QVariant marshalling
    const InternalColorModel *cm = static_cast<const InternalColorModel *>(sourceModel());
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

bool BrickLink::ColorModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_itemtype_filter && !m_itemtype_filter->hasColors()) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Color* via data()/QVariant marshalling
        const InternalColorModel *cm = static_cast<const InternalColorModel *>(sourceModel());
        const Color *c = cm->color(cm->index(source_row, 0));

        return (c && c->id() == 0);
    }
    else
        return true;
}


const BrickLink::Color *BrickLink::ColorModel::color(const QModelIndex &index) const
{
    InternalColorModel *cm = static_cast<InternalColorModel *>(sourceModel());

    if (cm && index.isValid())
        return cm->color(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ColorModel::index(const Color *color) const
{
    InternalColorModel *cm = static_cast<InternalColorModel *>(sourceModel());

    if (cm && color)
        return mapFromSource(cm->index(color));
    return QModelIndex();
}



/////////////////////////////////////////////////////////////
// CATEGORYMODEL
/////////////////////////////////////////////////////////////

// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::InternalCategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::InternalCategoryModel::InternalCategoryModel()
    : QAbstractListModel(core())
{
    MODELTEST_ATTACH(this)
}

QModelIndex BrickLink::InternalCategoryModel::index(int row, int column, const QModelIndex &parent) const
{
    bool isall = (row == 0);

    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Category *>(isall ? AllCategories : *(core()->categories().begin() + (row - 1))));
    return QModelIndex();
}

const BrickLink::Category *BrickLink::InternalCategoryModel::category(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Category *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalCategoryModel::index(const Category *category) const
{
    int row;

    if (category == AllCategories) {
        row = 0;
    } else {
        row = 1;
        for (QMap<int, const Category *>::const_iterator it = core()->categories().begin(); it != core()->categories().end(); ++it, ++row) {
            if (*it == category)
                break;
        }
    }

    return (category && row < rowCount()) ? createIndex(row, 0, const_cast<Category *>(category)) : QModelIndex();
}

int BrickLink::InternalCategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : core()->categories().count() + 1;
}

int BrickLink::InternalCategoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::InternalCategoryModel::data(const QModelIndex &index, int role) const
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

QVariant BrickLink::InternalCategoryModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Category");
    return QVariant();
}

BrickLink::CategoryModel::CategoryModel(QObject *parent)
    : QSortFilterProxyModel(parent), m_itemtype_filter(0), m_all_filter(false)
{
    setSourceModel(core()->categoryModel());
}

void BrickLink::CategoryModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;
    m_itemtype_filter = it;
    invalidateFilter();
}

void BrickLink::CategoryModel::setFilterAllCategories(bool b)
{
    if (b == m_all_filter)
        return;

    m_all_filter = b;
    invalidateFilter();
}

bool BrickLink::CategoryModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Categorsety* via data()/QVariant marshalling
    const InternalCategoryModel *cm = static_cast<const InternalCategoryModel *>(left.model());
    const Category *c1 = cm->category(left);
    const Category *c2 = cm->category(right);

    if (!c1 || c1 == InternalCategoryModel::AllCategories)
        return true;
    else if (!c2 || c2 == InternalCategoryModel::AllCategories)
        return false;
    else
        return (qstrcmp(c1->name(), c2->name()) < 0);
}

bool BrickLink::CategoryModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_itemtype_filter || m_all_filter) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Category* via data()/QVariant marshalling
        const InternalCategoryModel *cm = static_cast<const InternalCategoryModel *>(sourceModel());
        const Category *c = cm->category(cm->index(source_row, 0));

        if (!c)
            return false;
        else if (c == InternalCategoryModel::AllCategories)
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

const BrickLink::Category *BrickLink::CategoryModel::category(const QModelIndex &index) const
{
    InternalCategoryModel *cm = static_cast<InternalCategoryModel *>(sourceModel());

    if (cm && index.isValid())
        return cm->category(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::CategoryModel::index(const Category *category) const
{
    InternalCategoryModel *cm = static_cast<InternalCategoryModel *>(sourceModel());

    if (cm && category)
        return mapFromSource(cm->index(category));
    return QModelIndex();
}









/////////////////////////////////////////////////////////////
// ITEMTYPEMODEL
/////////////////////////////////////////////////////////////

BrickLink::InternalItemTypeModel::InternalItemTypeModel()
    : QAbstractListModel(core())
{
    MODELTEST_ATTACH(this)
}

QModelIndex BrickLink::InternalItemTypeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<ItemType *>(*(core()->itemTypes().begin() + row)));
    return QModelIndex();
}

const BrickLink::ItemType *BrickLink::InternalItemTypeModel::itemType(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const ItemType *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalItemTypeModel::index(const ItemType *itemtype) const
{
    int row = 0;
    for (QMap<int, const ItemType *>::const_iterator it = core()->itemTypes().begin(); it != core()->itemTypes().end(); ++it, ++row) {
        if (*it == itemtype)
            break;
    }

    return (itemtype && row < rowCount()) ? createIndex(row, 0, const_cast<ItemType *>(itemtype)) : QModelIndex();
}

int BrickLink::InternalItemTypeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : core()->itemTypes().count();
}

int BrickLink::InternalItemTypeModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BrickLink::InternalItemTypeModel::data(const QModelIndex &index, int role) const
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

QVariant BrickLink::InternalItemTypeModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}



BrickLink::ItemTypeModel::ItemTypeModel(QObject *parent)
    : QSortFilterProxyModel(parent), m_inv_filter(false)
{
    setSourceModel(core()->itemTypeModel());
}

void BrickLink::ItemTypeModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
    invalidateFilter();
}

bool BrickLink::ItemTypeModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const InternalItemTypeModel *im = static_cast<const InternalItemTypeModel *>(sourceModel());
    const ItemType *i1 = im->itemType(left);
    const ItemType *i2 = im->itemType(right);

    if (!i1)
        return true;
    else if (!i2)
        return false;
    else
        return (qstrcmp(i1->name(), i2->name()) < 0);
}

bool BrickLink::ItemTypeModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    if (m_inv_filter) {
        // the indexes are from the source model, so the internal pointers are valid
        // this is faster than fetching the Category* via data()/QVariant marshalling
        const InternalItemTypeModel *im = static_cast<const InternalItemTypeModel *>(sourceModel());
        const ItemType *i = im->itemType(im->index(source_row, 0));

        if (!i || !i->hasInventories())
            return false;
    }
    return true;
}

const BrickLink::ItemType *BrickLink::ItemTypeModel::itemType(const QModelIndex &index) const
{
    InternalItemTypeModel *im = static_cast<InternalItemTypeModel *>(sourceModel());

    if (im && index.isValid())
        return im->itemType(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ItemTypeModel::index(const ItemType *itemtype) const
{
    InternalItemTypeModel *im = static_cast<InternalItemTypeModel *>(sourceModel());

    if (im && itemtype)
        return mapFromSource(im->index(itemtype));
    return QModelIndex();
}








/////////////////////////////////////////////////////////////
// ITEMMODEL
/////////////////////////////////////////////////////////////

BrickLink::InternalItemModel::InternalItemModel()
    : QAbstractTableModel(core())
{
    MODELTEST_ATTACH(this)

    connect(core(), SIGNAL(pictureUpdated(BrickLink::Picture *)), this, SLOT(pictureUpdated(BrickLink::Picture *)));
}

QModelIndex BrickLink::InternalItemModel::index(int row, int column, const QModelIndex &parent) const
{
    // if (hasIndex(row, column, parent)) // too expensive
    if (!parent.isValid() && row >= 0 && column >= 0 && row < core()->items().count() && column < 3)
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Item *>(core()->items().at(row)));
    return QModelIndex();
}

const BrickLink::Item *BrickLink::InternalItemModel::item(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Item *>(index.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalItemModel::index(const Item *item) const
{
    return item ? createIndex(core()->items().indexOf(item), 0, const_cast<Item *>(item)) : QModelIndex();
}

int BrickLink::InternalItemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : core()->items().count();
}

int BrickLink::InternalItemModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant BrickLink::InternalItemModel::data(const QModelIndex &index, int role) const
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

QVariant BrickLink::InternalItemModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole)) {
        switch(section) {
        case 1: return tr("Part #");
        case 2: return tr("Description");
        }
    }
    return QVariant();
}

void BrickLink::InternalItemModel::pictureUpdated(Picture *pic)
{
    if (!pic || !pic->item() || pic->color() != pic->item()->defaultColor())
        return;

    QModelIndex idx = index(pic->item());
    if (idx.isValid())
        emit dataChanged(idx, idx);
}


BrickLink::ItemModel::ItemModel(QObject *parent)
    : QSortFilterProxyModel(parent), m_itemtype_filter(0), m_category_filter(0), m_inv_filter(false)
{
    m_filter_timer.setInterval(0);
    m_filter_timer.setSingleShot(true);
    connect(&m_filter_timer, SIGNAL(timeout()), this, SLOT(invalidateFilterSlot()));

    setSourceModel(core()->itemModel());
}

void BrickLink::ItemModel::setFilterItemType(const ItemType *it)
{
    if (it == m_itemtype_filter)
        return;

    m_itemtype_filter = it;
//    m_filter_timer.start();
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterCategory(const Category *cat)
{
    if (cat == m_category_filter)
        return;
    m_category_filter = cat;
//    m_filter_timer.start();
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterText(const QString &str)
{
    if (str == m_text_filter.pattern())
        return;
    m_text_filter = QRegExp(str, Qt::CaseInsensitive, QRegExp::Wildcard);
//    m_filter_timer.start();
    invalidateFilter();
}

void BrickLink::ItemModel::setFilterWithoutInventory(bool b)
{
    if (b == m_inv_filter)
        return;

    m_inv_filter = b;
//    m_filter_timer.start();
    invalidateFilter();
}

void BrickLink::ItemModel::invalidateFilterSlot()
{
    invalidateFilter();
}

const BrickLink::Item *BrickLink::ItemModel::item(const QModelIndex &index) const
{
    InternalItemModel *im = static_cast<InternalItemModel *>(sourceModel());

    if (im && index.isValid())
        return im->item(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::ItemModel::index(const Item *item) const
{
    InternalItemModel *im = static_cast<InternalItemModel *>(sourceModel());

    if (im && item)
        return mapFromSource(im->index(item));
    return QModelIndex();
}

bool BrickLink::ItemModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const InternalItemModel *im = static_cast<const InternalItemModel *>(sourceModel());
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

bool BrickLink::ItemModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (source_parent.isValid())
        return false;

    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const Item *item = core()->items().at(source_row);

    if (source_row == 0)
        qWarning("FILTERING ROW 0");

    if (!item)
        return false;
    else if (m_itemtype_filter && item->itemType() != m_itemtype_filter)
        return false;
    else if (m_category_filter && (m_category_filter != BrickLink::InternalCategoryModel::AllCategories) && !item->hasCategory(m_category_filter))
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


BrickLink::InternalAppearsInModel::InternalAppearsInModel(const Item *item, const Color *color, QObject *parent)
    : QAbstractTableModel(parent), m_item(item), m_color(color)
{
    MODELTEST_ATTACH(this)

    if (item)
        m_appearsin = item->appearsIn(color);

   foreach(const AppearsInColor &vec, m_appearsin) {
        foreach(const AppearsInItem &item, vec)
            m_items.append(const_cast<AppearsInItem *>(&item));
   }
}

BrickLink::InternalAppearsInModel::InternalAppearsInModel(const InvItemList &list, QObject *parent)
    : QAbstractTableModel(parent), m_item(0), m_color(0)
{
    MODELTEST_ATTACH(this)

    if (!list.isEmpty()) {
        QMap<const Item *, int> unique;
        bool first_item = true;

        foreach (InvItem *invitem, list) {
            foreach (const AppearsInColor &vec, invitem->item()->appearsIn(invitem->color())) {
                foreach (const AppearsInItem &item, vec) {
                    QMap<const Item *, int>::iterator it = unique.find(item.second);
                    if (it != unique.end())
                        ++it.value();
                    else if (first_item)
                        unique.insert(item.second, 1);
                }
            }
            first_item = false;
        }
        for (QMap<const Item *, int>::iterator it = unique.begin(); it != unique.end(); ++it) {
            if (it.value() == list.count())
                m_items.append(new AppearsInItem(-1, it.key()));
        }
    }
}

BrickLink::InternalAppearsInModel::~InternalAppearsInModel()
{
    if (!m_item && !m_color && !m_items.isEmpty())
        qDeleteAll(m_items);
}

QModelIndex BrickLink::InternalAppearsInModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent) && !parent.isValid())
        return createIndex(row, column, m_items.at(row));
    return QModelIndex();
}

const BrickLink::AppearsInItem *BrickLink::InternalAppearsInModel::appearsIn(const QModelIndex &idx) const
{
    return idx.isValid() ? static_cast<const AppearsInItem *>(idx.internalPointer()) : 0;
}

QModelIndex BrickLink::InternalAppearsInModel::index(const AppearsInItem *const_ai) const
{
    AppearsInItem *ai = const_cast<AppearsInItem *>(const_ai);

    return ai ? createIndex(m_items.indexOf(ai), 0, ai) : QModelIndex();
}

int BrickLink::InternalAppearsInModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int BrickLink::InternalAppearsInModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant BrickLink::InternalAppearsInModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    const AppearsInItem *appears = appearsIn(index);
    int col = index.column();

    if (!appears)
        return res;

    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: res = appears->first < 0 ? QLatin1String("-") : QString::number(appears->first); break;
        case 1: res = appears->second->id(); break;
        case 2: res = appears->second->name(); break;
        }
    }
    else if (role == BrickLink::AppearsInItemPointerRole) {
        res.setValue(appears);
    }
    return res;
}

QVariant BrickLink::InternalAppearsInModel::headerData(int section, Qt::Orientation orient, int role) const
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

BrickLink::AppearsInModel::AppearsInModel(const BrickLink::InvItemList &list, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalAppearsInModel(list, this));
}

BrickLink::AppearsInModel::AppearsInModel(const Item *item, const Color *color, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(new InternalAppearsInModel(item, color, this));
}

const BrickLink::AppearsInItem *BrickLink::AppearsInModel::appearsIn(const QModelIndex &index) const
{
    InternalAppearsInModel *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && index.isValid())
        return aim->appearsIn(mapToSource(index));
    return 0;
}

QModelIndex BrickLink::AppearsInModel::index(const AppearsInItem *item) const
{
    InternalAppearsInModel *aim = static_cast<InternalAppearsInModel *>(sourceModel());

    if (aim && item)
        return mapFromSource(aim->index(item));
    return QModelIndex();
}

bool BrickLink::AppearsInModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // the indexes are from the source model, so the internal pointers are valid
    // this is faster than fetching the Category* via data()/QVariant marshalling
    const InternalAppearsInModel *aim = static_cast<const InternalAppearsInModel *>(sourceModel());
    const AppearsInItem *ai1 = aim->appearsIn(left);
    const AppearsInItem *ai2 = aim->appearsIn(right);

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
