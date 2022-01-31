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
#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QDateTime>
#include <QtCore/QScopedPointer>
#include <QtGui/QColor>

#include "bricklink/global.h"
#include "bricklink/item.h"

namespace BrickLink {

class Lot
{
public:
    Lot(const Color *color = nullptr, const Item *item = nullptr);
    Lot(const Lot &copy);
    ~Lot();

    Lot &operator=(const Lot &copy);
    bool operator==(const Lot &cmp) const;
    bool operator!=(const Lot &cmp) const;

    const Item *item() const           { return m_item; }
    void setItem(const Item *i);
    const Category *category() const   { return m_item ? m_item->category() : nullptr; }
    const ItemType *itemType() const   { return m_item ? m_item->itemType() : nullptr; }
    const Color *color() const         { return m_color; }
    void setColor(const Color *c);

    QByteArray itemId() const          { return m_item ? m_item->id()
                                                       : (m_incomplete ? m_incomplete->m_item_id
                                                                       : QByteArray()); }
    QString itemName() const           { return m_item ? m_item->name()
                                                       : (m_incomplete ? m_incomplete->m_item_name
                                                                       : QString()); }
    uint colorId() const               { return m_color ? m_color->id()
                                                        : (m_incomplete ? m_incomplete->m_color_id
                                                                        : 0); }
    QString colorName() const          { return m_color ? m_color->name()
                                                        : (m_incomplete ? m_incomplete->m_color_name
                                                                        : QString()); }
    uint categoryId() const            { return category() ? category()->id()
                                                           : (m_incomplete ? m_incomplete->m_category_id
                                                                           : 0); }
    QString categoryName() const       { return category() ? category()->name()
                                                           : (m_incomplete ? m_incomplete->m_category_name
                                                                           : QString()); }
    char itemTypeId() const            { return itemType() ? itemType()->id()
                                                           : (m_incomplete ? m_incomplete->m_itemtype_id
                                                                           : 0); }
    QString itemTypeName() const       { return itemType() ? itemType()->name()
                                                           : (m_incomplete ? m_incomplete->m_itemtype_name
                                                                           : QString()); }
    int itemYearReleased() const       { return m_item ? m_item->yearReleased() : 0; }

    Status status() const              { return m_status; }
    void setStatus(Status s)           { m_status = s; }
    Condition condition() const        { return m_condition; }
    void setCondition(Condition c)     { m_condition = c; }
    SubCondition subCondition() const  { return m_scondition; }
    void setSubCondition(SubCondition c) { m_scondition = c; }
    QString comments() const           { return m_comments; }
    void setComments(const QString &n) { m_comments = n; }
    QString remarks() const            { return m_remarks; }
    void setRemarks(const QString &r)  { m_remarks = r; }

    int quantity() const               { return m_quantity; }
    void setQuantity(int q)            { m_quantity = q; }
    int bulkQuantity() const           { return m_bulk_quantity; }
    void setBulkQuantity(int q)        { m_bulk_quantity = qMax(1, q); }
    int tierQuantity(int i) const      { return m_tier_quantity [qBound(0, i, 2)]; }
    void setTierQuantity(int i, int q) { m_tier_quantity [qBound(0, i, 2)] = q; }
    double price() const               { return m_price; }
    double priceKilo() const           { return price() > 0 && weight() > 0 ? price() / weight() * 1000 : 0; }
    void setPrice(double p)            { m_price = p; }
    double tierPrice(int i) const      { return m_tier_price[qBound(0, i, 2)]; }
    void setTierPrice(int i, double p) { m_tier_price[qBound(0, i, 2)] = p; }

    int sale() const                   { return m_sale; }
    void setSale(int s)                { m_sale = qMax(-99, qMin(100, s)); }
    double total() const               { return m_price * m_quantity; }
    void setCost(double c)             { m_cost = c; }
    double cost() const                { return m_cost; }

    uint lotId() const                 { return m_lot_id; }
    void setLotId(uint lid)            { m_lot_id = lid; }

    bool retain() const                { return m_retain; }
    void setRetain(bool r)             { m_retain = r; }
    Stockroom stockroom() const        { return m_stockroom; }
    void setStockroom(Stockroom sr)    { m_stockroom = sr; }

    bool hasCustomWeight() const       { return (m_weight > 0); }
    double weight() const              { return hasCustomWeight() ? m_weight : (m_item ? m_item->weight() : 0); }
    double totalWeight() const         { return weight() * quantity(); }
    void setWeight(double w)           { m_weight = (w <= 0) ? 0 : w; }
    void setTotalWeight(double w)      { m_weight = (w <= 0) ? 0 : (w / (m_quantity ? qAbs(m_quantity) : 1)); }

    QString reserved() const           { return m_reserved; }
    void setReserved(const QString &r) { m_reserved = r; }

    bool alternate() const             { return m_alternate; }
    void setAlternate(bool a)          { m_alternate = a; }
    uint alternateId() const           { return m_alt_id; }
    void setAlternateId(uint aid)      { m_alt_id = aid; }

    bool counterPart() const           { return m_cpart; }
    void setCounterPart(bool b)        { m_cpart = b; }

    // needed for the copy/merge template code -- std::bind doesn't work there
    int tierQuantity0() const          { return tierQuantity(0); }
    int tierQuantity1() const          { return tierQuantity(1); }
    int tierQuantity2() const          { return tierQuantity(2); }
    void setTierQuantity0(int q)       { setTierQuantity(0, q); }
    void setTierQuantity1(int q)       { setTierQuantity(1, q); }
    void setTierQuantity2(int q)       { setTierQuantity(2, q); }
    double tierPrice0() const          { return tierPrice(0); }
    double tierPrice1() const          { return tierPrice(1); }
    double tierPrice2() const          { return tierPrice(2); }
    void setTierPrice0(double p)       { setTierPrice(0, p); }
    void setTierPrice1(double p)       { setTierPrice(1, p); }
    void setTierPrice2(double p)       { setTierPrice(2, p); }

    bool isMarked() const              { return !m_markerText.isEmpty() || m_markerColor.isValid(); }
    QString markerText() const         { return m_markerText; }
    QColor markerColor() const         { return m_markerColor; }
    void setMarkerText(const QString &text)  { m_markerText = text; }
    void setMarkerColor(const QColor &color) { m_markerColor = color; }

    QDateTime dateAdded() const        { return m_dateAdded; }
    void setDateAdded(const QDateTime &dt)    { m_dateAdded = dt.toUTC(); }
    QDateTime dateLastSold() const     { return m_dateLastSold; }
    void setDateLastSold(const QDateTime &dt) { m_dateLastSold = dt.toUTC(); }

    Incomplete *isIncomplete() const    { return m_incomplete.data(); }
    void setIncomplete(Incomplete *inc) { m_incomplete.reset(inc); }

    bool mergeFrom(const Lot &merge, bool useCostQtyAg = false);

    void save(QDataStream &ds) const;
    static Lot *restore(QDataStream &ds);

private:
    const Item * m_item;
    const Color *m_color;

    QScopedPointer<Incomplete> m_incomplete;

    Status       m_status    : 3 = Status::Include;
    Condition    m_condition : 2 = Condition::New;
    SubCondition m_scondition: 3 = SubCondition::None;
    int          m_retain    : 1 = false;
    Stockroom    m_stockroom : 5 = Stockroom::None;
    int          m_alternate : 1 = false;
    uint         m_alt_id    : 6 = 0;
    int          m_cpart     : 1 = false;

    uint    m_lot_id = 0;
    QString m_reserved;

    QString m_comments;
    QString m_remarks;

    int     m_quantity = 0;
    int     m_bulk_quantity = 1;
    int     m_tier_quantity[3] = { 0, 0, 0 };
    int     m_sale = 0;

    double  m_price = 0;
    double  m_cost = 0;
    double  m_tier_price[3] = { 0, 0, 0 };

    double  m_weight = 0;

    QString m_markerText;
    QColor  m_markerColor;

    QDateTime m_dateAdded;
    QDateTime m_dateLastSold;

    friend class Core;
};

typedef QVector<Lot *> LotList;

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Lot *)
Q_DECLARE_METATYPE(const BrickLink::Lot *)
