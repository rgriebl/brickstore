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
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDate>
#include <QDebug>

#include "color.h"


namespace BrickLink {

class ColorChangeLogEntry
{
public:
    uint fromColorId() const  { return m_fromColorId; }
    uint toColorId() const  { return m_toColorId; }

    explicit ColorChangeLogEntry(uint fromColorId = Color::InvalidId, uint toColorId = Color::InvalidId,
                                 QDate date = { })

        : m_fromColorId(fromColorId)
        , m_toColorId(toColorId)
        , m_date(date)
    { }

    constexpr std::weak_ordering operator<=>(uint colorId) const { return m_fromColorId <=> colorId; }
    constexpr std::weak_ordering operator<=>(const ColorChangeLogEntry &other) const { return *this <=> other.m_fromColorId; }
    constexpr bool operator==(uint colorId) const { return (*this <=> colorId == 0); }
    constexpr bool operator==(const ColorChangeLogEntry &other) const { return *this == other.m_fromColorId; }

private:
    uint m_fromColorId;
    uint m_toColorId;
    QDate m_date; // only used when rebuilding the DB

    friend class Core;
    friend class Database;
};


class ItemChangeLogEntry
{
public:
    char fromItemTypeId() const    { return m_fromTypeAndId.at(0); }
    QByteArray fromItemId() const  { return m_fromTypeAndId.mid(1); }

    char toItemTypeId() const    { return m_toTypeAndId.at(0); }
    QByteArray toItemId() const  { return m_toTypeAndId.mid(1); }

    QDate date() const  { return m_date; }

    explicit ItemChangeLogEntry(const QByteArray &fromTypeAndId = { }, const QByteArray &toTypeAndId = { },
                                QDate date = { })
        : m_fromTypeAndId(fromTypeAndId)
        , m_toTypeAndId(toTypeAndId)
        , m_date(date)
    { }

    std::weak_ordering operator<=>(const QByteArray &typeAndId) const { return m_fromTypeAndId.compare(typeAndId) <=> 0; }
    std::weak_ordering operator<=>(const ItemChangeLogEntry &other) const { return *this <=> other.m_fromTypeAndId; }
    bool operator==(const QByteArray &typeAndId) const { return (*this <=> typeAndId == 0); }
    bool operator==(const ItemChangeLogEntry &other) const { return *this == other.m_fromTypeAndId; }

private:
    QByteArray m_fromTypeAndId;
    QByteArray m_toTypeAndId;
    QDate m_date; // only used when rebuilding the DB

    friend class Core;
    friend class Database;
};

} // namespace BrickLink
