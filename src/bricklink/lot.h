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
#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QDateTime>
#include <QtGui/QColor>

#include "bricklink/global.h"
#include "bricklink/item.h"
#include "bricklink/core.h"

class QmlDocumentLots;

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
    void setBulkQuantity(int q)        { m_bulk_quantity = qMax(1, q); }
    int tierQuantity(int i) const      { return m_tier_quantity [qBound(0, i, 2)]; }
    void setTierQuantity(int i, int q) { m_tier_quantity [qBound(0, i, 2)] = q; }
    double price() const               { return m_price; }
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

    Incomplete *isIncomplete() const    { return m_incomplete.get(); }
    void setIncomplete(Incomplete *inc) { m_incomplete.reset(inc); }

    bool mergeFrom(const Lot &merge, bool useCostQtyAg = false);

    void save(QDataStream &ds) const;
    static Lot *restore(QDataStream &ds);

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

typedef QList<Lot *> LotList;


class QmlLot : public QmlWrapperBase<Lot>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(QmlItem item READ item WRITE setItem)
    Q_PROPERTY(QmlColor color READ color WRITE setColor)
    Q_PROPERTY(QmlCategory category READ category)
    Q_PROPERTY(QmlItemType itemType READ itemType)
    Q_PROPERTY(QString itemId READ itemId)
    Q_PROPERTY(QString id READ itemId)
    Q_PROPERTY(QString itemName READ itemName)
    Q_PROPERTY(QString name READ itemName)
    Q_PROPERTY(QString colorName READ colorName)
    Q_PROPERTY(QString categoryName READ categoryName)
    Q_PROPERTY(QString itemTypeName READ itemTypeName)
    Q_PROPERTY(int itemYearReleased READ itemYearReleased)

    Q_PROPERTY(QmlBrickLink::Status status READ status WRITE setStatus)
    Q_PROPERTY(QmlBrickLink::Condition condition READ condition WRITE setCondition)
    Q_PROPERTY(QmlBrickLink::SubCondition subCondition READ subCondition WRITE setSubCondition)

    Q_PROPERTY(QString comments READ comments WRITE setComments)
    Q_PROPERTY(QString remarks READ remarks WRITE setRemarks)

    Q_PROPERTY(int quantity READ quantity WRITE setQuantity)
    Q_PROPERTY(int bulkQuantity READ bulkQuantity WRITE setBulkQuantity)
    Q_PROPERTY(int tier1Quantity READ tier1Quantity WRITE setTier1Quantity)
    Q_PROPERTY(int tier2Quantity READ tier2Quantity WRITE setTier2Quantity)
    Q_PROPERTY(int tier3Quantity READ tier3Quantity WRITE setTier3Quantity)

    Q_PROPERTY(double price READ price WRITE setPrice)
    Q_PROPERTY(double tier1Price READ tier1Price WRITE setTier1Price)
    Q_PROPERTY(double tier2Price READ tier2Price WRITE setTier2Price)
    Q_PROPERTY(double tier3Price READ tier3Price WRITE setTier3Price)

    Q_PROPERTY(int sale READ sale WRITE setSale)
    Q_PROPERTY(double total READ total)

    Q_PROPERTY(uint lotId READ lotId WRITE setLotId)
    Q_PROPERTY(bool retain READ retain WRITE setRetain)
    Q_PROPERTY(QmlBrickLink::Stockroom stockroom READ stockroom WRITE setStockroom)

    Q_PROPERTY(double totalWeight READ totalWeight WRITE setTotalWeight)
    Q_PROPERTY(QString reserved READ reserved WRITE setReserved)
    Q_PROPERTY(bool alternate READ alternate WRITE setAlternate)
    Q_PROPERTY(uint alternateId READ alternateId WRITE setAlternateId)
    Q_PROPERTY(bool counterPart READ counterPart WRITE setCounterPart)

    Q_PROPERTY(bool incomplete READ incomplete)

    Q_PROPERTY(QImage image READ image)

public:
    QmlLot(Lot *lot = nullptr, ::QmlDocumentLots *documentLots = nullptr);
    QmlLot(const QmlLot &copy);
    QmlLot(QmlLot &&move);
    ~QmlLot() override;

    static QmlLot create(Lot * &&lot); // QmlLot owns the lot
    QmlLot &operator=(const QmlLot &assign);

    QmlItem item() const               { return get()->item(); }
    void setItem(QmlItem item)         { set().to()->setItem(item.wrappedObject()); }
    QmlColor color() const             { return get()->color(); }
    void setColor(QmlColor color)      { set().to()->setColor(color.wrappedObject()); }
    QmlCategory category() const       { return get()->category(); }
    QmlItemType itemType() const       { return get()->itemType(); }

    QString itemId() const             { return QString::fromLatin1(get()->itemId()); }
    QString itemName() const           { return get()->itemName(); }
    QString colorName() const          { return get()->colorName(); }
    QString categoryName() const       { return get()->categoryName(); }
    QString itemTypeName() const       { return get()->itemTypeName(); }
    int itemYearReleased() const       { return get()->itemYearReleased(); }

    QmlBrickLink::Status status() const                { return static_cast<QmlBrickLink::Status>(get()->status()); }
    void setStatus(QmlBrickLink::Status s)             { set().to()->setStatus(static_cast<Status>(s)); }
    QmlBrickLink::Condition condition() const          { return static_cast<QmlBrickLink::Condition>(get()->condition()); }
    void setCondition(QmlBrickLink::Condition c)       { set().to()->setCondition(static_cast<Condition>(c)); }
    QmlBrickLink::SubCondition subCondition() const    { return static_cast<QmlBrickLink::SubCondition>(get()->subCondition()); }
    void setSubCondition(QmlBrickLink::SubCondition c) { set().to()->setSubCondition(static_cast<SubCondition>(c)); }
    QString comments() const           { return get()->comments(); }
    void setComments(const QString &n) { set().to()->setComments(n); }
    QString remarks() const            { return get()->remarks(); }
    void setRemarks(const QString &r)  { set().to()->setRemarks(r); }

    int quantity() const               { return get()->quantity(); }
    void setQuantity(int q)            { set().to()->setQuantity(q); }
    int bulkQuantity() const           { return get()->bulkQuantity(); }
    void setBulkQuantity(int q)        { set().to()->setBulkQuantity(q); }
    int tier1Quantity() const          { return get()->tierQuantity(0); }
    void setTier1Quantity(int q)       { set().to()->setTierQuantity(0, q); }
    int tier2Quantity() const          { return get()->tierQuantity(1); }
    void setTier2Quantity(int q)       { set().to()->setTierQuantity(1, q); }
    int tier3Quantity() const          { return get()->tierQuantity(2); }
    void setTier3Quantity(int q)       { set().to()->setTierQuantity(2, q); }

    double price() const               { return get()->price(); }
    void setPrice(double p)            { set().to()->setPrice(p); }
    double tier1Price() const          { return get()->tierPrice(0); }
    void setTier1Price(double p)       { set().to()->setTierPrice(0, p); }
    double tier2Price() const          { return get()->tierPrice(1); }
    void setTier2Price(double p)       { set().to()->setTierPrice(1, p); }
    double tier3Price() const          { return get()->tierPrice(2); }
    void setTier3Price(double p)       { set().to()->setTierPrice(2, p); }

    int sale() const                   { return get()->sale(); }
    void setSale(int s)                { set().to()->setSale(s); }
    double total() const               { return get()->total(); }

    uint lotId() const                 { return get()->lotId(); }
    void setLotId(uint lid)            { set().to()->setLotId(lid); }

    bool retain() const                { return get()->retain(); }
    void setRetain(bool r)             { set().to()->setRetain(r); }
    QmlBrickLink::Stockroom stockroom() const     { return static_cast<QmlBrickLink::Stockroom>(get()->stockroom()); }
    void setStockroom(QmlBrickLink::Stockroom sr) { set().to()->setStockroom(static_cast<Stockroom>(sr)); }

    double totalWeight() const         { return get()->totalWeight(); }
    void setTotalWeight(double w)      { set().to()->setTotalWeight(w); }

    QString reserved() const           { return get()->reserved(); }
    void setReserved(const QString &r) { set().to()->setReserved(r); }

    bool alternate() const             { return get()->alternate(); }
    void setAlternate(bool a)          { set().to()->setAlternate(a); }
    uint alternateId() const           { return get()->alternateId(); }
    void setAlternateId(uint aid)      { set().to()->setAlternateId(aid); }

    bool counterPart() const           { return get()->counterPart(); }
    void setCounterPart(bool b)        { set().to()->setCounterPart(b); }

    bool incomplete() const            { return get()->isIncomplete(); }

    QImage image() const;

private:
    class Setter
    {
    public:
        Setter(QmlLot *lot);
        Lot *to();
        ~Setter();

    private:
        // the definition is in document.cpp
        void doChangeLot(::QmlDocumentLots *lots, Lot *which, const Lot &value);
        QmlLot *m_lot;
        Lot m_to;
    };
    Setter set();
    Lot *get() const;

    QmlDocumentLots *m_documentLots = nullptr;
    constexpr static quintptr Owning = 1u;

    friend class Setter;
    friend class ::QmlDocumentLots;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Lot *)
Q_DECLARE_METATYPE(const BrickLink::Lot *)
Q_DECLARE_METATYPE(BrickLink::QmlLot)
