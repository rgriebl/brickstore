/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include "bricklink/item.h"
#include "bricklink/core.h"


extern int _dwords_for_appears;
extern int _qwords_for_consists;

int _dwords_for_appears  = 0;
int _qwords_for_consists = 0;


bool BrickLink::Item::lessThan(const Item &item, const std::pair<char, QByteArray> &ids)
{
    int d = (item.m_itemTypeId - ids.first);
    return d == 0 ? (item.m_id.compare(ids.second) < 0) : (d < 0);
}

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

const BrickLink::ItemType *BrickLink::Item::itemType() const
{
    return (m_itemTypeIndex != -1) ? &core()->itemTypes()[m_itemTypeIndex] : nullptr;
}

const BrickLink::Category *BrickLink::Item::category() const
{
    return (m_categoryIndex != -1) ? &core()->categories()[m_categoryIndex] : nullptr;
}

const QVector<const BrickLink::Category *> BrickLink::Item::additionalCategories(bool includeMainCategory) const
{
    QVector<const BrickLink::Category *> cats;
    if (includeMainCategory && (m_categoryIndex != 1))
        cats << &core()->categories()[m_categoryIndex];
    for (const auto catIndex : m_additionalCategoryIndexes)
        cats << &core()->categories()[catIndex];
    return cats;
}

const BrickLink::Color *BrickLink::Item::defaultColor() const
{
    return (m_defaultColorIndex != -1) ? &core()->colors()[m_defaultColorIndex] : nullptr;
}

QDateTime BrickLink::Item::inventoryUpdated() const
{
    QDateTime dt;
    if (m_lastInventoryUpdate >= 0)
        dt.setSecsSinceEpoch(m_lastInventoryUpdate);
    return dt;
}

bool BrickLink::Item::hasKnownColor(const Color *col) const
{
    if (!col)
        return true;
//    return std::find(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
//                     quint16(col - core()->colors().data())) != m_knownColorIndexes.cend();
    return std::binary_search(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
                              quint16(col - core()->colors().data()));
}

const QVector<const BrickLink::Color *> BrickLink::Item::knownColors() const
{
    QVector<const Color *> result;
    for (const quint16 idx : m_knownColorIndexes)
        result << &core()->colors()[idx];
    return result;
}

uint BrickLink::Item::index() const
{
    return (this - core()->items().data());
}

const BrickLink::Item *BrickLink::Item::ConsistsOf::item() const
{
    return &core()->items().at(m_itemIndex);
}

const BrickLink::Color *BrickLink::Item::ConsistsOf::color() const
{
    return &core()->colors().at(m_colorIndex);
}

bool BrickLink::Item::ConsistsOf::isSimple(const QVector<ConsistsOf> &parts)
{
    for (const auto &co : parts) {
        if (co.isExtra() || co.isCounterPart() || co.isAlternate())
            return false;
    }
    return true;
}
