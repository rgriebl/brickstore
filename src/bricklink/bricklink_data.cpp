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

void BrickLink::Item::setConsistsOf(const QVector<BrickLink::Item::ConsistsOf> &items)
{
    m_consists_of = items;
}

const QVector<BrickLink::Item::ConsistsOf> &BrickLink::Item::consistsOf() const
{
    return m_consists_of;
}

bool BrickLink::Incomplete::operator==(const BrickLink::Incomplete &other) const
{
    return m_item_id           == other.m_item_id
            && m_item_name     == other.m_item_name
            && m_itemtype_id   == other.m_itemtype_id
            && m_itemtype_name == other.m_itemtype_name
            && m_color_id      == other.m_color_id
            && m_color_name    == other.m_color_name
            && m_category_id   == other.m_category_id
            && m_category_name == other.m_category_name;
}


BrickLink::Order::Order(const QString &id, OrderType type)
    : m_id(id)
    , m_type(type)
{ }

BrickLink::Cart::Cart()
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

static QString countryForCode(const QString &code)
{
    for (const auto &country : countryList) {
        QString istr = QString::fromLatin1(country);
        if (istr[0] == code[0] && istr[1] == code[1])
            return istr.mid(3);
    }
    return QString();
}

static QString codeForCountry(const QString &country)
{
    if (!country.isEmpty()) {
        for (const auto &i : countryList) {
            QString istr = QString::fromLatin1(i);
            if (istr.mid(3) == country)
                return istr.left(2);
        }
    }
    return { };
}


void BrickLink::Order::setCountryCode(const QString &str)
{
    if (str.length() == 2) {
        m_countryCode[0] = str[0];
        m_countryCode[1] = str[1];
    }
}

void BrickLink::Order::setCountryName(const QString &str)
{
    setCountryCode(codeForCountry(str));
}

QString BrickLink::Order::countryCode() const
{
    return QString(m_countryCode, 2);
}

QString BrickLink::Order::countryName() const
{
    return countryForCode(countryCode());
}

void BrickLink::Cart::setCountryCode(const QString &str)
{
    if (str.length() == 2) {
        m_countryCode[0] = str[0];
        m_countryCode[1] = str[1];
    }
}

void BrickLink::Cart::setCountryName(const QString &str)
{
    setCountryCode(codeForCountry(str));
}

QString BrickLink::Cart::countryCode() const
{
    return QString(m_countryCode, 2);
}

QString BrickLink::Cart::countryName() const
{
    return countryForCode(countryCode());
}
