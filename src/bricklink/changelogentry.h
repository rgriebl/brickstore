// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDate>

#include "bricklink/color.h"
#include "utility/pooledarray.h"


namespace BrickLink {

class ColorChangeLogEntry
{
public:
    uint id() const          { return m_id; }
    QDate date() const       { return QDate::fromJulianDay(m_julianDay); }
    uint fromColorId() const { return m_fromColorId; }
    uint toColorId() const   { return m_toColorId; }

    explicit ColorChangeLogEntry() = default;

    std::weak_ordering operator<=>(uint colorId) const;
    std::weak_ordering operator<=>(const ColorChangeLogEntry &other) const;

private:
    uint m_id = 0;
    uint m_julianDay = 0;
    uint m_fromColorId = Color::InvalidId;
    uint m_toColorId = Color::InvalidId;

    friend class Database;
    friend class TextImport;
};


inline std::weak_ordering ColorChangeLogEntry::operator<=>(uint colorId) const
{
    return m_fromColorId <=> colorId;
}

inline std::weak_ordering ColorChangeLogEntry::operator<=>(const ColorChangeLogEntry &other) const
{
    auto cmp = (m_fromColorId <=> other.m_fromColorId);
    return (cmp == 0) ? (m_id <=> other.m_id) : cmp;
}


class ItemChangeLogEntry
{
public:
    uint id() const                     { return m_id; }
    QDate date() const                  { return QDate::fromJulianDay(m_julianDay); }
    char fromItemTypeId() const         { return m_fromTypeAndId.isEmpty() ? 0 : m_fromTypeAndId[0]; }
    QByteArray fromItemId() const       { return m_fromTypeAndId.asQByteArray(1); }
    QByteArray fromItemTypeAndId() const{ return m_fromTypeAndId.asQByteArray(); }
    char toItemTypeId() const           { return m_toTypeAndId.isEmpty() ? 0 : m_toTypeAndId[0]; }
    QByteArray toItemId() const         { return m_toTypeAndId.asQByteArray(1); }
    QByteArray toItemTypeAndId() const  { return m_toTypeAndId.asQByteArray(); }

    explicit ItemChangeLogEntry() = default;

    std::weak_ordering operator<=>(const QByteArray &typeAndId) const;
    std::weak_ordering operator<=>(const ItemChangeLogEntry &other) const;

private:
    uint m_id = 0;
    uint m_julianDay = 0;
    PooledArray<char> m_fromTypeAndId;
    PooledArray<char> m_toTypeAndId;

    friend class Database;
    friend class TextImport;
};

inline std::weak_ordering ItemChangeLogEntry::operator<=>(const QByteArray &typeAndId) const
{
    return fromItemTypeAndId().compare(typeAndId) <=> 0;
}

inline std::weak_ordering ItemChangeLogEntry::operator<=>(const ItemChangeLogEntry &other) const
{
    auto cmp = (fromItemTypeAndId().compare(other.fromItemTypeAndId()) <=> 0);
    return (cmp == 0) ? (m_id <=> other.m_id) : cmp;
}

} // namespace BrickLink
