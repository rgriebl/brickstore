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

#include "global.h"


namespace BrickLink {

class PartColorCode
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const             { return m_id; }
    const Item *item() const;
    const Color *color() const;

    PartColorCode() = default;

private:
    uint m_id = InvalidId;
    signed int m_itemIndex  : 20 = -1;
    signed int m_colorIndex : 12 = -1;

private:
    static bool lessThan(const PartColorCode &pcc, uint id) { return pcc.m_id < id; }

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink
