/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <cstdlib>

#include <QString>

#include "utility.h"
#include "bricklink.h"


QString BrickLink::Color::typeName(TypeFlag t)
{
    static QMap<TypeFlag, QString> colortypes;
    if (colortypes.isEmpty()) {
        colortypes.insert(Solid,    "Solid");
        colortypes.insert(Transparent, "Transparent");
        colortypes.insert(Glitter,  "Glitter");
        colortypes.insert(Speckle,  "Speckle");
        colortypes.insert(Metallic, "Metallic");
        colortypes.insert(Chrome,   "Chrome");
        colortypes.insert(Pearl,    "Pearl");
        colortypes.insert(Milky,    "Milky");
        colortypes.insert(Modulex,  "Modulex");
    };
    return colortypes.value(t);
}

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const Color *col)
{
    ds << col->m_id << col->m_name.toUtf8() << col->m_ldraw_id << col->m_color
       << quint32(col->m_type) << float(col->m_popularity) << col->m_year_from << col->m_year_to;
    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::Color *col)
{
    col->~Color();

    quint32 flags;
    float popularity;
    QByteArray name;

    ds >> col->m_id >> name >> col->m_ldraw_id >> col->m_color >> flags >> popularity
       >> col->m_year_from >> col->m_year_to;

    col->m_name = QString::fromUtf8(name);
    col->m_type = static_cast<BrickLink::Color::Type>(flags);
    col->m_popularity = qreal(popularity);
    return ds;
}

} // namespace BrickLink


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
    flags |= (itt->m_has_inventories   ? 0x01 : 0);
    flags |= (itt->m_has_colors        ? 0x02 : 0);
    flags |= (itt->m_has_weight        ? 0x04 : 0);
    flags |= (itt->m_has_year          ? 0x08 : 0);
    flags |= (itt->m_has_subconditions ? 0x10 : 0);

    ds << quint8(itt->m_id) << quint8(itt->m_picture_id) << itt->m_name.toUtf8() << flags;

    ds << quint32(itt->m_categories.size());
    for (const BrickLink::Category *cat : itt->m_categories)
        ds << qint32(cat->id());

    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::ItemType *itt)
{
    itt->~ItemType();

    quint8 flags = 0;
    quint32 catcount = 0;
    quint8 id = 0, picid = 0;
    QByteArray name;
    ds >> id >> picid >> name >> flags >> catcount;

    itt->m_name = QString::fromUtf8(name);
    itt->m_id = id;
    itt->m_picture_id = id;

    itt->m_categories.resize(catcount);

    for (quint32 i = 0; i < catcount; i++) {
        qint32 id = 0;
        ds >> id;
        itt->m_categories[i] = BrickLink::core()->category(id);
    }

    itt->m_has_inventories   = flags & 0x01;
    itt->m_has_colors        = flags & 0x02;
    itt->m_has_weight        = flags & 0x04;
    itt->m_has_year          = flags & 0x08;
    itt->m_has_subconditions = (id == 'S'); //flags & 0x10;
    return ds;
}

} // namespace BrickLink


namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::Category *cat)
{
    return ds << cat->m_id << cat->m_name.toUtf8();
}

QDataStream &operator >> (QDataStream &ds, BrickLink::Category *cat)
{
    cat->~Category();
    QByteArray name;
    ds >> cat->m_id >> name;
    cat->m_name = QString::fromUtf8(name);
    return ds;
}

} // namespace BrickLink

BrickLink::Item::~Item()
{
    delete [] m_appears_in;
    delete [] m_consists_of;
}

bool BrickLink::Item::hasCategory(const BrickLink::Category *cat) const
{
    for (const Category *c: m_categories) {
        if (c == cat)
            return true;
    }
    return false;
}

bool BrickLink::Item::lessThan(const BrickLink::Item *a, const BrickLink::Item *b)
{
    int d = (a->m_item_type->id() - b->m_item_type->id());

    return d == 0 ? (a->m_id.compare(b->m_id) < 0) : (d < 0);
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

    auto *ptr = new quint32 [s];
    m_appears_in = ptr;

    *ptr++ = map.size();   // how many colors
    *ptr++ = s;             // only useful in save/restore ->size in DWORDs

    for (AppearsIn::const_iterator it = map.begin(); it != map.end(); ++it) {
        auto *color_header = reinterpret_cast <appears_in_record *>(ptr);

        color_header->m12 = it.key()->id();        // color id
        color_header->m20 = it.value().size();      // # of entries

        ptr++;

        for (AppearsInColor::const_iterator itvec = it.value().begin(); itvec != it.value().end(); ++itvec) {
            auto *color_entry = reinterpret_cast <appears_in_record *>(ptr);

            color_entry->m12 = itvec->first;            // qty
            color_entry->m20 = itvec->second->m_index; // item index

            ptr++;
        }
    }
}

BrickLink::AppearsIn BrickLink::Item::appearsIn(const Color *only_color) const
{
    AppearsIn map;

    const BrickLink::Item * const *items = BrickLink::core()->items().data();
    uint count = BrickLink::core()->items().count();

    if (m_appears_in) {
        quint32 *ptr = m_appears_in + 2;

        for (quint32 i = 0; i < m_appears_in [0]; i++) {
            auto *color_header = reinterpret_cast <appears_in_record *>(ptr);
            ptr++;

            const BrickLink::Color *color = BrickLink::core()->color(color_header->m12);

            if (color && (!only_color || (color == only_color))) {
                AppearsInColor &vec = map [color];

                for (quint32 j = 0; j < color_header->m20; j++, ptr++) {
                    auto *color_entry = reinterpret_cast <appears_in_record *>(ptr);

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

    auto *ptr = new quint64 [items.count() + 1];
    m_consists_of = ptr;

    *ptr++ = items.count();     // how many entries

    for (const InvItem *item : items) {
        auto *entry = reinterpret_cast <consists_of_record *>(ptr);

        if (item->item() && item->color() && item->quantity()) {
            entry->m_qty      = item->quantity();
            entry->m_index    = item->item()->m_index;
            entry->m_color    = item->color()->id();
            entry->m_extra    = (item->status() == BrickLink::Extra) ? 1 : 0;
            entry->m_isalt    = item->alternate();
            entry->m_altid    = item->alternateId();
            entry->m_cpart    = item->counterPart();
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

    const BrickLink::Item * const *items = BrickLink::core()->items().data();
    uint count = BrickLink::core()->items().count();

    if (m_consists_of) {
        quint64 *ptr = m_consists_of + 1;

        for (uint i = 0; i < uint(m_consists_of [0]); i++) {
            auto *entry = reinterpret_cast <consists_of_record *>(ptr);
            ptr++;

            const BrickLink::Color *color = BrickLink::core()->color(entry->m_color);
            const BrickLink::Item *item = (entry->m_index < count) ? items [entry->m_index] : nullptr;

            if (color && item) {
                auto *ii = new InvItem(color, item);
                ii->setQuantity(entry->m_qty);
                if (entry->m_extra)
                    ii->setStatus(BrickLink::Extra);
                ii->setAlternate(entry->m_isalt);
                ii->setAlternateId(entry->m_altid);
                ii->setCounterPart(entry->m_cpart);

                list.append(ii);
            }
        }
    }

    return list;
}

namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::Item *item)
{
    ds << item->m_id.toUtf8() << item->m_name.toUtf8() << quint8(item->m_item_type->id());

    ds << quint32(item->m_categories.size());
    for (const BrickLink::Category *cat : item->m_categories)
        ds << qint32(cat->id());

    qint32 colorid = item->m_color ? qint32(item->m_color->id()) : -1;
    ds << colorid << qint64(item->m_last_inv_update) << item->m_weight
       << quint32(item->m_index) << quint32(item->m_year);

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
    item->~Item();

    quint8 ittid = 0;
    quint32 catcount = 0;
    QByteArray id;
    QByteArray name;

    ds >> id >> name >> ittid >> catcount;
    item->m_id = QString::fromUtf8(id);
    item->m_name = QString::fromUtf8(name);
    item->m_item_type = BrickLink::core()->itemType(ittid);

    item->m_categories.resize(catcount);

    for (quint32 i = 0; i < catcount; i++) {
        qint32 id = 0;
        ds >> id;
        item->m_categories[i] = BrickLink::core()->category(id);
    }

    qint32 colorid = 0;
    quint32 index = 0, year = 0;
    qint64 invupd = 0;
    ds >> colorid >> invupd >> item->m_weight >> index >> year;
    item->m_color = BrickLink::core()->color(colorid == -1 ? 0 : colorid);
    item->m_index = index;
    item->m_year = year;
    item->m_last_inv_update = invupd;

    quint32 appears = 0, appears_size = 0;
    ds >> appears;
    if (appears)
        ds >> appears_size;

    if (appears && appears_size) {
        auto *ptr = new quint32 [appears_size];
        item->m_appears_in = ptr;

        *ptr++ = appears;
        *ptr++ = appears_size;

        for (quint32 i = 2; i < appears_size; i++)
            ds >> *ptr++;
    }
    else
        item->m_appears_in = nullptr;

    quint32 consists = 0;
    ds >> consists;

    if (consists) {
        auto *ptr = new quint64 [consists + 1];
        item->m_consists_of = ptr;

        *ptr++ = consists;

        for (quint32 i = 0; i < consists; i++)
            ds >> *ptr++;
    }
    else
        item->m_consists_of = nullptr;

    return ds;
}

} // namespace BrickLink


BrickLink::InvItem::InvItem(const Color *color, const Item *item)
{
    m_item = item;
    m_color = color;
    m_status = Include;
    m_condition = New;
    m_scondition = None;
    m_retain = m_stockroom = false;
    m_alternate = false;
    m_alt_id = 0;
    m_cpart = false;
    m_xreserved = 0;
    m_weight = 0;

    m_quantity = m_orig_quantity = 0;
    m_bulk_quantity = 1;
    m_price = m_orig_price = .0;
    m_sale = 0;


    for (int i = 0; i < 3; i++) {
        m_tier_quantity [i] = 0;
        m_tier_price [i] = .0;
    }

    m_incomplete = nullptr;
    m_lot_id = 0;
}

BrickLink::InvItem::InvItem(const BrickLink::InvItem &copy)
{
    m_incomplete = nullptr;
    *this = copy;
}

BrickLink::InvItem &BrickLink::InvItem::operator = (const InvItem &copy)
{
    delete m_incomplete;
    m_incomplete = nullptr;

    m_item           = copy.m_item;
    m_color          = copy.m_color;
    m_status         = copy.m_status;
    m_condition      = copy.m_condition;
    m_scondition     = copy.m_scondition;
    m_retain         = copy.m_retain;
    m_stockroom      = copy.m_stockroom;
    m_alternate      = copy.m_alternate;
    m_alt_id         = copy.m_alt_id;
    m_cpart          = copy.m_cpart;
    m_comments       = copy.m_comments;
    m_remarks        = copy.m_remarks;
    m_reserved       = copy.m_reserved;
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
        m_incomplete->m_itemtype_name = copy.m_incomplete->m_itemtype_name;
        m_incomplete->m_category_name = copy.m_incomplete->m_category_name;
        m_incomplete->m_color_name    = copy.m_incomplete->m_color_name;
    }

    return *this;
}

bool BrickLink::InvItem::operator == (const InvItem &cmp) const
{
    bool same = true;

    same &= (m_incomplete         == cmp.m_incomplete);
    same &= (m_item               == cmp.m_item);
    same &= (m_color              == cmp.m_color);
    same &= (m_status             == cmp.m_status);
    same &= (m_condition          == cmp.m_condition);
    same &= (m_scondition         == cmp.m_scondition);
    same &= (m_retain             == cmp.m_retain);
    same &= (m_stockroom          == cmp.m_stockroom);
    same &= (m_comments           == cmp.m_comments);
    same &= (m_remarks            == cmp.m_remarks);
    same &= (m_reserved           == cmp.m_reserved);
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
}

bool BrickLink::InvItem::mergeFrom(const InvItem &from, bool prefer_from)
{
    if ((&from == this) ||
        (from.isIncomplete() || isIncomplete()) ||
        (from.item() != item()) ||
        (from.color() != color()) ||
        (from.condition() != condition()) ||
        (from.subCondition() != subCondition()))
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

    if (prefer_from) {
        setStatus(from.status());
        setRetain(from.retain());
        setStockroom(from.stockroom());
        setLotId(from.lotId());
    }

    setQuantity(quantity() + from.quantity());
    setOrigQuantity(origQuantity() + from.origQuantity());

    return true;
}


namespace BrickLink {

QDataStream &operator << (QDataStream &ds, const BrickLink::InvItem &ii)
{
    ds << ii.itemId().toUtf8();
    ds << qint8(ii.itemType() ? ii.itemType()->id() : -1);
    ds << qint32(ii.color() ? ii.color()->id() : 0xffffffff);

    ds << qint32(ii.status()) << qint32(ii.condition()) << ii.comments() << ii.remarks()
       << ii.quantity() << ii.bulkQuantity() << ii.tierQuantity(0) << ii.tierQuantity(1) << ii.tierQuantity(2)
       << ii.price() << ii.tierPrice(0) << ii.tierPrice(1) << ii.tierPrice(2) << ii.sale()
       << qint8(ii.retain() ? 1 : 0) << qint8(ii.stockroom() ? 1 : 0) << ii.m_reserved << quint32(ii.m_lot_id)
       << ii.origQuantity() << ii.origPrice() << qint32(ii.subCondition());
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

    const BrickLink::Item *item = BrickLink::core()->item(itemtypeid, QString::fromUtf8(itemid));
    const BrickLink::Color *color = BrickLink::core()->color(colorid);

    ii.setItem(item);
    ii.setColor(color);

    BrickLink::InvItem::Incomplete *inc = nullptr;
    if (!item || !color) {
        inc = new BrickLink::InvItem::Incomplete;
        if (!item) {
            inc->m_item_id = itemid;
            inc->m_itemtype_name = QLatin1Char(itemtypeid);
        }
        if (!color)
            inc->m_color_name = QString::number(colorid);
    }
    ii.setIncomplete(inc);

    qint32 status = 0, cond = 0, scond = 0;

    ds >> status >> cond >> ii.m_comments >> ii.m_remarks
       >> ii.m_quantity >> ii.m_bulk_quantity >> ii.m_tier_quantity [0] >> ii.m_tier_quantity [1] >> ii.m_tier_quantity [2]
       >> ii.m_price >> ii.m_tier_price [0] >> ii.m_tier_price [1] >> ii.m_tier_price [2] >> ii.m_sale
       >> retain >> stockroom >> ii.m_reserved >> ii.m_lot_id
       >> ii.m_orig_quantity >> ii.m_orig_price >> scond;

    ii.m_status = (BrickLink::Status) status;
    ii.m_condition = (BrickLink::Condition) cond;
    ii.m_scondition = (BrickLink::SubCondition) scond;
    ii.m_retain = (retain);
    ii.m_stockroom = (stockroom);

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
    for (const InvItem *ii : items) {
        ds << *ii;
        if (!text.isEmpty())
            text.append("\n");
        text.append(ii->itemId());
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

        if (!data.isEmpty()) {
            quint32 count = 0;
            ds >> count;

            for (; count && !ds.atEnd(); count--) {
                auto *ii = new InvItem();
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

BrickLink::Order::Order(const QString &id, OrderType type)
    : m_id(id), m_type(type), m_shipping(0), m_insurance(0),
      m_delivery(0), m_credit(0), m_grand_total(0)
{
    m_countryCode[0] = QLatin1Char('U');
    m_countryCode[1] = QLatin1Char('S');
}

// Bricklink doesn't use the standard ISO country names...
static const char *countryList[] = {
    "AF Afghanistan",
    "AX ",
    "AL Albania",
    "DZ Algeria",
    "AS American Samoa",
    "AD Andorra",
    "AO Angola",
    "AI Anguilla",
    "AQ ",
    "AG Antigua and Barbuda",
    "AR Argentina",
    "AM Armenia",
    "AW Aruba",
    "AU Australia",
    "AT Austria",
    "AZ Azerbaijan",
    "BS Bahamas",
    "BH Bahrain",
    "BD Bangladesh",
    "BB Barbados",
    "BY Belarus",
    "BE Belgium",
    "BZ Belize",
    "BJ Benin",
    "BM Bermuda",
    "BT Bhutan",
    "BO Bolivia",
    "BQ ",
    "BA Bosnia and Herzegovina",
    "BW Botswana",
    "BV ",
    "BR Brazil",
    "IO British Indian Ocean Territory",
    "BN Brunei",
    "BG Bulgaria",
    "BF Burkina Faso",
    "BI Burundi",
    "KH Cambodia",
    "CM Cameroon",
    "CA Canada",
    "CV Cape Verde",
    "KY Cayman Islands",
    "CF Central African Republic",
    "TD Chad",
    "CL Chile",
    "CN China",
    "CX ",
    "CC ",
    "CO Colombia",
    "KM Comoros",
    "CG Congo",
    "CD Congo (DRC)",
    "CK Cook Islands",
    "CR Costa Rica",
    "CI Cote D'Ivoire",
    "HR Croatia",
    "CU Cuba",
    "CW ",
    "CY Cyprus",
    "CZ Czech Republic",
    "DK Denmark",
    "DJ Djibouti",
    "DM Dominica",
    "DO Dominican Republic",
    "EC Ecuador",
    "EG Egypt",
    "SV El Salvador",
    "GQ Equatorial Guinea",
    "ER Eritrea",
    "EE Estonia",
    "ET Ethiopia",
    "FK Falkland Islands (Islas Malvinas)",
    "FO Faroe Islands",
    "FJ Fiji",
    "FI Finland",
    "FR France",
    "GF French Guiana",
    "PF French Polynesia",
    "TF ",
    "GA Gabon",
    "GM Gambia",
    "GE Georgia",
    "DE Germany",
    "GH Ghana",
    "GI Gibraltar",
    "GR Greece",
    "GL Greenland",
    "GD Grenada",
    "GP Guadeloupe",
    "GU Guam",
    "GT Guatemala",
    "GG Guernsey",
    "GN Guinea",
    "GW Guinea-Bissau",
    "GY Guyana",
    "HT Haiti",
    "HM ",
    "VA Vatican City State",
    "HN Honduras",
    "HK Hong Kong",
    "HU Hungary",
    "IS Iceland",
    "IN India",
    "ID Indonesia",
    "IR Iran",
    "IQ Iraq",
    "IE Ireland",
    "IM Isle of Man",
    "IL Israel",
    "IT Italy",
    "JM Jamaica",
    "JP Japan",
    "JE Jersey",
    "JO Jordan",
    "KZ Kazakhstan",
    "KE Kenya",
    "KI Kiribati",
    "KP North Korea",
    "KR South Korea",
    "KW Kuwait",
    "KG Kyrgyzstan",
    "LA Laos",
    "LV Latvia",
    "LB Lebanon",
    "LS Lesotho",
    "LR Liberia",
    "LY Lybia",
    "LI Liechtenstein",
    "LT Lithuania",
    "LU Luxembourg",
    "MO Macau",
    "MK Macedonia",
    "MG Madagascar",
    "MW Malawi",
    "MY Malaysia",
    "MV Maldives",
    "ML Mali",
    "MT Malta",
    "MH Marshall Islands",
    "MQ Martinique",
    "MR Mauritania",
    "MU Mauritius",
    "YT Mayotte",
    "MX Mexico",
    "FM Micronesia",
    "MD Moldova",
    "MC Monaco",
    "MN Mongolia",
    "ME Montenegro",
    "MS Montserrat",
    "MA Morocco",
    "MZ Mozambique",
    "MM Myanmar",
    "NA Namibia",
    "NR Nauru",
    "NP Nepal",
    "NL Netherlands",
    "NC New Caledonia",
    "NZ New Zealand",
    "NI Nicaragua",
    "NE Niger",
    "NG Nigeria",
    "NU Niue",
    "NF Norfolk Island",
    "MP Northern Mariana Islands",
    "NO Norway",
    "OM Oman",
    "PK Pakistan",
    "PW Palau",
    "PS ",
    "PA Panama",
    "PG Papua new Guinea",
    "PY Paraguay",
    "PE Peru",
    "PH Philippines",
    "PN Pitcairn Islands",
    "PL Poland",
    "PT Portugal",
    "PR Puerto Rico",
    "QA Qatar",
    "RE Reunion",
    "RO Romania",
    "RU Russia",
    "RW Rwanda",
    "BL ",
    "SH St. Helena",
    "KN St. Kitts and Nevis",
    "LC St. Lucia",
    "MF ",
    "PM St. Pierre and Miquelon",
    "VC St. Vincent and the Grenadines",
    "WS Samoa",
    "SM San Marino",
    "ST Sao Tome and Principe",
    "SA Saudi Arabia",
    "SN Senegal",
    "RS Serbia",
    "SC Seychelles",
    "SL Sierra Leone",
    "SG Singapore",
    "SX ",
    "SK Slovakia",
    "SI Slovenia",
    "SB Solomon Islands",
    "SO Somalia",
    "ZA South Africa",
    "GS South Georgia",
    "ES Spain",
    "LK Sri Lanka",
    "SD Sudan",
    "SR Suriname",
    "SJ Svalbard and Jan Mayen",
    "SZ Swaziland",
    "SE Sweden",
    "CH Switzerland",
    "SY Syria",
    "TW Taiwan",
    "TJ Tajikistan",
    "TZ Tanzania",
    "TH Thailand",
    "TL East Timor",
    "TG Togo",
    "TK ",
    "TO Tonga",
    "TT Trinidad and Tobago",
    "TN Tunisia",
    "TR Turkey",
    "TM Turkmenistan",
    "TC Turks and Caicos Islands",
    "TV Tuvalu",
    "UG Uganda",
    "UA Ukraine",
    "AE United Arab Emirates",
    "GB ",
    "US USA",
    "UM ",
    "UY Uruguay",
    "UZ Uzbekistan",
    "VU Vanuatu",
    "VE Venezuela",
    "VN Vietnam",
    "VG Virgin Islands (British)",
    "VI Virgin Islands (US)",
    "WF Wallis and Futuna",
    "EH ",
    "YE Yemen",
    "ZM Zambia",
    "ZW Zimbabwe"
};


void BrickLink::Order::setCountryCode(const QString &str)
{
    if (str.length() == 2) {
        m_countryCode[0] = str[0];
        m_countryCode[1] = str[1];
    }
}

void BrickLink::Order::setCountryName(const QString &str)
{
    if (str.isEmpty())
        return;
    for (const auto &i : countryList) {
        QString istr = QString::fromLatin1(i);
        if (istr.mid(3) == str) {
            setCountryCode(istr.left(2));
            break;
        }
    }
}

QString BrickLink::Order::countryCode() const
{
    return QString(m_countryCode, 2);
}

QString BrickLink::Order::countryName() const
{
    for (const auto &country : countryList) {
        QString istr = QString::fromLatin1(country);
        if (istr[0] == m_countryCode[0] && istr[1] == m_countryCode[1])
            return istr.mid(3);
    }
    return QString();
}
