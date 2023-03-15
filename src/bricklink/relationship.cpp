// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "relationship.h"

namespace BrickLink {

Relationship::Relationship(uint id, const QString &name)
    : m_id(id)
    , m_name(name)
{ }

std::weak_ordering Relationship::operator<=>(uint id) const
{
    return m_id <=> id;
}

std::weak_ordering Relationship::operator<=>(const Relationship &other) const
{
    return m_id <=> other.m_id;
}



RelationshipMatch::RelationshipMatch(uint id, uint relationshipId, std::vector<uint> &&itemIndexes)
    : m_id(id)
    , m_relationshipId(relationshipId)
    , m_itemIndexes(std::move(itemIndexes))
{ }

std::weak_ordering RelationshipMatch::operator<=>(uint id) const
{
    return m_id <=> id;
}

std::weak_ordering RelationshipMatch::operator<=>(const RelationshipMatch &other) const
{
    return m_id <=> other.m_id;
}

} // namespace BrickLink
