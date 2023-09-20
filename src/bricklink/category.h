// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include "utility/pooledarray.h"


namespace BrickLink {

class ItemType;

class Category
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const       { return m_id; }
    QString name() const  { return m_name.asQString(); }

    bool hasInventories() const  { return (m_has_inventories); }
    bool hasInventories(const ItemType *itemType) const;
    int yearReleased() const     { return m_year_from ? m_year_from + 1900 : 0; }
    int yearLastProduced() const { return m_year_to ? m_year_to + 1900 : 0; }
    int yearRecency() const      { return m_year_recency ? m_year_recency + 1900 : 0; }

    uint index() const;   // only for internal use

    Category() = default;

    explicit Category(std::nullptr_t) : Category() { } // for scripting only!

    constexpr std::strong_ordering operator<=>(uint id) const { return m_id <=> id; }
    constexpr std::strong_ordering operator<=>(const Category &other) const { return *this <=> other.m_id; }
    constexpr bool operator==(uint id) const { return (*this <=> id == 0); }

private:
    uint     m_id = InvalidId;
    quint8   m_year_from = 0;
    quint8   m_year_to = 0;
    quint8   m_year_recency = 0;
    quint8   m_has_inventories = 0;
    PooledArray<char16_t> m_name;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Category *)
