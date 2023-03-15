// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>

#include <QString>


namespace BrickLink {

class Relationship
{
public:
    uint id() const       { return m_id; }
    QString name() const  { return m_name; }
    uint count() const    { return m_count; }

    explicit Relationship() = default;
    explicit Relationship(uint id, const QString &name);

    std::weak_ordering operator<=>(uint id) const;
    std::weak_ordering operator<=>(const Relationship &other) const;

private:
    uint m_id = 0;
    uint m_count = 0;
    QString m_name;

    friend class TextImport;
    friend class Database;
};

class RelationshipMatch
{
public:
    uint id() const              { return m_id; }
    uint relationshipId() const  { return m_relationshipId; }
    const std::vector<uint> &itemIndexes() const { return m_itemIndexes; }

    explicit RelationshipMatch() = default;
    explicit RelationshipMatch(uint id, uint relationshipId, std::vector<uint> &&itemIndexes);

    std::weak_ordering operator<=>(uint id) const;
    std::weak_ordering operator<=>(const RelationshipMatch &other) const;

private:
    uint m_id = 0;
    uint m_relationshipId = 0;
    std::vector<uint> m_itemIndexes;

    friend class TextImport;
    friend class Database;
};

} // namespace BrickLink
