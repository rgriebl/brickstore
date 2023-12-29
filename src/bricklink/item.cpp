// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "bricklink/item.h"
#include "bricklink/core.h"


namespace BrickLink {

AppearsIn Item::appearsIn(const Color *onlyColor) const
{
    AppearsIn appearsHash;

    for (auto it = m_appears_in.cbegin(); it != m_appears_in.cend(); ) {
        // 1st level (color header)
        quint32 vectorSize = it->m_colorBits.m_colorSize;
        const Color *color = &core()->colors()[it->m_colorBits.m_colorIndex];

        ++it;

        if (!onlyColor || (color == onlyColor)) {
            AppearsInColor &vec = appearsHash[color];

            for (quint32 i = 0; i < vectorSize; ++i, ++it) {
                // 2nd level (color entry)
                const Item *item = &core()->items()[it->m_itemBits.m_itemIndex];

                if (quint32 qty = it->m_itemBits.m_quantity)
                    vec.append(qMakePair(qty, item)); // clazy:exclude=reserve-candidates
            }
        } else {
            it += vectorSize; // skip 2nd level
        }
    }
    return appearsHash;
}

std::span<const Item::ConsistsOf, std::dynamic_extent> Item::consistsOf() const
{
    return { m_consists_of.cbegin(), m_consists_of.cend() };
}

QVector<const RelationshipMatch *> Item::relationshipMatches() const
{
    QVector<const RelationshipMatch *> matches;
    for (const uint relMatchId : m_relationshipMatchIds) {
        if (auto m = core()->relationshipMatch(relMatchId))
            matches << m;
    }
    return matches;
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

std::span<const Item::PCC, std::dynamic_extent> Item::pccs() const
{
    return { m_pccs.cbegin(), m_pccs.cend() };
}

const Color *Item::hasPCC(uint pcc) const
{
    auto it = std::lower_bound(m_pccs.cbegin(), m_pccs.cend(), pcc, [](const auto &p1, uint p2) {
        return p1.pcc() < p2;
    });
    if ((it != m_pccs.end()) && (it->pcc() == pcc))
        return &core()->colors()[it->m_colorIndex];
    return nullptr;
}

char Item::itemTypeId() const
{
    if (auto itt = itemType())
        return itt->id();
    return 0;
}

const ItemType *Item::itemType() const
{
    return (m_itemTypeIndex != 0xf) ? &core()->itemTypes()[uint(m_itemTypeIndex)] : nullptr;
}

const Category *Item::category() const
{
    return m_categoryIndexes.size() ? &core()->categories()[uint(m_categoryIndexes[0])] : nullptr;
}

const QVector<const Category *> Item::categories(bool includeMainCategory) const
{
    QVector<const Category *> cats;
    auto it = m_categoryIndexes.cbegin();
    if (!includeMainCategory && (it != m_categoryIndexes.cend()))
        ++it;
    for ( ; it != m_categoryIndexes.cend(); ++it)
        cats << &core()->categories()[uint(*it)];
    return cats;
}

const Color *Item::defaultColor() const
{
    return (m_defaultColorIndex != 0xfff) ? &core()->colors()[uint(m_defaultColorIndex)] : nullptr;
}

std::span<const Dimensions, std::dynamic_extent> Item::dimensions() const
{
    return { m_dimensions.cbegin(), m_dimensions.cend() };
}

bool Item::hasKnownColor(const Color *col) const
{
    if (!col)
        return true;
    int index = quint16(col - core()->colors().data());
    for (const auto &colIdx : m_knownColorIndexes) {
        if (colIdx == index)
            return true;
    }
    return false;
}

QVector<const Color *> Item::knownColors() const
{
    QVector<const Color *> result;
    if (itemType() && itemType()->hasColors()) {
        result.reserve(m_knownColorIndexes.size());
        for (const quint16 idx : m_knownColorIndexes)
            result << &core()->colors()[idx];
    } else {
        result.reserve(1);
        result << core()->color(0);
    }
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

const Color *Item::PCC::color() const
{
    return &core()->colors().at(m_colorIndex);
}

} // namespace BrickLink
