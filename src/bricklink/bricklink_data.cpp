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

#include "utility.h"
#include "bricklink.h"


QString BrickLink::Color::typeName(TypeFlag t)
{
    static const QMap<TypeFlag, QString> colortypes = {
        { Solid,       "Solid"_l1 },
        { Transparent, "Transparent"_l1 },
        { Glitter,     "Glitter"_l1 },
        { Speckle,     "Speckle"_l1 },
        { Metallic,    "Metallic"_l1 },
        { Chrome,      "Chrome"_l1 },
        { Pearl,       "Pearl"_l1 },
        { Milky,       "Milky"_l1 },
        { Modulex,     "Modulex"_l1 },
        { Satin,       "Satin"_l1 },
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

bool BrickLink::Item::lowerBound(const Item *item, const std::pair<char, QByteArray> &ids)
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
