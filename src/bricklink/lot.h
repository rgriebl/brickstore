// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QDateTime>
#include <QtGui/QColor>

#include "bricklink/global.h"
#include "bricklink/item.h"

class QmlDocumentLots;

namespace BrickLink {

class Lot
{
public:
    explicit Lot(const Item *item = nullptr, const Color *color = nullptr);
    Lot(const Lot &copy);
    ~Lot();

    Lot &operator=(const Lot &copy);
    bool operator==(const Lot &cmp) const;

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
    int itemYearLastProduced() const   { return m_item ? m_item->yearLastProduced() : 0; }

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
    void setBulkQuantity(int q)        { m_bulk_quantity = std::max(1, q); }
    int tierQuantity(int i) const      { return m_tier_quantity [std::clamp(i, 0, 2)]; }
    void setTierQuantity(int i, int q) { m_tier_quantity [std::clamp(i, 0, 2)] = q; }
    double price() const               { return m_price; }
    void setPrice(double p)            { m_price = p; }
    double tierPrice(int i) const      { return m_tier_price[std::clamp(i, 0, 2)]; }
    void setTierPrice(int i, double p) { m_tier_price[std::clamp(i, 0, 2)] = p; }

    int sale() const                   { return m_sale; }
    void setSale(int s)                { m_sale = std::clamp(s, -99, 100); }
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

    Incomplete *isIncomplete() const    { return m_incomplete.get(); }
    void setIncomplete(Incomplete *inc) { m_incomplete.reset(inc); }

    void save(QDataStream &ds) const;
    static Lot *restore(QDataStream &ds, uint startChangelogAt);

private:
    const Item * m_item;
    const Color *m_color;

    std::unique_ptr<Incomplete> m_incomplete;

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
    std::array<int, 3> m_tier_quantity = { 0, 0, 0 };
    int     m_sale = 0;

    double  m_price = 0;
    double  m_cost = 0;
    std::array<double, 3> m_tier_price = { 0, 0, 0 };

    double  m_weight = 0;

    QString m_markerText;
    QColor  m_markerColor;

    QDateTime m_dateAdded;
    QDateTime m_dateLastSold;

    friend class Core;
};

using LotList = QList<Lot *>;

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Lot *)
Q_DECLARE_METATYPE(const BrickLink::Lot *)
