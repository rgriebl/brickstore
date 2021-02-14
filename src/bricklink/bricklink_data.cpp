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
#include <QStringBuilder>

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

bool BrickLink::Item::lowerBound(const Item *item, const std::pair<char, QString> &ids)
{
    int d = (item->m_item_type->id() - ids.first);

    return d == 0 ? (item->m_id.compare(ids.second) < 0) : (d < 0);
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

    auto *ptr = new quint32 [size_t(s)];
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
    int count = int(BrickLink::core()->items().size());

    if (m_appears_in) {
        quint32 *ptr = m_appears_in + 2;

        for (quint32 i = 0; i < m_appears_in [0]; i++) {
            const auto *color_header = reinterpret_cast <const appears_in_record *>(ptr);
            ptr++;

            const BrickLink::Color *color = BrickLink::core()->color(color_header->m12);

            if (color && (!only_color || (color == only_color))) {
                AppearsInColor &vec = map[color];

                for (quint32 j = 0; j < color_header->m20; j++, ptr++) {
                    const auto *color_entry = reinterpret_cast <const appears_in_record *>(ptr);

                    int qty = color_entry->m12;    // qty
                    int index = color_entry->m20;  // item index

                    if (qty && (index < count))
                        vec.append(qMakePair(qty, items[index]));
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

    auto *ptr = new quint64 [size_t(items.count()) + 1];
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
    auto count = BrickLink::core()->items().size();

    if (m_consists_of) {
        const quint64 *ptr = m_consists_of + 1;

        for (uint i = 0; i < uint(m_consists_of[0]); i++) {
            const auto *entry = reinterpret_cast <const consists_of_record *>(ptr);
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

    //TODO: replace with member initializers when switching to c++20
    m_status = Status::Include;
    m_condition = Condition::New;
    m_scondition = SubCondition::None;
    m_retain = false;
    m_stockroom = Stockroom::None;
    m_alternate = false;
    m_alt_id = 0;
    m_cpart = false;
}

BrickLink::InvItem::InvItem(const BrickLink::InvItem &copy)
{
    *this = copy;
}

BrickLink::InvItem &BrickLink::InvItem::operator=(const InvItem &copy)
{
    if (this == &copy)
        return *this;

    m_item                = copy.m_item;
    m_color               = copy.m_color;

    m_incomplete.reset(copy.m_incomplete ? new Incomplete(*copy.m_incomplete.get()) : nullptr);

    m_status              = copy.m_status;
    m_condition           = copy.m_condition;
    m_scondition          = copy.m_scondition;
    m_retain              = copy.m_retain;
    m_stockroom           = copy.m_stockroom;
    m_alternate           = copy.m_alternate;
    m_alt_id              = copy.m_alt_id;
    m_cpart               = copy.m_cpart;
    m_lot_id              = copy.m_lot_id;
    m_reserved            = copy.m_reserved;
    m_comments            = copy.m_comments;
    m_remarks             = copy.m_remarks;
    m_quantity            = copy.m_quantity;
    m_bulk_quantity       = copy.m_bulk_quantity;
    m_tier_quantity[0]    = copy.m_tier_quantity[0];
    m_tier_quantity[1]    = copy.m_tier_quantity[1];
    m_tier_quantity[2]    = copy.m_tier_quantity[2];
    m_sale                = copy.m_sale;
    m_price               = copy.m_price;
    m_cost                = copy.m_cost;
    m_tier_price[0]       = copy.m_tier_price[0];
    m_tier_price[1]       = copy.m_tier_price[1];
    m_tier_price[2]       = copy.m_tier_price[2];
    m_weight              = copy.m_weight;

    return *this;
}

bool BrickLink::InvItem::operator!=(const InvItem &cmp) const
{
    return !operator==(cmp);
}

bool BrickLink::InvItem::operator==(const InvItem &cmp) const
{
    return (!m_incomplete && !cmp.m_incomplete)
            && (m_item             == cmp.m_item)
            && (m_color            == cmp.m_color)
            && (m_status           == cmp.m_status)
            && (m_condition        == cmp.m_condition)
            && (m_scondition       == cmp.m_scondition)
            && (m_retain           == cmp.m_retain)
            && (m_stockroom        == cmp.m_stockroom)
            && (m_lot_id           == cmp.m_lot_id)
            && (m_reserved         == cmp.m_reserved)
            && (m_comments         == cmp.m_comments)
            && (m_remarks          == cmp.m_remarks)
            && (m_quantity         == cmp.m_quantity)
            && (m_bulk_quantity    == cmp.m_bulk_quantity)
            && (m_tier_quantity[0] == cmp.m_tier_quantity[0])
            && (m_tier_quantity[1] == cmp.m_tier_quantity[1])
            && (m_tier_quantity[2] == cmp.m_tier_quantity[2])
            && (m_sale             == cmp.m_sale)
            && qFuzzyCompare(m_price,         cmp.m_price)
            && qFuzzyCompare(m_cost,          cmp.m_cost)
            && qFuzzyCompare(m_tier_price[0], cmp.m_tier_price[0])
            && qFuzzyCompare(m_tier_price[1], cmp.m_tier_price[1])
            && qFuzzyCompare(m_tier_price[2], cmp.m_tier_price[2])
            && qFuzzyCompare(m_weight,        cmp.m_weight);
}

BrickLink::InvItem::~InvItem()
{ }

bool BrickLink::InvItem::mergeFrom(const InvItem &from, bool useCostQtyAg)
{
    if ((&from == this) ||
        (from.isIncomplete() || isIncomplete()) ||
        (from.item() != item()) ||
        (from.color() != color()) ||
        (from.condition() != condition()) ||
        (from.subCondition() != subCondition()))
        return false;

    if (useCostQtyAg) {
        setCost((cost() * quantity() + from.cost() * from.quantity()) / (quantity() + from.quantity()));
    } else if (!qFuzzyIsNull(from.cost()) && qFuzzyIsNull(cost())) {
        setCost(from.cost());
    }
    setQuantity(quantity() + from.quantity());

    if (!qFuzzyIsNull(from.price()) && qFuzzyIsNull(price()))
        setPrice(from.price());
    if ((from.bulkQuantity() != 1) && (bulkQuantity() == 1))
        setBulkQuantity(from.bulkQuantity());
    if ((from.sale()) && !sale())
        setSale(from.sale());

    for (int i = 0; i < 3; i++) {
        if (!qFuzzyIsNull(from.tierPrice(i)) && qFuzzyIsNull(tierPrice(i)))
            setTierPrice(i, from.tierPrice(i));
        if (from.tierQuantity(i) && !tierQuantity(i))
            setTierQuantity(i, from.tierQuantity(i));
    }

    if (!from.remarks().isEmpty() && !remarks().isEmpty() && (from.remarks() != remarks())) {
        QRegularExpression fromRe { u"\\b" % QRegularExpression::escape(from.remarks()) % u"\\b" };

        if (!fromRe.match(remarks()).hasMatch()) {
            QRegularExpression thisRe { u"\\b" % QRegularExpression::escape(remarks()) % u"\\b" };

            if (thisRe.match(from.remarks()).hasMatch())
                setRemarks(from.remarks());
            else
                setRemarks(remarks() % u" " % from.remarks());
        }
    } else if (!from.remarks().isEmpty()) {
        setRemarks(from.remarks());
    }

    if (!from.comments().isEmpty() && !comments().isEmpty() && (from.comments() != comments())) {
        QRegularExpression fromRe { u"\\b" % QRegularExpression::escape(from.comments()) % u"\\b" };

        if (!fromRe.match(comments()).hasMatch()) {
            QRegularExpression thisRe { u"\\b" % QRegularExpression::escape(comments()) % u"\\b" };

            if (thisRe.match(from.comments()).hasMatch())
                setComments(from.comments());
            else
                setComments(comments() % u" " % from.comments());
        }
    } else if (!from.comments().isEmpty()) {
        setComments(from.comments());
    }

    if (!from.reserved().isEmpty() && reserved().isEmpty())
        setReserved(from.reserved());

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

void BrickLink::InvItem::save(QDataStream &ds) const
{
    ds << QByteArray("II") << qint32(2)
       << itemId()
       << qint8(itemType() ? itemType()->id() : char(-1))
       << uint(color() ? color()->id() : uint(0xffffffff))
       << qint8(m_status) << qint8(m_condition) << qint8(m_scondition) << qint8(m_retain ? 1 : 0)
       << qint8(m_stockroom) << m_lot_id << m_reserved << m_comments << m_remarks
       << m_quantity << m_bulk_quantity
       << m_tier_quantity[0] << m_tier_quantity[1] << m_tier_quantity[2]
       << m_sale << m_price << m_cost
       << m_tier_price[0] << m_tier_price[1] << m_tier_price[2]
       << m_weight;
}

BrickLink::InvItem *BrickLink::InvItem::restore(QDataStream &ds)
{
    QScopedPointer<InvItem> ii;

    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version != 2))
        return nullptr;

    QString itemid;
    uint colorid = 0;
    qint8 itemtypeid = 0;

    ds >> itemid >> itemtypeid >> colorid;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    auto item = BrickLink::core()->item(itemtypeid, itemid);
    auto color = BrickLink::core()->color(colorid);

    ii.reset(new BrickLink::InvItem(color, item));

    if (!item || !color) {
        ii->m_incomplete.reset(new BrickLink::InvItem::Incomplete);
        if (!item) {
            ii->m_incomplete->m_item_id = itemid;
            ii->m_incomplete->m_itemtype_id = QLatin1Char(itemtypeid);
        }
        if (!color)
            ii->m_incomplete->m_color_name = QString::number(colorid);

        BrickLink::core()->applyChangeLogToItem(ii.get());
    }

    // alternate, cpart and altid are left out on purpose!

    qint8 status = 0, cond = 0, scond = 0, retain = 0, stockroom = 0;
    ds >> status >> cond >> scond >> retain >> stockroom
            >> ii->m_lot_id >> ii->m_reserved >> ii->m_comments >> ii->m_remarks
            >> ii->m_quantity >> ii->m_bulk_quantity
            >> ii->m_tier_quantity[0] >> ii->m_tier_quantity[1] >> ii->m_tier_quantity[2]
            >> ii->m_sale >> ii->m_price >> ii->m_cost
            >> ii->m_tier_price[0] >> ii->m_tier_price[1] >> ii->m_tier_price[2]
            >> ii->m_weight;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    ii->m_status = static_cast<BrickLink::Status>(status);
    ii->m_condition = static_cast<BrickLink::Condition>(cond);
    ii->m_scondition = static_cast<BrickLink::SubCondition>(scond);
    ii->m_retain = (retain);
    ii->m_stockroom = static_cast<BrickLink::Stockroom>(stockroom);

    return ii.take();
}

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

    ds << quint32(items.count());
    for (const InvItem *ii : items) {
        ii->save(ds);
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
                if (auto item = BrickLink::InvItem::restore(ds))
                    items.append(item);
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
    : m_id(id)
    , m_type(type)
{ }

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
