// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDate>
#include <QDebug>

#include "color.h"


namespace BrickLink {

class ColorChangeLogEntry
{
public:
    uint id() const          { return m_id; }
    QDate date() const       { return QDate::fromJulianDay(m_julianDay); }
    uint fromColorId() const { return m_fromColorId; }
    uint toColorId() const   { return m_toColorId; }

    explicit ColorChangeLogEntry() = default;
    explicit ColorChangeLogEntry(uint id, const QDate &date, uint fromColorId, uint toColorId)
        : m_id(id)
        , m_julianDay(uint(date.toJulianDay()))
        , m_fromColorId(fromColorId)
        , m_toColorId(toColorId)
    { }

    std::weak_ordering operator<=>(uint colorId) const;
    std::weak_ordering operator<=>(const ColorChangeLogEntry &other) const;

private:
    uint m_id = 0;
    uint m_julianDay = 0;
    uint m_fromColorId = Color::InvalidId;
    uint m_toColorId = Color::InvalidId;

    friend class Core;
    friend class Database;
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
    char fromItemTypeId() const         { return m_fromTypeAndId.at(0); }
    QByteArray fromItemId() const       { return m_fromTypeAndId.mid(1); }
    char toItemTypeId() const           { return m_toTypeAndId.at(0); }
    QByteArray toItemId() const         { return m_toTypeAndId.mid(1); }
    QByteArray toItemTypeAndId() const  { return m_toTypeAndId; }

    explicit ItemChangeLogEntry() = default;
    explicit ItemChangeLogEntry(uint id, const QDate &date, const QByteArray &fromTypeAndId,
                                const QByteArray &toTypeAndId)
        : m_id(id)
        , m_julianDay(uint(date.toJulianDay()))
        , m_fromTypeAndId(fromTypeAndId)
        , m_toTypeAndId(toTypeAndId)
    { }

    std::weak_ordering operator<=>(const QByteArray &typeAndId) const;
    std::weak_ordering operator<=>(const ItemChangeLogEntry &other) const;

private:
    uint m_id = 0;
    uint m_julianDay = 0;
    QByteArray m_fromTypeAndId;
    QByteArray m_toTypeAndId;

    friend class Core;
    friend class Database;
};

inline std::weak_ordering ItemChangeLogEntry::operator<=>(const QByteArray &typeAndId) const
{
    return m_fromTypeAndId.compare(typeAndId) <=> 0;
}

inline std::weak_ordering ItemChangeLogEntry::operator<=>(const ItemChangeLogEntry &other) const
{
    auto cmp = (m_fromTypeAndId.compare(other.m_fromTypeAndId) <=> 0);
    return (cmp == 0) ? (m_id <=> other.m_id) : cmp;
}

} // namespace BrickLink
