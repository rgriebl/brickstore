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

#include <QtCore/QMap>

#include "bricklink/color.h"
#include "utility/utility.h"


QString BrickLink::Color::typeName(TypeFlag t)
{
    static const QMap<TypeFlag, QString> colortypes = {
        { Solid,       "Solid"_l1 },
        { Transparent, "Transparent"_l1 },
        { Glitter,     "Glitter"_l1 },
        { Speckle,     "Speckle"_l1 },
        { Metallic,    "Metallic"_l1 },
        { Chrome,      "Chrome"_l1 },
        { Pearl,       "Pearl"_l1 },
        { Milky,       "Milky"_l1 },
        { Modulex,     "Modulex"_l1 },
        { Satin,       "Satin"_l1 },
    };
    return colortypes.value(t);
}
