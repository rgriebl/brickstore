// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <memory>
#include <QRegularExpression>

#include "bricklink/core.h"
#include "bricklink/lot.h"


namespace BrickLink {

Lot::Lot(const Item *item, const Color *color)
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

void Lot::save(QDataStream &ds) const
{
    ds << QByteArray("II") << qint32(5)
       << itemId()
       << (itemType() ? itemType()->id() : ItemType::InvalidId)
       << (color() ? color()->id() : Color::InvalidId)
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

Lot *Lot::restore(QDataStream &ds, uint startChangelogAt)
{
    std::unique_ptr<Lot> lot;

    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version != 5))
        return nullptr;

    QByteArray itemId;
    uint colorId = 0;
    char itemTypeId = 0;

    ds >> itemId >> itemTypeId >> colorId;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    if (startChangelogAt) {
        const QByteArray itemTypeAndId = itemTypeId + itemId;
        QByteArray newId = core()->applyItemChangeLog(itemTypeAndId, startChangelogAt, { });
        if (newId != itemTypeAndId) {
            itemTypeId = newId.at(0);
            itemId = newId.mid(1);
        }
        colorId = core()->applyColorChangeLog(colorId, startChangelogAt, { });
    }

    auto item = core()->item(itemTypeId, itemId);
    auto color = core()->color(colorId);

    lot = std::make_unique<Lot>(item, color);
    if (!item || !color) {
        auto *inc = new Incomplete;
        if (!item) {
            inc->m_item_id = itemId;
            inc->m_itemtype_id = itemTypeId;
        }
        if (!color) {
            inc->m_color_id = colorId;
            inc->m_color_name = u"BL #"_qs + QString::number(colorId);
        }
        lot->setIncomplete(inc);
    }

    // alternate, cpart and altid are left out on purpose!

    qint8 status = 0, cond = 0, scond = 0, retain = 0, stockroom = 0;
    ds >> status >> cond >> scond >> retain >> stockroom
        >> lot->m_lot_id >> lot->m_reserved >> lot->m_comments >> lot->m_remarks
        >> lot->m_quantity >> lot->m_bulk_quantity
        >> lot->m_tier_quantity[0] >> lot->m_tier_quantity[1] >> lot->m_tier_quantity[2]
        >> lot->m_sale >> lot->m_price >> lot->m_cost
        >> lot->m_tier_price[0] >> lot->m_tier_price[1] >> lot->m_tier_price[2]
        >> lot->m_weight >> lot->m_markerText >> lot->m_markerColor
        >> lot->m_dateAdded >> lot->m_dateLastSold;

    if (ds.status() != QDataStream::Ok)
        return nullptr;

    lot->m_status = static_cast<Status>(status);
    lot->m_condition = static_cast<Condition>(cond);
    lot->m_scondition = static_cast<SubCondition>(scond);
    lot->m_retain = (retain);
    lot->m_stockroom = static_cast<Stockroom>(stockroom);

    return lot.release();
}

} // namespace BrickLink
