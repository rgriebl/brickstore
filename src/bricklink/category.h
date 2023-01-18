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

#include <QtCore/QMetaType>
#include <QtCore/QString>


namespace BrickLink {

class ItemType;

class Category
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const       { return m_id; }
    QString name() const  { return m_name; }

    bool hasInventories() const  { return (m_has_inventories); }
    bool hasInventories(const ItemType *itemType) const;
    int yearReleased() const     { return m_year_from ? m_year_from + 1900 : 0; }
    int yearLastProduced() const { return m_year_to ? m_year_to + 1900 : 0; }
    int yearRecency() const      { return m_year_recency ? m_year_recency + 1900 : 0; }

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
    QString  m_name;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Category *)
