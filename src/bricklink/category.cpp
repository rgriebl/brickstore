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


namespace BrickLink {

/*! \qmltype Category
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item category.

    Each category in the BrickLink catalog is available as a Category object.

    You cannot create Category objects yourself, but you can retrieve a Category object given the
    id via BrickLink::category().
    Each Item also has a read-only property Item::category.

    See \l https://www.bricklink.com/catalogCategory.asp
*/
/*! \qmlproperty bool Category::isNull
    \readonly
    Returns whether this Category is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Category::id
    \readonly
    The BrickLink id of this category.
*/
/*! \qmlproperty string Category::name
    \readonly
    The BrickLink name of this category.
*/

QmlCategory::QmlCategory(const Category *cat)
    : QmlWrapperBase(cat)
{ }

} // namespace BrickLink
