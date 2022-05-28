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

#include "bricklink/item.h"
#include "bricklink/core.h"
#include "bricklink/lot.h"


extern int _dwords_for_appears;
extern int _qwords_for_consists;

int _dwords_for_appears  = 0;
int _qwords_for_consists = 0;


namespace BrickLink {

bool Item::lessThan(const Item &item, const std::pair<char, QByteArray> &ids)
{
    int d = (item.m_itemTypeId - ids.first);
    return d == 0 ? (item.m_id.compare(ids.second) < 0) : (d < 0);
}

// color-idx -> { vector < qty, item-idx > }
void Item::setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash)
{
    // we are compacting a "hash of a vector of pairs" down to a list of 32bit integers

    m_appears_in.clear();

    for (auto it = appearHash.cbegin(); it != appearHash.cend(); ++it) {
        const auto &colorVector = it.value();

        m_appears_in.push_back({ it.key() /*colorIndex*/, uint(colorVector.size()) /*vectorSize*/ });

        for (auto vecIt = colorVector.cbegin(); vecIt != colorVector.cend(); ++vecIt)
            m_appears_in.push_back({ uint(vecIt->first) /*quantity*/, vecIt->second /*itemIndex*/ });
    }
    m_appears_in.shrink_to_fit();

    _dwords_for_appears += int(m_appears_in.size());
}

AppearsIn Item::appearsIn(const Color *onlyColor) const
{
    AppearsIn appearsHash;

    for (auto it = m_appears_in.cbegin(); it != m_appears_in.cend(); ) {
        // 1st level (color header):  m12: color index / m20: size of 2nd level vector
        quint32 colorIndex = it->m12;
        quint32 vectorSize = it->m20;

        ++it;
        const Color *color = &core()->colors()[colorIndex];

        if (!onlyColor || (color == onlyColor)) {
            AppearsInColor &vec = appearsHash[color];

            for (quint32 i = 0; i < vectorSize; ++i, ++it) {
                // 2nd level (color entry):   m12: quantity / m20: item index

                int quantity = int(it->m12);
                quint32 itemIndex = it->m20;

                const Item *item = &core()->items()[itemIndex];

                if (quantity)
                    vec.append(qMakePair(quantity, item));
            }
        } else {
            it += vectorSize; // skip 2nd level
        }
    }
    return appearsHash;
}

void Item::setConsistsOf(const QVector<Item::ConsistsOf> &items)
{
    m_consists_of = items;
    _qwords_for_consists += m_consists_of.size();
}

const QVector<Item::ConsistsOf> &Item::consistsOf() const
{
    return m_consists_of;
}

const ItemType *Item::itemType() const
{
    return (m_itemTypeIndex > -1) ? &core()->itemTypes()[uint(m_itemTypeIndex)] : nullptr;
}

const Category *Item::category() const
{
    return (m_categoryIndex > -1) ? &core()->categories()[uint(m_categoryIndex)] : nullptr;
}

const QVector<const Category *> Item::additionalCategories(bool includeMainCategory) const
{
    QVector<const Category *> cats;
    if (includeMainCategory && (m_categoryIndex > -1))
        cats << &core()->categories()[uint(m_categoryIndex)];
    for (const auto catIndex : m_additionalCategoryIndexes) {
        if (catIndex > -1)
            cats << &core()->categories()[uint(catIndex)];
    }
    return cats;
}

const Color *Item::defaultColor() const
{
    return (m_defaultColorIndex > -1) ? &core()->colors()[uint(m_defaultColorIndex)] : nullptr;
}

QDateTime Item::inventoryUpdated() const
{
    QDateTime dt;
    if (m_lastInventoryUpdate >= 0)
        dt.setSecsSinceEpoch(m_lastInventoryUpdate);
    return dt;
}

bool Item::hasKnownColor(const Color *col) const
{
    if (!col)
        return true;
//    return std::find(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
//                     quint16(col - core()->colors().data())) != m_knownColorIndexes.cend();
    return std::binary_search(m_knownColorIndexes.cbegin(), m_knownColorIndexes.cend(),
                              quint16(col - core()->colors().data()));
}

const QVector<const Color *> Item::knownColors() const
{
    QVector<const Color *> result;
    for (const quint16 idx : m_knownColorIndexes)
        result << &core()->colors()[idx];
    return result;
}

uint Item::index() const
{
    return uint(this - core()->items().data());
}

const Item *Item::ConsistsOf::item() const
{
    return &core()->items().at(m_itemIndex);
}

const Color *Item::ConsistsOf::color() const
{
    return &core()->colors().at(m_colorIndex);
}

bool Item::ConsistsOf::isSimple(const QVector<ConsistsOf> &parts)
{
    for (const auto &co : parts) {
        if (co.isExtra() || co.isCounterPart() || co.isAlternate())
            return false;
    }
    return true;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Item
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink item.

    Each item in the BrickLink catalog is available as an Item object.

    You cannot create Item objects yourself, but you can retrieve an Item object given the
    id via BrickLink::item().

    See \l https://www.bricklink.com/catalog.asp
*/
/*! \qmlproperty bool Item::isNull
    \readonly
    Returns whether this Item is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Item::id
    \readonly
    The BrickLink id of this item.
*/
/*! \qmlproperty string Item::name
    \readonly
    The BrickLink name of this item.
*/
/*! \qmlproperty ItemType Item::itemType
    \readonly
    The BrickLink item type of this item.
*/
/*! \qmlproperty Category Item::category
    \readonly
    The BrickLink category of this item.
*/
/*! \qmlproperty bool Item::hasInventory
    \readonly
    Returns \c true if a valid inventory exists for this item, or \c false otherwise.
*/
/*! \qmlproperty date Item::inventoryUpdated
    \readonly
    Holds the time stamp of the last successful update of the item's inventory.
*/
/*! \qmlproperty Color Item::defaultColor
    \readonly
    Returns the default color used by BrickLink to display a large picture for this item.
*/
/*! \qmlproperty real Item::weight
    \readonly
    Returns the weight of this item in gram.
*/
/*! \qmlproperty date Item::yearReleased
    \readonly
    Returns the year this item was first released.
*/
/*! \qmlproperty list<Color> Item::knownColors
    \readonly
    Returns a list of \l Color objects, containing all the colors the item is known to exist in.
    \note An item might still exist in more colors than returned here: BrickStore is deriving this
          data by looking at all the known inventories and PCCs (part-color-codes).
*/
/*! \qmlmethod bool Item::hasKnownColor(Color color)
    Returns \c true if this item is known to exist in the given \a color, or \c false otherwise.
    \sa knownColors
*/

QmlItem::QmlItem(const Item *item)
    : QmlWrapperBase(item)
{ }

QString QmlItem::id() const
{
    return QString::fromLatin1(wrapped->id());
}

bool QmlItem::hasKnownColor(QmlColor color) const
{
    return wrapped->hasKnownColor(color.wrappedObject());
}

QVariantList QmlItem::knownColors() const
{
    auto known = wrapped->knownColors();
    QVariantList result;
    for (auto c : known)
        result.append(QVariant::fromValue(QmlColor { c }));
    return result;
}

QVariantList QmlItem::consistsOf() const
{
    const auto consists = wrapped->consistsOf();
    QVariantList result;
    for (const auto &co : consists) {
        auto *lot = new Lot { co.color(), co.item() };
        lot->setAlternate(co.isAlternate());
        lot->setAlternateId(co.alternateId());
        lot->setCounterPart(co.isCounterPart());
        if (co.isExtra())
            lot->setStatus(Status::Extra);
        result << QVariant::fromValue(QmlLot::create(std::move(lot)));
    }
    return result;
}

} // namespace BrickLink
