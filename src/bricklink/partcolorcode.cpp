// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/color.h"
#include "bricklink/partcolorcode.h"


const BrickLink::Item *BrickLink::PartColorCode::item() const
{
    return (m_itemIndex > -1) ? &core()->items()[uint(m_itemIndex)] : nullptr;
}

const BrickLink::Color *BrickLink::PartColorCode::color() const
{
    return (m_colorIndex > -1) ? &core()->colors()[uint(m_colorIndex)] : nullptr;
}
