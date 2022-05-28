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
#include <QRegularExpression>
#include <QStringBuilder>
#include <QQmlInfo>

#include "bricklink/core.h"
#include "bricklink/lot.h"
#include "bricklink/picture.h"


namespace BrickLink {

Lot::Lot(const Color *color, const Item *item)
    : m_item(item)
    , m_color(color)
{ }

Lot::Lot(const Lot &copy)
{
    *this = copy;
}

Lot &Lot::operator=(const Lot &copy)
{
    if (this == &copy)
        return *this;

    m_item                = copy.m_item;
    m_color               = copy.m_color;

    m_incomplete.reset(copy.m_incomplete ? new Incomplete(*copy.m_incomplete.get())
                                         : nullptr);

    m_status              = copy.m_status;
    m_condition           = copy.m_condition;
    m_scondition          = copy.m_scondition;
    m_retain              = copy.m_retain;
    m_stockroom           = copy.m_stockroom;
    m_alternate           = copy.m_alternate;
    m_alt_id              = copy.m_alt_id;
    m_cpart               = copy.m_cpart;
    m_lot_id              = copy.m_lot_id;
    m_reserved            = copy.m_reserved;
    m_comments            = copy.m_comments;
    m_remarks             = copy.m_remarks;
    m_quantity            = copy.m_quantity;
    m_bulk_quantity       = copy.m_bulk_quantity;
    m_tier_quantity[0]    = copy.m_tier_quantity[0];
    m_tier_quantity[1]    = copy.m_tier_quantity[1];
    m_tier_quantity[2]    = copy.m_tier_quantity[2];
    m_sale                = copy.m_sale;
    m_price               = copy.m_price;
    m_cost                = copy.m_cost;
    m_tier_price[0]       = copy.m_tier_price[0];
    m_tier_price[1]       = copy.m_tier_price[1];
    m_tier_price[2]       = copy.m_tier_price[2];
    m_weight              = copy.m_weight;
    m_markerText          = copy.m_markerText;
    m_markerColor         = copy.m_markerColor;
    m_dateAdded           = copy.m_dateAdded;
    m_dateLastSold        = copy.m_dateLastSold;

    return *this;
}

bool Lot::operator!=(const Lot &cmp) const
{
    return !operator==(cmp);
}

void Lot::setItem(const Item *i)
{
    m_item = i;

    if (m_item && m_color && m_incomplete)
        m_incomplete.reset();
}

void Lot::setColor(const Color *c)
{
    m_color = c;

    if (m_item && m_color && m_incomplete)
        m_incomplete.reset();
}

bool Lot::operator==(const Lot &cmp) const
{
    return (!m_incomplete && !cmp.m_incomplete)
            && (m_item             == cmp.m_item)
            && (m_color            == cmp.m_color)
            && (m_status           == cmp.m_status)
            && (m_condition        == cmp.m_condition)
            && (m_scondition       == cmp.m_scondition)
            && (m_retain           == cmp.m_retain)
            && (m_stockroom        == cmp.m_stockroom)
            && (m_lot_id           == cmp.m_lot_id)
            && (m_reserved         == cmp.m_reserved)
            && (m_comments         == cmp.m_comments)
            && (m_remarks          == cmp.m_remarks)
            && (m_quantity         == cmp.m_quantity)
            && (m_bulk_quantity    == cmp.m_bulk_quantity)
            && (m_tier_quantity[0] == cmp.m_tier_quantity[0])
            && (m_tier_quantity[1] == cmp.m_tier_quantity[1])
            && (m_tier_quantity[2] == cmp.m_tier_quantity[2])
            && (m_sale             == cmp.m_sale)
            && qFuzzyCompare(m_price,         cmp.m_price)
            && qFuzzyCompare(m_cost,          cmp.m_cost)
            && qFuzzyCompare(m_tier_price[0], cmp.m_tier_price[0])
            && qFuzzyCompare(m_tier_price[1], cmp.m_tier_price[1])
            && qFuzzyCompare(m_tier_price[2], cmp.m_tier_price[2])
            && qFuzzyCompare(m_weight,        cmp.m_weight)
            && (m_markerText       == cmp.m_markerText)
            && (m_markerColor      == cmp.m_markerColor)
            && (m_dateAdded        == cmp.m_dateAdded)
            && (m_dateLastSold     == cmp.m_dateLastSold);
}

Lot::~Lot()
{ }

bool Lot::mergeFrom(const Lot &from, bool useCostQtyAg)
{
    if ((&from == this) ||
        (from.isIncomplete() || isIncomplete()) ||
        (from.item() != item()) ||
        (from.color() != color()) ||
        (from.condition() != condition()) ||
        (from.subCondition() != subCondition()))
        return false;

    if (useCostQtyAg) {
        int fromQty = std::abs(from.quantity());
        int toQty = std::abs(quantity());

        if ((fromQty == 0) && (toQty == 0))
            fromQty = toQty = 1;

        setCost((cost() * toQty + from.cost() * fromQty) / (toQty + fromQty));
    } else if (!qFuzzyIsNull(from.cost()) && qFuzzyIsNull(cost())) {
        setCost(from.cost());
    }
    setQuantity(quantity() + from.quantity());

    if (!qFuzzyIsNull(from.price()) && qFuzzyIsNull(price()))
        setPrice(from.price());
    if ((from.bulkQuantity() != 1) && (bulkQuantity() == 1))
        setBulkQuantity(from.bulkQuantity());
    if ((from.sale()) && !sale())
        setSale(from.sale());

    for (int i = 0; i < 3; i++) {
        if (!qFuzzyIsNull(from.tierPrice(i)) && qFuzzyIsNull(tierPrice(i)))
            setTierPrice(i, from.tierPrice(i));
        if (from.tierQuantity(i) && !tierQuantity(i))
            setTierQuantity(i, from.tierQuantity(i));
    }

    if (!from.remarks().isEmpty() && !remarks().isEmpty() && (from.remarks() != remarks())) {
        QRegularExpression fromRe { u"\\b" % QRegularExpression::escape(from.remarks()) % u"\\b" };

        if (!fromRe.match(remarks()).hasMatch()) {
            QRegularExpression thisRe { u"\\b" % QRegularExpression::escape(remarks()) % u"\\b" };

            if (thisRe.match(from.remarks()).hasMatch())
                setRemarks(from.remarks());
            else
                setRemarks(remarks() % u" " % from.remarks());
        }
    } else if (!from.remarks().isEmpty()) {
        setRemarks(from.remarks());
    }

    if (!from.comments().isEmpty() && !comments().isEmpty() && (from.comments() != comments())) {
        QRegularExpression fromRe { u"\\b" % QRegularExpression::escape(from.comments()) % u"\\b" };

        if (!fromRe.match(comments()).hasMatch()) {
            QRegularExpression thisRe { u"\\b" % QRegularExpression::escape(comments()) % u"\\b" };

            if (thisRe.match(from.comments()).hasMatch())
                setComments(from.comments());
            else
                setComments(comments() % u" " % from.comments());
        }
    } else if (!from.comments().isEmpty()) {
        setComments(from.comments());
    }

    if (!from.reserved().isEmpty() && reserved().isEmpty())
        setReserved(from.reserved());

    //TODO: add marker or remove this completely

    if ((from.dateAdded().isValid()) && !dateAdded().isValid())
        setDateAdded(from.dateAdded());
    if ((from.dateLastSold().isValid()) && !dateLastSold().isValid())
        setDateLastSold(from.dateLastSold());

    return true;
}

void Lot::save(QDataStream &ds) const
{
    ds << QByteArray("II") << qint32(4)
       << QString::fromLatin1(itemId())
       << qint8(itemType() ? itemType()->id() : ItemType::InvalidId)
       << uint(color() ? color()->id() : Color::InvalidId)
       << qint8(m_status) << qint8(m_condition) << qint8(m_scondition) << qint8(m_retain ? 1 : 0)
       << qint8(m_stockroom) << m_lot_id << m_reserved << m_comments << m_remarks
       << m_quantity << m_bulk_quantity
       << m_tier_quantity[0] << m_tier_quantity[1] << m_tier_quantity[2]
       << m_sale << m_price << m_cost
       << m_tier_price[0] << m_tier_price[1] << m_tier_price[2]
       << m_weight
       << m_markerText << m_markerColor
       << m_dateAdded << m_dateLastSold;
}

Lot *Lot::restore(QDataStream &ds)
{
    std::unique_ptr<Lot> lot;

    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version < 2) || (version > 4))
        return nullptr;

    QString itemid;
    uint colorid = 0;
    qint8 itemtypeid = 0;

    ds >> itemid >> itemtypeid >> colorid;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    auto item = core()->item(itemtypeid, itemid.toLatin1());
    auto color = core()->color(colorid);
    std::unique_ptr<Incomplete> inc;

    if (!item || !color) {
        inc.reset(new Incomplete);
        if (!item) {
            inc->m_item_id = itemid.toLatin1();
            inc->m_itemtype_id = itemtypeid;
        }
        if (!color) {
            inc->m_color_id = colorid;
            inc->m_color_name = QString::number(colorid);
        }

        if (core()->applyChangeLog(item, color, inc.get()))
            inc.reset();
    }
    lot.reset(new Lot(color, item));
    if (inc)
        lot->setIncomplete(inc.release());

    // alternate, cpart and altid are left out on purpose!

    qint8 status = 0, cond = 0, scond = 0, retain = 0, stockroom = 0;
    ds >> status >> cond >> scond >> retain >> stockroom
            >> lot->m_lot_id >> lot->m_reserved >> lot->m_comments >> lot->m_remarks
            >> lot->m_quantity >> lot->m_bulk_quantity
            >> lot->m_tier_quantity[0] >> lot->m_tier_quantity[1] >> lot->m_tier_quantity[2]
            >> lot->m_sale >> lot->m_price >> lot->m_cost
            >> lot->m_tier_price[0] >> lot->m_tier_price[1] >> lot->m_tier_price[2]
            >> lot->m_weight;
    if (version >= 3)
        ds >> lot->m_markerText >> lot->m_markerColor;
    if (version >= 4)
        ds >> lot->m_dateAdded >> lot->m_dateLastSold;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    lot->m_status = static_cast<Status>(status);
    lot->m_condition = static_cast<Condition>(cond);
    lot->m_scondition = static_cast<SubCondition>(scond);
    lot->m_retain = (retain);
    lot->m_stockroom = static_cast<Stockroom>(stockroom);

    return lot.release();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype Lot
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represent a lot in a document.

    A Lot corresponds to a row in a BrickStore document.
*/
/*! \qmlproperty bool Lot::isNull
    \readonly
    Returns whether this Lot is \c null. Since this type is a value wrapper around a C++ object, we
    cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item Lot::item
    The \l Item represented by this lot. Can be a BrickLink::noItem, if there's no item set.
*/
/*! \qmlproperty Color Lot::color
    The \l Color selected for this lot. Can be BrickLink::noColor, if there's no color set.
*/
/*! \qmlproperty Category Lot::category
    \readonly
    The \l Category of the lot's item or BrickLink::noCategory, if the lot's item is not valid.
*/
/*! \qmlproperty ItemType Lot::itemType
    \readonly
    The ItemType of the lot's item or BrickLink::noItemType, if the lot's item is not valid.
*/
/*! \qmlproperty string Lot::itemId
    \readonly
    The id of the lot's item. The same as \c{item.id}, but you don't have to check for isNull on
    \c item.
*/
/*! \qmlproperty string Lot::id
    \readonly
    Obsolete. Please use itemId instead.
*/
/*! \qmlproperty string Lot::itemName
    \readonly
    The name of the lot's item. The same as \c{item.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty string Lot::name
    \readonly
    Obsolete. Please use itemName instead.
*/
/*! \qmlproperty string Lot::colorName
    \readonly
    The color name of the lot's item. The same as \c{color.name}, but you don't have to check for
    isNull on \c color.
*/
/*! \qmlproperty string Lot::categoryName
    \readonly
    The category name of the lot's item. The same as \c{item.category.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty string Lot::itemTypeName
    \readonly
    The item-type name of the lot's item. The same as \c{item.itemType.name}, but you don't have to
    check for isNull on \c item.
*/
/*! \qmlproperty int Lot::itemYearReleased
    \readonly
    The year the lot's item was first released.
*/

/*! \qmlproperty Status Lot::status
    Represents the status of this lot. The Status enumeration has these values:
    \value BrickLink.Include   The green checkmark in the UI.
    \value BrickLink.Exclude   The red stop sign in the UI.
    \value BrickLink.Extra     The blue plus sign in the UI.
*/
/*! \qmlproperty Condition Lot::condition
    Describes the condition of this lot. The Condition enumeration has these values:
    \value BrickLink.New    The items in this lot are new.
    \value BrickLink.Used   The items in this lot are used.
*/
/*! \qmlproperty SubCondition Lot::subCondition
    Describes the sub-condition of this lot, if it represents a set. The SubCondition enumeration
    has these values:
    \value BrickLink.None         No sub-condition is set.
    \value BrickLink.Complete     The set is complete.
    \value BrickLink.Incomplete   The set is not complete.
    \value BrickLink.Sealed       The set is still sealed.
*/

/*! \qmlproperty string Lot::comments
    The comment or description for this lot. This is the public text that a buyer can see.
*/
/*! \qmlproperty string Lot::remarks
    The remark is the private text that only the seller can see.
*/

/*! \qmlproperty int Lot::quantity
    The quantity of the item.
*/
/*! \qmlproperty int Lot::bulkQuantity
    The bulk quantity. This lot can only be sold in multiple of this.
*/
/*! \qmlproperty int Lot::tier1Quantity
    The tier-1 quantity: if a buyer buys this quantity or more, the price will be tier1Price
    instead of price.
*/
/*! \qmlproperty int Lot::tier2Quantity
    The tier-2 quantity: if a buyer buys this quantity or more, the price will be tier2Price
    instead of tier1Price.
    \note This value needs to be larger than tier1Quantity.
*/
/*! \qmlproperty int Lot::tier3Quantity
    The tier-3 quantity: if a buyer buys this quantity or more, the price will be tier3Price
    instead of tier2Price.
    \note This value needs to be larger than tier2Quantity.
*/

/*! \qmlproperty real Lot::price
    The unit price of the item.
*/
/*! \qmlproperty real Lot::tier1Price
    The tier-3 price: this will be the price, if a buyer buys tier1Quantity or more parts.
    \note This value needs to be smaller than price.
*/
/*! \qmlproperty real Lot::tier2Price
    The tier-3 price: this will be the price, if a buyer buys tier2Quantity or more parts.
    \note This value needs to be smaller than tier2Price.
*/
/*! \qmlproperty real Lot::tier3Price
    The tier-3 price: this will be the price, if a buyer buys tier3Quantity or more parts.
    \note This value needs to be smaller than tier2Price.
*/

/*! \qmlproperty int Lot::sale
    The optional sale on this lots in percent. \c{[0 .. 100]}
*/
/*! \qmlproperty real Lot::total
    \readonly
    A convenience value, return price times quantity.
*/

/*! \qmlproperty uint Lot::lotId
    The BrickLink lot-id, which uniquely identifies a lot for sale on BrickLink.
*/
/*! \qmlproperty bool Lot::retain
    A boolean flag indicating whether the lot should be retained in the store's stockroom if the
    last item has been sold.
*/
/*! \qmlproperty Stockroom Lot::stockroom
    Describes if and in which stockroom this lot is located. The Stockroom enumeration has these
    values:
    \value BrickLink.None  Not in a stockroom.
    \value BrickLink.A     In stockroom \c A.
    \value BrickLink.B     In stockroom \c B.
    \value BrickLink.C     In stockroom \c C.
*/

/*! \qmlproperty real Lot::totalWeight
    The weight of the complete lot, i.e. quantity times the weight of a single item.
*/
/*! \qmlproperty string Lot::reserved
    The name of the buyer this item is reserved for or an empty string.
*/
/*! \qmlproperty bool Lot::alternate
    A boolean flag denoting this lot as an \e alternate in a set inventory.
    \note This value does not get saved.
*/
/*! \qmlproperty uint Lot::alternateId
    If this lot is an \e alternate in a set inventory, this property holds the \e{alternate id}.
    \note This value does not get saved.
*/
/*! \qmlproperty bool Lot::counterPart
    A boolean flag denoting this lot as a \e{counter part} in a set inventory.
    \note This value does not get saved.
*/

/*! \qmlproperty bool Lot::incomplete
    \readonly
    Returns \c false if this lot has a valid item and color, or \c true otherwise.
*/

/*! \qmlproperty image Lot::image
    \readonly
    The item's image in the lot's color; can be \c null.
    \note The image isn't readily available, but needs to be asynchronously loaded (or even
          downloaded) at runtime. See the Picture type for more information.
*/

QmlLot::QmlLot(Lot *lot, ::QmlDocumentLots *documentLots)
    : QmlWrapperBase(lot)
    , m_documentLots(documentLots)
{ }

QmlLot::QmlLot(const QmlLot &copy)
    : QmlLot(quintptr(copy.m_documentLots) == Owning
             ? new Lot(*copy.wrappedObject())
             : copy.wrappedObject(), copy.m_documentLots)
{ }

QmlLot::QmlLot(QmlLot &&move)
    : QmlWrapperBase(move)
{
    std::swap(m_documentLots, move.m_documentLots);
}

QmlLot::~QmlLot()
{
    if ((quintptr(m_documentLots) == Owning) && !isNull())
        delete wrappedObject();
}

QmlLot QmlLot::create(Lot *&&lot)
{
    return QmlLot(std::move(lot), reinterpret_cast<::QmlDocumentLots *>(Owning));
}

QmlLot &QmlLot::operator=(const QmlLot &assign)
{
    if (this != &assign) {
        this->~QmlLot();
        new (this) QmlLot(assign);
    }
    return *this;
}

QImage QmlLot::image() const
{
    static QImage dummy;
    auto pic = core()->picture(get()->item(), get()->color(), true);
    return pic ? pic->image() : dummy;
}

QmlLot::Setter::Setter(QmlLot *lot)
    : m_lot((lot && !lot->isNull()) ? lot : nullptr)
{
    if (m_lot)
        m_to = *m_lot->wrapped;
}

Lot *QmlLot::Setter::to()
{
    return &m_to;
}

QmlLot::Setter::~Setter()
{
    if (!m_lot->m_documentLots) {
        qmlWarning(nullptr) << "Cannot modify a const Lot";
        return;
    }

    if (m_lot && (*m_lot->wrapped != m_to)) {
        if (m_lot->m_documentLots)
            doChangeLot(m_lot->m_documentLots, m_lot->wrapped, m_to);
        else
            *m_lot->wrapped = m_to;
    }
}

QmlLot::Setter QmlLot::set()
{
    return Setter(this);
}

Lot *QmlLot::get() const
{
    return wrapped;
}

} // namespace BrickLink
