// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


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

uint Category::index() const
{
    return uint(this - core()->categories().data());
}

} // namespace BrickLink
