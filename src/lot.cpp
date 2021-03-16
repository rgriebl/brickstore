/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include "bricklink.h"
#include "lot.h"


Lot::Lot(const BrickLink::Color *color, const BrickLink::Item *item)
    : m_item(item)
    , m_color(color)
{
    //TODO: replace with member initializers when switching to c++20
    m_status = BrickLink::Status::Include;
    m_condition = BrickLink::Condition::New;
    m_scondition = BrickLink::SubCondition::None;
    m_retain = false;
    m_stockroom = BrickLink::Stockroom::None;
    m_alternate = false;
    m_alt_id = 0;
    m_cpart = false;
}

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

    m_incomplete.reset(copy.m_incomplete ? new BrickLink::Incomplete(*copy.m_incomplete.get())
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

    return *this;
}

bool Lot::operator!=(const Lot &cmp) const
{
    return !operator==(cmp);
}

void Lot::setItem(const BrickLink::Item *i)
{
    m_item = i;

    if (m_item && m_color && m_incomplete)
        m_incomplete.reset();
}

void Lot::setColor(const BrickLink::Color *c)
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
            && (m_markerColor      == cmp.m_markerColor);
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
        setCost((cost() * quantity() + from.cost() * from.quantity()) / (quantity() + from.quantity()));
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

    return true;
}

void Lot::save(QDataStream &ds) const
{
    ds << QByteArray("II") << qint32(3)
       << QString::fromLatin1(itemId())
       << qint8(itemType() ? itemType()->id() : BrickLink::ItemType::InvalidId)
       << uint(color() ? color()->id() : BrickLink::Color::InvalidId)
       << qint8(m_status) << qint8(m_condition) << qint8(m_scondition) << qint8(m_retain ? 1 : 0)
       << qint8(m_stockroom) << m_lot_id << m_reserved << m_comments << m_remarks
       << m_quantity << m_bulk_quantity
       << m_tier_quantity[0] << m_tier_quantity[1] << m_tier_quantity[2]
       << m_sale << m_price << m_cost
       << m_tier_price[0] << m_tier_price[1] << m_tier_price[2]
       << m_weight
       << m_markerText << m_markerColor;
}

Lot *Lot::restore(QDataStream &ds)
{
    QScopedPointer<Lot> lot;

    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version < 2) || (version > 3))
        return nullptr;

    QString itemid;
    uint colorid = 0;
    qint8 itemtypeid = 0;

    ds >> itemid >> itemtypeid >> colorid;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    auto item = BrickLink::core()->item(itemtypeid, itemid.toLatin1());
    auto color = BrickLink::core()->color(colorid);
    QScopedPointer<BrickLink::Incomplete> inc;

    if (!item || !color) {
        inc.reset(new BrickLink::Incomplete);
        if (!item) {
            inc->m_item_id = itemid.toLatin1();
            inc->m_itemtype_id = itemtypeid;
        }
        if (!color) {
            inc->m_color_id = colorid;
            inc->m_color_name = QString::number(colorid);
        }

        if (BrickLink::core()->applyChangeLog(item, color, inc.get()))
            inc.reset();
    }
    lot.reset(new Lot(color, item));
    if (inc)
        lot->setIncomplete(inc.take());

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

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    lot->m_status = static_cast<BrickLink::Status>(status);
    lot->m_condition = static_cast<BrickLink::Condition>(cond);
    lot->m_scondition = static_cast<BrickLink::SubCondition>(scond);
    lot->m_retain = (retain);
    lot->m_stockroom = static_cast<BrickLink::Stockroom>(stockroom);

    return lot.take();
}
