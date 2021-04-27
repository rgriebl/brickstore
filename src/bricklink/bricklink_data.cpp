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


const QVector<const BrickLink::Category *> BrickLink::ItemType::categories() const
{
    QVector<const BrickLink::Category *> result;
    result.reserve(int(m_categoryIndexes.size()));
    for (const quint16 idx : m_categoryIndexes)
        result << &core()->categories()[idx];
    return result;
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
{ }

bool BrickLink::Item::lessThan(const Item &item, const std::pair<char, QByteArray> &ids)
{
    int d = (item.m_itemTypeId - ids.first);
    return d == 0 ? (item.m_id.compare(ids.second) < 0) : (d < 0);
}

extern int _dwords_for_appears;
extern int _qwords_for_consists;

int _dwords_for_appears  = 0;
int _qwords_for_consists = 0;

// color-idx -> { vector < qty, item-idx > }
void BrickLink::Item::setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash)
{
    // we are compacting a "hash of a vector of pairs" down to a list of 32bit integers

    m_appears_in.clear();

    for (auto it = appearHash.cbegin(); it != appearHash.cend(); ++it) {
        const auto &colorVector = it.value();

        m_appears_in.push_back({ it.key() /*colorIndex*/, uint(colorVector.size()) /*vectorSize*/ });

        for (auto vecIt = colorVector.cbegin(); vecIt != colorVector.cend(); ++vecIt)
            m_appears_in.push_back({ uint(vecIt->first) /*quantity*/, vecIt->second /*itemIndex*/ });
    }
    m_appears_in.shrink_to_fit();

    _dwords_for_appears += int(m_appears_in.size());
}

BrickLink::AppearsIn BrickLink::Item::appearsIn(const Color *onlyColor) const
{
    AppearsIn appearsHash;

    for (auto it = m_appears_in.cbegin(); it != m_appears_in.cend(); ) {
        // 1st level (color header):  m12: color index / m20: size of 2nd level vector
        quint32 colorIndex = it->m12;
        quint32 vectorSize = it->m20;

        ++it;
        const Color *color = &core()->colors()[colorIndex];

        if (!onlyColor || (color == onlyColor)) {
            AppearsInColor &vec = appearsHash[color];

            for (quint32 i = 0; i < vectorSize; ++i, ++it) {
                // 2nd level (color entry):   m12: quantity / m20: item index

                int quantity = int(it->m12);
                quint32 itemIndex = it->m20;

                const Item *item = &core()->items()[itemIndex];

                if (quantity)
                    vec.append(qMakePair(quantity, item));
            }
        } else {
            it += vectorSize; // skip 2nd level
        }
    }
    return appearsHash;
}

void BrickLink::Item::setConsistsOf(const QVector<BrickLink::Item::ConsistsOf> &items)
{
    m_consists_of = items;
    _qwords_for_consists += m_consists_of.size();
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
