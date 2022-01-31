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

    Category() = default;
    Category(std::nullptr_t) : Category() { } // for scripting only!

private:
    uint     m_id = InvalidId;
    // 4 bytes padding here
    QString  m_name;

private:
    static bool lessThan(const Category &cat, uint id) { return cat.m_id < id; }

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Category *)
