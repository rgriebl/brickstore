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

#include "bricklink/itemtype.h"
#include "bricklink/category.h"
#include "bricklink/core.h"


namespace BrickLink {

const QVector<const Category *> ItemType::categories() const
{
    QVector<const Category *> result;
    result.reserve(int(m_categoryIndexes.size()));
    for (const quint16 idx : m_categoryIndexes)
        result << &core()->categories()[idx];
    return result;
}

QSize ItemType::pictureSize() const
{
    QSize s = rawPictureSize();
    double f = core()->itemImageScaleFactor();
    if (!qFuzzyCompare(f, 1.))
        s *= f;
    return s;
}

QSize ItemType::rawPictureSize() const
{
    QSize s(80, 60);
    if (m_id == 'M')
        s.transpose();
    return s;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype ItemType
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item type.

    Each item type in the BrickLink catalog is available as an ItemType object.

    You cannot create ItemType objects yourself, but you can retrieve an ItemType object given the
    id via BrickLink::itemType().
    Each Item also has a read-only property Item::itemType.

    The currently available item types are
    \table
    \header
      \li Id
      \li Name
    \row
      \li \c B
      \li Book
    \row
      \li \c C
      \li Catalog
    \row
      \li \c G
      \li Gear
    \row
      \li \c I
      \li Instruction
    \row
      \li \c M
      \li Minifigure
    \row
      \li \c O
      \li Original Box
    \row
      \li \c P
      \li Part
    \row
      \li \c S
      \li Set
    \endtable
*/
/*! \qmlproperty bool ItemType::isNull
    \readonly
    Returns whether this ItemType is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int ItemType::id
    \readonly
    The BrickLink id of this item type.
*/
/*! \qmlproperty string ItemType::name
    \readonly
    The BrickLink name of this item type.
*/
/*! \qmlproperty list<Category> ItemType::categories
    \readonly
    A list of \l Category objects describing all the categories that are referencing at least one
    item of the given item type.
*/
/*! \qmlproperty bool ItemType::hasInventories
    \readonly
    Returns \c true if items under this type can have inventories, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasColors
    \readonly
    Returns \c true if items under this type can have colors, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasWeight
    \readonly
    Returns \c true if items under this type can have weights, or \c false otherwise.
*/
/*! \qmlproperty bool ItemType::hasSubConditions
    \readonly
    Returns \c true if items under this type can have sub-conditions, or \c false otherwise.
*/
/*! \qmlproperty size ItemType::pictureSize
    \readonly
    The default size and aspect ratio for item pictures of this type.
*/

QmlItemType::QmlItemType(const BrickLink::ItemType *itt)
    : QmlWrapperBase(itt)
{ }

QString QmlItemType::id() const
{
    return QString { QLatin1Char(wrapped->id()) };
}

QVariantList QmlItemType::categories() const
{
    auto cats = wrapped->categories();
    QVariantList result;
    for (auto cat : cats)
        result.append(QVariant::fromValue(QmlCategory { cat }));
    return result;
}

} // namespace BrickLink
