// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>

#include <QString>

#include "utility/pooledarray.h"


namespace BrickLink {

class Item;

class Relationship
{
public:
    static constexpr uint InvalidId = 0;

    uint id() const       { return m_id; }
    QString name() const  { return m_name.asQString(); }
    uint count() const    { return m_count; }

    explicit Relationship() = default;

    std::weak_ordering operator<=>(uint id) const;
    std::weak_ordering operator<=>(const Relationship &other) const;

private:
    uint m_id = 0;
    uint m_count = 0;
    PooledArray<char16_t> m_name;

    friend class TextImport;
    friend class Database;
};

class RelationshipMatch
{
public:
    static constexpr uint InvalidId = 0;

    uint id() const              { return m_id; }
    uint relationshipId() const  { return m_relationshipId; }
    QVector<const Item *> items() const;

    explicit RelationshipMatch() = default;

    std::weak_ordering operator<=>(uint id) const;
    std::weak_ordering operator<=>(const RelationshipMatch &other) const;

private:
    uint m_id = 0;
    uint m_relationshipId = 0;
    PooledArray<uint> m_itemIndexes;

    friend class TextImport;
    friend class Database;
};

} // namespace BrickLink
