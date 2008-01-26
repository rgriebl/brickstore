/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#include <stdlib.h>

#include "cutility.h"
#include "bricklink.h"


BrickLink::Color::Color() : m_name(0), m_peeron_name(0) { }
BrickLink::Color::~Color() { delete [] m_name; delete [] m_peeron_name; }

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const Color *col)
{
    return ds << col->m_id << col->m_name << col->m_peeron_name << col->m_ldraw_id << col->m_color << quint8(col->m_type);
}

QDataStream &operator >> (QDataStream &ds, BrickLink::Color *col)
{
    delete [] col->m_name; delete [] col->m_peeron_name;

    quint8 flags;
    ds >> col->m_id >> col->m_name >> col->m_peeron_name >> col->m_ldraw_id >> col->m_color >> flags;

    col->m_type = flags;
    return ds;
}

} // namespace BrickLink

BrickLink::ItemType::ItemType() : m_name(0), m_categories(0) { }
BrickLink::ItemType::~ItemType() { delete [] m_name; delete [] m_categories; }

QSize BrickLink::ItemType::pictureSize() const
{
    QSize s(80, 60);
    if (m_id == 'M')
        s.transpose();
    return s;
}

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const ItemType *itt)
{
    quint8 flags = 0;
    flags |= (itt->m_has_inventories ? 0x01 : 0);
    flags |= (itt->m_has_colors      ? 0x02 : 0);
    flags |= (itt->m_has_weight      ? 0x04 : 0);
    flags |= (itt->m_has_year        ? 0x08 : 0);

    ds << quint8(itt->m_id) << quint8(itt->m_picture_id) << itt->m_name << flags;

    quint32 catcount = 0;
    if (itt->m_categories) {
        for (const BrickLink::Category **catp = itt->m_categories; *catp; catp++)
            catcount++;
    }
    ds << catcount;
    if (catcount) {
        for (const BrickLink::Category **catp = itt->m_categories; *catp; catp++)
            ds << qint32((*catp)->id());
    }

    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::ItemType *itt)
{
    delete [] itt->m_name;
    delete [] itt->m_categories;

    quint8 flags = 0;
    quint32 catcount = 0;
    quint8 id = 0, picid = 0;
    ds >> id >> picid >> itt->m_name >> flags >> catcount;

    itt->m_id = id;
    itt->m_picture_id = id;

    itt->m_categories = new const BrickLink::Category * [catcount + 1];

    for (quint32 i = 0; i < catcount; i++) {
        qint32 id = 0;
        ds >> id;
        itt->m_categories [i] = BrickLink::inst()->category(id);
    }
    itt->m_categories [catcount] = 0;

    itt->m_has_inventories = flags & 0x01;
    itt->m_has_colors      = flags & 0x02;
    itt->m_has_weight      = flags & 0x04;
    itt->m_has_year        = flags & 0x08;
    return ds;
}

} // namespace BrickLink

BrickLink::Category::Category() : m_name(0) { }
BrickLink::Category::~Category() { delete [] m_name; }

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::Category *cat)
{
    return ds << cat->m_id << cat->m_name;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::Category *cat)
{
    delete [] cat->m_name;
    return ds >> cat->m_id >> cat->m_name;
}

} // namespace BrickLink

BrickLink::Item::Item() : m_id(0), m_name(0), m_categories(0), m_last_inv_update(-1), m_appears_in(0), m_consists_of(0) { }
BrickLink::Item::~Item() { delete [] m_id; delete [] m_name; delete [] m_categories; delete [] m_appears_in; delete [] m_consists_of; }

bool BrickLink::Item::hasCategory(const BrickLink::Category *cat) const
{
    for (const Category **catp = m_categories; *catp; catp++) {
        if (*catp == cat)
            return true;
    }
    return false;
}

int BrickLink::Item::compare(const BrickLink::Item **a, const BrickLink::Item **b)
{
    int d = ((*a)->m_item_type->id() - (*b)->m_item_type->id());

    return d ? d : qstrcmp((*a)->m_id, (*b)->m_id);
}

uint _dwords_for_appears  = 0;
uint _qwords_for_consists = 0;

void BrickLink::Item::setAppearsIn(const AppearsIn &map) const
{
    delete [] m_appears_in;

    int s = 2;

    for (AppearsIn::const_iterator it = map.begin(); it != map.end(); ++it)
        s += (1 + it.value().size());

    _dwords_for_appears += s;

    quint32 *ptr = new quint32 [s];
    m_appears_in = ptr;

    *ptr++ = map.size();   // how many colors
    *ptr++ = s;             // only useful in save/restore ->size in DWORDs

    for (AppearsIn::const_iterator it = map.begin(); it != map.end(); ++it) {
        appears_in_record *color_header = reinterpret_cast <appears_in_record *>(ptr);

        color_header->m12 = it.key()->id();        // color id
        color_header->m20 = it.value().size();      // # of entries

        ptr++;

        for (AppearsInColor::const_iterator itvec = it.value().begin(); itvec != it.value().end(); ++itvec) {
            appears_in_record *color_entry = reinterpret_cast <appears_in_record *>(ptr);

            color_entry->m12 = itvec->first;            // qty
            color_entry->m20 = itvec->second->m_index; // item index

            ptr++;
        }
    }
}

BrickLink::Item::AppearsIn BrickLink::Item::appearsIn(const Color *only_color) const
{
    AppearsIn map;

    const BrickLink::Item * const *items = BrickLink::inst()->items().data();
    uint count = BrickLink::inst()->items().count();

    if (m_appears_in) {
        quint32 *ptr = m_appears_in + 2;

        for (quint32 i = 0; i < m_appears_in [0]; i++) {
            appears_in_record *color_header = reinterpret_cast <appears_in_record *>(ptr);
            ptr++;

            const BrickLink::Color *color = BrickLink::inst()->color(color_header->m12);

            if (color && (!only_color || (color == only_color))) {
                AppearsInColor &vec = map [color];

                for (quint32 j = 0; j < color_header->m20; j++, ptr++) {
                    appears_in_record *color_entry = reinterpret_cast <appears_in_record *>(ptr);

                    uint qty = color_entry->m12;    // qty
                    uint index = color_entry->m20;  // item index

                    if (qty && (index < count))
                        vec.append(QPair <int, const Item *> (qty, items [index]));
                }
            }
            else
                ptr += color_header->m20; // skip
        }
    }

    return map;
}

void BrickLink::Item::setConsistsOf(const InvItemList &items) const
{
    delete [] m_consists_of;

    _qwords_for_consists += (items.count() + 1);

    quint64 *ptr = new quint64 [items.count() + 1];
    m_consists_of = ptr;

    *ptr++ = items.count();     // how many entries

    foreach(const InvItem *item, items) {
        consists_of_record *entry = reinterpret_cast <consists_of_record *>(ptr);

        if (item->item() && item->color() && item->quantity()) {
            entry->m_qty      = item->quantity();
            entry->m_index    = item->item()->m_index;
            entry->m_color    = item->color()->id();
            entry->m_extra    = (item->status() == BrickLink::Extra) ? 1 : 0;
            entry->m_isalt    = 0;
            entry->m_altid    = 0;
            entry->m_reserved = 0;
            ptr++;
        }
        else
            m_consists_of [0]--;
    }
}

BrickLink::InvItemList BrickLink::Item::consistsOf() const
{
    InvItemList list;

    const BrickLink::Item * const *items = BrickLink::inst()->items().data();
    uint count = BrickLink::inst()->items().count();

    if (m_consists_of) {
        quint64 *ptr = m_consists_of + 1;

        for (uint i = 0; i < uint(m_consists_of [0]); i++) {
            consists_of_record *entry = reinterpret_cast <consists_of_record *>(ptr);
            ptr++;

            const BrickLink::Color *color = BrickLink::inst()->color(entry->m_color);
            const BrickLink::Item *item = (entry->m_index < count) ? items [entry->m_index] : 0;

            if (color && item) {
                InvItem *ii = new InvItem(color, item);
                ii->setQuantity(entry->m_qty);
                if (entry->m_extra)
                    ii->setStatus(BrickLink::Extra);

                list.append(ii);
            }
        }
    }

    return list;
}

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::Item *item)
{
    ds << item->m_id << item->m_name << quint8(item->m_item_type->id());

    quint32 catcount = 0;
    if (item->m_categories) {
        for (const BrickLink::Category **catp = item->m_categories; *catp; catp++)
            catcount++;
    }
    ds << catcount;
    if (catcount) {
        for (const BrickLink::Category **catp = item->m_categories; *catp; catp++)
            ds << qint32((*catp)->id());
    }

    qint32 colorid = item->m_color ? qint32(item->m_color->id()) : -1;
    ds << colorid << qint64(item->m_last_inv_update) << item->m_weight << quint32(item->m_index) << quint32(item->m_year);

    if (item->m_appears_in && item->m_appears_in [0] && item->m_appears_in [1]) {
        quint32 *ptr = item->m_appears_in;

        for (quint32 i = 0; i < item->m_appears_in [1]; i++)
            ds << *ptr++;
    }
    else
        ds << quint32(0);

    if (item->m_consists_of && item->m_consists_of [0]) {
        ds << quint32(item->m_consists_of [0]);

        quint64 *ptr = item->m_consists_of + 1;

        for (quint32 i = 0; i < quint32(item->m_consists_of [0]); i++)
            ds << *ptr++;
    }
    else
        ds << quint32(0);

    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::Item *item)
{
    delete [] item->m_id;
    delete [] item->m_name;
    delete [] item->m_categories;
    delete item->m_appears_in;

    quint8 ittid = 0;
    quint32 catcount = 0;

    ds >> item->m_id >> item->m_name >> ittid >> catcount;
    item->m_item_type = BrickLink::inst()->itemType(ittid);

    item->m_categories = new const BrickLink::Category * [catcount + 1];

    for (quint32 i = 0; i < catcount; i++) {
        qint32 id = 0;
        ds >> id;
        item->m_categories [i] = BrickLink::inst()->category(id);
    }
    item->m_categories [catcount] = 0;

    qint32 colorid = 0;
    quint32 index = 0, year = 0;
    qint64 invupd = 0;
    ds >> colorid >> invupd >> item->m_weight >> index >> year;
    item->m_color = colorid == -1 ? 0 : BrickLink::inst()->color(colorid);
    item->m_index = index;
    item->m_year = year;
    item->m_last_inv_update = invupd;

    quint32 appears = 0, appears_size = 0;
    ds >> appears;
    if (appears)
        ds >> appears_size;

    if (appears && appears_size) {
        quint32 *ptr = new quint32 [appears_size];
        item->m_appears_in = ptr;

        *ptr++ = appears;
        *ptr++ = appears_size;

        for (quint32 i = 2; i < appears_size; i++)
            ds >> *ptr++;
    }
    else
        item->m_appears_in = 0;

    quint32 consists = 0;
    ds >> consists;

    if (consists) {
        quint64 *ptr = new quint64 [consists + 1];
        item->m_consists_of = ptr;

        *ptr++ = consists;

        for (quint32 i = 0; i < consists; i++)
            ds >> *ptr++;
    }
    else
        item->m_consists_of = 0;

    return ds;
}

} // namespace BrickLink

BrickLink::InvItem::InvItem(const Color *color, const Item *item)
{
    m_item = item;
    m_color = color;

    m_custom_picture = 0;

    m_status = Include;
    m_condition = New;
    m_retain = m_stockroom = false;
    m_weight = 0;

    m_quantity = m_orig_quantity = 0;
    m_bulk_quantity = 1;
    m_price = m_orig_price = .0;
    m_sale = 0;


    for (int i = 0; i < 3; i++) {
        m_tier_quantity [i] = 0;
        m_tier_price [i] = .0;
    }

    m_incomplete = 0;
    m_lot_id = 0;
}

BrickLink::InvItem::InvItem(const BrickLink::InvItem &copy)
{
    m_incomplete = 0;
    m_custom_picture = 0;

    *this = copy;
}

BrickLink::InvItem &BrickLink::InvItem::operator = (const InvItem &copy)
{
    delete m_incomplete;
    m_incomplete = 0;

    if (m_custom_picture) {
        m_custom_picture->release();
        m_custom_picture = 0;
    }

    m_item           = copy.m_item;
    m_color          = copy.m_color;
    m_status         = copy.m_status;
    m_condition      = copy.m_condition;
    m_retain         = copy.m_retain;
    m_stockroom      = copy.m_stockroom;
    m_comments       = copy.m_comments;
    m_remarks        = copy.m_remarks;
    m_reserved       = copy.m_reserved;
    m_custom_picture_url = copy.m_custom_picture_url;
    m_quantity       = copy.m_quantity;
    m_bulk_quantity  = copy.m_bulk_quantity;
    m_tier_quantity [0] = copy.m_tier_quantity [0];
    m_tier_quantity [1] = copy.m_tier_quantity [1];
    m_tier_quantity [2] = copy.m_tier_quantity [2];
    m_sale           = copy.m_sale;
    m_price          = copy.m_price;
    m_tier_price [0] = copy.m_tier_price [0];
    m_tier_price [1] = copy.m_tier_price [1];
    m_tier_price [2] = copy.m_tier_price [2];
    m_weight         = copy.m_weight;
    m_lot_id         = copy.m_lot_id;
    m_orig_price     = copy.m_orig_price;
    m_orig_quantity  = copy.m_orig_quantity;

    if (copy.m_incomplete) {
        m_incomplete = new Incomplete;

        m_incomplete->m_item_id       = copy.m_incomplete->m_item_id;
        m_incomplete->m_item_name     = copy.m_incomplete->m_item_name;
        m_incomplete->m_itemtype_id   = copy.m_incomplete->m_itemtype_id;
        m_incomplete->m_itemtype_name = copy.m_incomplete->m_itemtype_name;
        m_incomplete->m_category_id   = copy.m_incomplete->m_category_id;
        m_incomplete->m_category_name = copy.m_incomplete->m_category_name;
        m_incomplete->m_color_id      = copy.m_incomplete->m_color_id;
        m_incomplete->m_color_name    = copy.m_incomplete->m_color_name;
    }

    if (m_custom_picture)
        m_custom_picture->addRef();

    return *this;
}

bool BrickLink::InvItem::operator == (const InvItem &cmp) const
{
    bool same = true;

    same &= (m_incomplete         == cmp.m_incomplete);
    same &= (m_custom_picture     == cmp.m_custom_picture);
    same &= (m_item               == cmp.m_item);
    same &= (m_color              == cmp.m_color);
    same &= (m_status             == cmp.m_status);
    same &= (m_condition          == cmp.m_condition);
    same &= (m_retain             == cmp.m_retain);
    same &= (m_stockroom          == cmp.m_stockroom);
    same &= (m_comments           == cmp.m_comments);
    same &= (m_remarks            == cmp.m_remarks);
    same &= (m_reserved           == cmp.m_reserved);
    same &= (m_custom_picture_url == cmp.m_custom_picture_url);
    same &= (m_quantity           == cmp.m_quantity);
    same &= (m_bulk_quantity      == cmp.m_bulk_quantity);
    same &= (m_tier_quantity [0]  == cmp.m_tier_quantity [0]);
    same &= (m_tier_quantity [1]  == cmp.m_tier_quantity [1]);
    same &= (m_tier_quantity [2]  == cmp.m_tier_quantity [2]);
    same &= (m_sale               == cmp.m_sale);
    same &= (m_price              == cmp.m_price);
    same &= (m_tier_price [0]     == cmp.m_tier_price [0]);
    same &= (m_tier_price [1]     == cmp.m_tier_price [1]);
    same &= (m_tier_price [2]     == cmp.m_tier_price [2]);
    same &= (m_weight             == cmp.m_weight);
    same &= (m_lot_id             == cmp.m_lot_id);
    same &= (m_orig_price         == cmp.m_orig_price);
    same &= (m_orig_quantity      == cmp.m_orig_quantity);

    return same;
}

BrickLink::InvItem::~InvItem()
{
    delete m_incomplete;

    if (m_custom_picture)
        m_custom_picture->release();
}

bool BrickLink::InvItem::mergeFrom(const InvItem &from, bool prefer_from)
{
    if ((&from == this) ||
        (from.item() != item()) ||
        (from.color() != color()) ||
        (from.condition() != condition()))
        return false;

    if ((from.price() != 0) && ((price() == 0) || prefer_from))
        setPrice(from.price());
    if ((from.bulkQuantity() != 1) && ((bulkQuantity() == 1) || prefer_from))
        setBulkQuantity(from.bulkQuantity());
    if ((from.sale()) && (!(sale()) || prefer_from))
        setSale(from.sale());

    for (int i = 0; i < 3; i++) {
        if ((from.tierPrice(i) != 0) && ((tierPrice(i) == 0) || prefer_from))
            setTierPrice(i, from.tierPrice(i));
        if ((from.tierQuantity(i)) && (!(tierQuantity(i)) || prefer_from))
            setTierQuantity(i, from.tierQuantity(i));
    }

    if (!from.remarks().isEmpty() && !remarks().isEmpty()) {
        if (from.remarks() == remarks())
            ;
        else if (remarks().indexOf(QRegExp("\\b" + QRegExp::escape(from.remarks()) + "\\b")) != -1)
            ;
        else if (from.remarks().indexOf(QRegExp("\\b" + QRegExp::escape(remarks()) + "\\b")) != -1)
            setRemarks(from.remarks());
        else
            setRemarks(QString(prefer_from  ? "%1 %2" : "%2 %1").arg(from.remarks(), remarks()));
    }
    else if (!from.remarks().isEmpty())
        setRemarks(from.remarks());

    if (!from.comments().isEmpty() && !comments().isEmpty()) {
        if (from.comments() == comments())
            ;
        else if (comments().indexOf(QRegExp("\\b" + QRegExp::escape(from.comments()) + "\\b")) != -1)
            ;
        else if (from.comments().indexOf(QRegExp("\\b" + QRegExp::escape(comments()) + "\\b")) != -1)
            setComments(from.comments());
        else
            setComments(QString(prefer_from  ? "%1 %2" : "%2 %1").arg(from.comments(), comments()));
    }
    else if (!from.comments().isEmpty())
        setComments(from.comments());

    if (!from.reserved().isEmpty() && (reserved().isEmpty() || prefer_from))
        setReserved(from.reserved());
    if (!from.customPictureUrl().isEmpty() && (customPictureUrl().isEmpty() || prefer_from))
        setCustomPictureUrl(from.customPictureUrl());

    if (prefer_from) {
        setStatus(from.status());
        setRetain(from.retain());
        setStockroom(from.stockroom());
    }

    setQuantity(quantity() + from.quantity());
    setOrigQuantity(origQuantity() + from.origQuantity());

    return true;
}


namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::InvItem &ii)
{
    ds << QByteArray(ii.item() ? ii.item()->id() : "");
    ds << qint8(ii.itemType() ? ii.itemType()->id() : -1);
    ds << qint32(ii.color() ? ii.color()->id() : 0xffffffff);

    ds << qint32(ii.status()) << qint32(ii.condition()) << ii.comments() << ii.remarks() << ii.customPictureUrl()
       << ii.quantity() << ii.bulkQuantity() << ii.tierQuantity(0) << ii.tierQuantity(1) << ii.tierQuantity(2)
       << ii.price() << ii.tierPrice(0) << ii.tierPrice(1) << ii.tierPrice(2) << ii.sale()
       << qint8(ii.retain() ? 1 : 0) << qint8(ii.stockroom() ? 1 : 0) << ii.m_reserved << quint32(ii.m_lot_id)
       << ii.origQuantity() << ii.origPrice();
    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::InvItem &ii)
{
    QByteArray itemid;
    qint32 colorid = 0;
    qint8 itemtypeid = 0;
    qint8 retain = 0, stockroom = 0;

    ds >> itemid;
    ds >> itemtypeid;
    ds >> colorid;

    const BrickLink::Item *item = BrickLink::inst()->item(itemtypeid, itemid);

    ii.setItem(item);
    ii.setColor(BrickLink::inst()->color(colorid));

    qint32 status = 0, cond = 0;

    ds >> status >> cond >> ii.m_comments >> ii.m_remarks >> ii.m_custom_picture_url
       >> ii.m_quantity >> ii.m_bulk_quantity >> ii.m_tier_quantity [0] >> ii.m_tier_quantity [1] >> ii.m_tier_quantity [2]
       >> ii.m_price >> ii.m_tier_price [0] >> ii.m_tier_price [1] >> ii.m_tier_price [2] >> ii.m_sale
       >> retain >> stockroom >> ii.m_reserved >> ii.m_lot_id
       >> ii.m_orig_quantity >> ii.m_orig_price;

    ii.m_status = (BrickLink::Status) status;
    ii.m_condition = (BrickLink::Condition) cond;
    ii.m_retain = (retain);
    ii.m_stockroom = (stockroom);

    if (ii.m_custom_picture) {
        ii.m_custom_picture->release();
        ii.m_custom_picture = 0;
    }

    return ds;
}

} // namespace BrickLink

const char *BrickLink::InvItemMimeData::s_mimetype = "application/x-bricklink-invitems";

BrickLink::InvItemMimeData::InvItemMimeData(const InvItemList &items)
    : QMimeData()
{
    setItems(items);
}

void BrickLink::InvItemMimeData::setItems(const InvItemList &items)
{
    QByteArray data;
    QString text;

    QDataStream ds(&data, QIODevice::WriteOnly);

    ds << items.count();
    foreach(const InvItem *ii, items) {
        ds << *ii;
        if (!text.isEmpty())
            text.append("\n");
        text.append(ii->item()->id());
    }
    setText(text);
    setData(s_mimetype, data);
}

BrickLink::InvItemList BrickLink::InvItemMimeData::items(const QMimeData *md)
{
    InvItemList items;

    if (md) {
        QByteArray data = md->data(s_mimetype);
        QDataStream ds(data);

        if (data.size()) {
            quint32 count = 0;
            ds >> count;

            for (; count && !ds.atEnd(); count--) {
                InvItem *ii = new InvItem();
                ds >> *ii;
                items.append(ii);
            }
        }
    }
    return items;
}

QStringList BrickLink::InvItemMimeData::formats() const
{
    static QStringList sl;

    if (sl.isEmpty())
        sl << s_mimetype << "text/plain";

    return sl;
}

bool BrickLink::InvItemMimeData::hasFormat(const QString & mimeType) const
{
    return mimeType.compare(s_mimetype) || mimeType.compare("text/plain");
}

BrickLink::Order::Order(const QString &id, Type type)
    : m_id(id), m_type(type)
{ }

















#include <QFontMetrics>
#include <QApplication>
#include <QPixmap>
#include <QImage>

BrickLink::ColorModel::ColorModel(const Core *)
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
        QPixmap pix = QPixmap::fromImage(BrickLink::inst()->colorImage(c, fm.height(), fm.height()));
        res = pix;
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
    if (m_asc) {
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

BrickLink::ColorModel *BrickLink::Core::colorModel() const
{
    return new ColorModel(this);
}



















// this hack is needed since 0 means 'no selection at all'
const BrickLink::Category *BrickLink::CategoryModel::AllCategories = reinterpret_cast <const BrickLink::Category *>(-1);

BrickLink::CategoryModel::CategoryModel(Features f, const Core *)
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


BrickLink::CategoryModel *BrickLink::Core::categoryModel(CategoryModel::Features f) const
{
    return new CategoryModel(f, this);
}















BrickLink::ItemTypeModel::ItemTypeModel(Features f, const Core *core)
{
    QList<const ItemType *> list = core->itemTypes().values();

    if (f == ExcludeWithoutInventory) {
        foreach(const ItemType *it, list) {
            if (it->hasInventories())
                m_itemtypes.append(it);
        }
    }
    else {
        m_itemtypes = list;
    }
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

void BrickLink::ItemTypeModel::sort(int column, Qt::SortOrder so)
{
    if (column == 0) {
        emit layoutAboutToBeChanged();
        Compare cmp(so == Qt::AscendingOrder);
        qStableSort(m_itemtypes.begin(), m_itemtypes.end(), cmp);
        emit layoutChanged();
    }
}

BrickLink::ItemTypeModel::Compare::Compare(bool asc) : m_asc(asc) { }

bool BrickLink::ItemTypeModel::Compare::operator()(const ItemType *c1, const ItemType *c2)
{
    bool b = (qstrcmp(c1->name(), c2->name()) >= 0);
    return m_asc ^ b;
}


BrickLink::ItemTypeModel *BrickLink::Core::itemTypeModel(ItemTypeModel::Features f) const
{
    return new ItemTypeModel(f, this);
}

