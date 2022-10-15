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

#include "bricklink/category.h"
#include "bricklink/itemtype.h"
#include "bricklink/core.h"


namespace BrickLink {

bool Category::hasInventories(const ItemType *itemType) const
{
    if (!itemType)
        return false;
    auto index = (itemType - core()->itemTypes().data());
    Q_ASSERT(index >= 0 && index < 8);
    return m_has_inventories & quint8(1) << index;
}

} // namespace BrickLink
