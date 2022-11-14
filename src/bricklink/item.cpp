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


namespace BrickLink {

bool Item::lessThan(const Item &item, const std::pair<char, QByteArray> &ids)
{
    int d = (item.m_itemTypeId - ids.first);
    return d == 0 ? (item.m_id.compare(ids.second) < 0) : (d < 0);
}

// color-idx -> { vector < qty, item-idx > }
void Item::setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash)
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

AppearsIn Item::appearsIn(const Color *onlyColor) const
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

void Item::setConsistsOf(const QVector<Item::ConsistsOf> &items)
{
    m_consists_of = items;
    _qwords_for_consists += m_consists_of.size();
}

const QVector<Item::ConsistsOf> &Item::consistsOf() const
{
    return m_consists_of;
}

PartOutTraits Item::partOutTraits() const
{
    PartOutTraits traits;

    if (itemTypeId() == 'S') {
        if (core()->item('I', id()))
            traits |= PartOutTrait::Instructions;
        if (core()->item('O', id()))
            traits |= PartOutTrait::OriginalBox;
    }

    for (const auto &co : m_consists_of) {
        if (co.isExtra())
            traits |= PartOutTrait::Extras;
        if (co.isCounterPart())
            traits |= PartOutTrait::CounterParts;
        if (co.isAlternate())
            traits |= PartOutTrait::Alternates;

        const auto itemTypeId = co.item()->itemTypeId();
        if (co.item()->hasInventory() && ((itemTypeId == 'M') || (itemTypeId == 'S'))) {
            traits |= ((itemTypeId == 'M') ? PartOutTrait::Minifigs : PartOutTrait::SetsInSet);
            traits |= co.item()->partOutTraits();
        }
    }
    return traits;
}

const ItemType *Item::itemType() const
{
    return (m_itemTypeIndex > -1) ? &core()->itemTypes()[uint(m_itemTypeIndex)] : nullptr;
}

const Category *Item::category() const
{
    return (m_categoryIndex > -1) ? &core()->categories()[uint(m_categoryIndex)] : nullptr;
}

const QVector<const Category *> Item::additionalCategories(bool includeMainCategory) const
{
    QVector<const Category *> cats;
    if (includeMainCategory && (m_categoryIndex > -1))
        cats << &core()->categories()[uint(m_categoryIndex)];
    for (const auto catIndex : m_additionalCategoryIndexes) {
        if (catIndex > -1)
            cats << &core()->categories()[uint(catIndex)];
    }
    return cats;
}

const Color *Item::defaultColor() const
{
    return (m_defaultColorIndex > -1) ? &core()->colors()[uint(m_defaultColorIndex)] : nullptr;
}

QDateTime Item::inventoryUpdated() const
{
    QDateTime dt;
    if (m_lastInventoryUpdate >= 0)
        dt.setSecsSinceEpoch(m_lastInventoryUpdate);
    return dt;
}

bool Item::hasKnownColor(const Color *col) const
{
    if (!col)
        return true;
//    return std::find(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
//                     quint16(col - core()->colors().data())) != m_knownColorIndexes.cend();
    return std::binary_search(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
                              quint16(col - core()->colors().data()));
}

const QVector<const Color *> Item::knownColors() const
{
    QVector<const Color *> result;
    for (const quint16 idx : m_knownColorIndexes)
        result << &core()->colors()[idx];
    return result;
}

uint Item::index() const
{
    return uint(this - core()->items().data());
}

const Item *Item::ConsistsOf::item() const
{
    return &core()->items().at(m_itemIndex);
}

const Color *Item::ConsistsOf::color() const
{
    return &core()->colors().at(m_colorIndex);
}

} // namespace BrickLink
