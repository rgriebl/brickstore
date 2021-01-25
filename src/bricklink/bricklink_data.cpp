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
#include <cstdlib>

#include <QString>

#include "bricklink.h"


QString BrickLink::Color::typeName(TypeFlag t)
{
    static const QMap<TypeFlag, QString> colortypes = {
        { Solid,       "Solid" },
        { Transparent, "Transparent" },
        { Glitter,     "Glitter" },
        { Speckle,     "Speckle" },
        { Metallic,    "Metallic" },
        { Chrome,      "Chrome" },
        { Pearl,       "Pearl" },
        { Milky,       "Milky" },
        { Modulex,     "Modulex" },
        { Satin,       "Satin" },
    };
    return colortypes.value(t);
}


QSize BrickLink::ItemType::pictureSize() const
{
    QSize s = rawPictureSize();
    qreal f = BrickLink::core()->itemImageScaleFactor();
    if (!qFuzzyCompare(f, 1))
        s *= f;
    return s;
}

QSize BrickLink::ItemType::rawPictureSize() const
{
    QSize s(80, 60);
    if (m_id == 'M')
        s.transpose();
    return s;
}


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

extern int _dwords_for_appears;
extern int _qwords_for_consists;

int _dwords_for_appears  = 0;
int _qwords_for_consists = 0;

void BrickLink::Item::setAppearsIn(const AppearsIn &map) const
{
    delete [] m_appears_in;

    int s = 2;

    for (AppearsIn::const_iterator it = map.begin(); it != map.end(); ++it)
        s += (1 + it.value().size());

    _dwords_for_appears += s;

    auto *ptr = new quint32 [s];
    m_appears_in = ptr;

    *ptr++ = quint32(map.size());   // how many colors
    *ptr++ = quint32(s);             // only useful in save/restore ->size in DWORDs

    for (AppearsIn::const_iterator it = map.begin(); it != map.end(); ++it) {
        auto *color_header = reinterpret_cast <appears_in_record *>(ptr);

        color_header->m12 = it.key()->id();        // color id
        color_header->m20 = quint32(it.value().size());      // # of entries

        ptr++;

        for (AppearsInColor::const_iterator itvec = it.value().begin(); itvec != it.value().end(); ++itvec) {
            auto *color_entry = reinterpret_cast <appears_in_record *>(ptr);

            color_entry->m12 = quint32(itvec->first);            // qty
            color_entry->m20 = itvec->second->m_index; // item index

            ptr++;
        }
    }
}

BrickLink::AppearsIn BrickLink::Item::appearsIn(const Color *only_color) const
{
    AppearsIn map;

    const BrickLink::Item * const *items = BrickLink::core()->items().data();
    int count = BrickLink::core()->items().count();

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

                    int qty = color_entry->m12;    // qty
                    int index = color_entry->m20;  // item index

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

    *ptr++ = quint32(items.count());     // how many entries

    for (const InvItem *item : items) {
        auto *entry = reinterpret_cast <consists_of_record *>(ptr);

        if (item->item() && item->color() && item->quantity()) {
            entry->m_qty      = uint(item->quantity());
            entry->m_index    = item->item()->m_index;
            entry->m_color    = item->color()->id();
            entry->m_extra    = (item->status() == BrickLink::Status::Extra) ? 1 : 0;
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
    int count = BrickLink::core()->items().count();

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
                    ii->setStatus(BrickLink::Status::Extra);
                ii->setAlternate(entry->m_isalt);
                ii->setAlternateId(entry->m_altid);
                ii->setCounterPart(entry->m_cpart);

                list.append(ii);
            }
        }
    }

    return list;
}


BrickLink::InvItem::InvItem(const Color *color, const Item *item)
{
    m_item = item;
    m_color = color;
    m_status = Status::Include;
    m_condition = Condition::New;
    m_scondition = SubCondition::None;
    m_retain = false;
    m_stockroom = Stockroom::None;
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

    m_cost = 0;
}

BrickLink::InvItem::InvItem(const BrickLink::InvItem &copy)
{
    m_incomplete = nullptr;
    *this = copy;
}

BrickLink::InvItem &BrickLink::InvItem::operator = (const InvItem &copy)
{
    if (this == &copy)
        return *this;

    delete m_incomplete;
    m_incomplete = nullptr;

    m_item           = copy.m_item;
    m_color          = copy.m_color;
    m_status         = copy.m_status;
    m_condition      = copy.m_condition;
    m_scondition     = copy.m_scondition;
    m_retain         = copy.m_retain;
    m_stockroom      = copy.m_stockroom;
    m_xreserved      = copy.m_xreserved;
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
    m_cost           = copy.m_cost;

    if (copy.m_incomplete) {
        m_incomplete = new Incomplete;

        m_incomplete->m_item_id       = copy.m_incomplete->m_item_id;
        m_incomplete->m_item_name     = copy.m_incomplete->m_item_name;
        m_incomplete->m_itemtype_id   = copy.m_incomplete->m_itemtype_id;
        m_incomplete->m_category_name = copy.m_incomplete->m_category_name;
        m_incomplete->m_color_name    = copy.m_incomplete->m_color_name;
    }

    return *this;
}

bool BrickLink::InvItem::operator!=(const InvItem &cmp) const
{
    return !operator==(cmp);
}

bool BrickLink::InvItem::operator == (const InvItem &cmp) const
{
    bool same = true;

    same = same && (m_incomplete         == cmp.m_incomplete);
    same = same && (m_item               == cmp.m_item);
    same = same && (m_color              == cmp.m_color);
    same = same && (m_status             == cmp.m_status);
    same = same && (m_condition          == cmp.m_condition);
    same = same && (m_scondition         == cmp.m_scondition);
    same = same && (m_retain             == cmp.m_retain);
    same = same && (m_stockroom          == cmp.m_stockroom);
    same = same && (m_comments           == cmp.m_comments);
    same = same && (m_remarks            == cmp.m_remarks);
    same = same && (m_reserved           == cmp.m_reserved);
    same = same && (m_quantity           == cmp.m_quantity);
    same = same && (m_bulk_quantity      == cmp.m_bulk_quantity);
    same = same && (m_tier_quantity [0]  == cmp.m_tier_quantity [0]);
    same = same && (m_tier_quantity [1]  == cmp.m_tier_quantity [1]);
    same = same && (m_tier_quantity [2]  == cmp.m_tier_quantity [2]);
    same = same && (m_sale               == cmp.m_sale);
    same = same && qFuzzyCompare(m_price,          cmp.m_price);
    same = same && qFuzzyCompare(m_tier_price [0], cmp.m_tier_price [0]);
    same = same && qFuzzyCompare(m_tier_price [1], cmp.m_tier_price [1]);
    same = same && qFuzzyCompare(m_tier_price [2], cmp.m_tier_price [2]);
    same = same && qFuzzyCompare(m_weight,         cmp.m_weight);
    same = same && (m_lot_id             == cmp.m_lot_id);
    same = same && qFuzzyCompare(m_orig_price,     cmp.m_orig_price);
    same = same && (m_orig_quantity      == cmp.m_orig_quantity);
    same = same && (m_cost               == cmp.m_cost);

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

    if (!qFuzzyIsNull(from.price()) && (qFuzzyIsNull(price()) || prefer_from))
        setPrice(from.price());
    if (!qFuzzyIsNull(from.cost()) && (qFuzzyIsNull(cost()) || prefer_from))
        setCost(from.cost());
    if ((from.bulkQuantity() != 1) && ((bulkQuantity() == 1) || prefer_from))
        setBulkQuantity(from.bulkQuantity());
    if ((from.sale()) && (!(sale()) || prefer_from))
        setSale(from.sale());

    for (int i = 0; i < 3; i++) {
        if (!qFuzzyIsNull(from.tierPrice(i)) && (qFuzzyIsNull(tierPrice(i)) || prefer_from))
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

QImage BrickLink::InvItem::image() const
{
    BrickLink::Picture *pic = BrickLink::core()->picture(item(), color());

    if (pic && pic->valid()) {
        return pic->image();
    } else {
        QSize s = BrickLink::core()->standardPictureSize();
        return BrickLink::core()->noImage(s);
    }
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
       << qint8(ii.retain() ? 1 : 0) << qint8(ii.stockroom()) << ii.m_reserved << quint32(ii.m_lot_id)
       << ii.origQuantity() << ii.origPrice() << qint32(ii.subCondition()) << ii.cost();
    return ds;
}

QDataStream &operator >> (QDataStream &ds, BrickLink::InvItem &ii)
{
    QByteArray itemid;
    quint32 colorid = 0;
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
            inc->m_itemtype_id = QLatin1Char(itemtypeid);
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
       >> ii.m_orig_quantity >> ii.m_orig_price >> scond >> ii.m_cost;

    ii.m_status = static_cast<BrickLink::Status>(status);
    ii.m_condition = static_cast<BrickLink::Condition>(cond);
    ii.m_scondition = static_cast<BrickLink::SubCondition>(scond);
    ii.m_retain = (retain);
    ii.m_stockroom = static_cast<BrickLink::Stockroom>(stockroom);

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

bool BrickLink::InvItemMimeData::hasFormat(const QString &mimeType) const
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

// BrickLink doesn't use the standard ISO country names...
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
