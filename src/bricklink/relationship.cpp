// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "core.h"
#include "relationship.h"


namespace BrickLink {

std::weak_ordering Relationship::operator<=>(uint id) const
{
    return m_id <=> id;
}

std::weak_ordering Relationship::operator<=>(const Relationship &other) const
{
    return m_id <=> other.m_id;
}



QVector<const Item *> RelationshipMatch::items() const
{
    QVector<const Item *> result;
    result.reserve(m_itemIndexes.size());
    for (const auto &idx : m_itemIndexes)
        result.emplace_back(&core()->items()[idx]);
    return result;
}

std::weak_ordering RelationshipMatch::operator<=>(uint id) const
{
    return m_id <=> id;
}

std::weak_ordering RelationshipMatch::operator<=>(const RelationshipMatch &other) const
{
    return m_id <=> other.m_id;
}

} // namespace BrickLink
