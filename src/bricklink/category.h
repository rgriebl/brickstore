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

#include "global.h"


namespace BrickLink {

class Category
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const       { return m_id; }
    QString name() const  { return m_name; }

    int yearReleased() const     { return m_year_from ? m_year_from + 1900 : 0; }
    int yearLastProduced() const { return m_year_to ? m_year_to + 1900 : 0; }
    int yearRecency() const      { return m_year_recency ? m_year_recency + 1900 : 0; }

    Category() = default;
    Category(std::nullptr_t) : Category() { } // for scripting only!

private:
    uint     m_id = InvalidId;
    quint8   m_year_from = 0;
    quint8   m_year_to = 0;
    quint8   m_year_recency = 0;
    // 1 byte padding
    QString  m_name;

private:
    static bool lessThan(const Category &cat, uint id) { return cat.m_id < id; }

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Category *)
