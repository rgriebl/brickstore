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