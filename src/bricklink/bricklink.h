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

#include "bricklinkfwd.h"

#include <QDateTime>
#include <QString>
#include <QColor>
#include <QObject>
#include <QImage>
#include <QtXml/QDomDocument>
#include <QLocale>
#include <QHash>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QUrl>
#include <QMimeData>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QMutex>
#include <QTimer>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QThreadPool>

#include <ctime>

#include "ref.h"
#include "currency.h"
#include "transfer.h"
#include "staticpointermodel.h"
#include "q3cache.h"

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QFile)


namespace BrickLink {

class ItemType
{
public:
    char id() const                 { return m_id; }
    QString name() const            { return m_name; }
    QString apiName () const        { return QString(m_name).replace(" ", "_"); } //TODO5: brickstock has this

    const QVector<const Category *> categories() const  { return m_categories; }
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasYearReleased() const    { return m_has_year; }
    bool hasWeight() const          { return m_has_weight; }
    bool hasSubConditions() const   { return m_has_subconditions; }
    char pictureId() const          { return m_picture_id; }
    QSize pictureSize() const;
    QSize rawPictureSize() const;

    ItemType(std::nullptr_t) : ItemType() { } // for scripting only!

private:
    char  m_id = 0;
    char  m_picture_id = 0;

    bool  m_has_inventories   : 1;
    bool  m_has_colors        : 1;
    bool  m_has_weight        : 1;
    bool  m_has_year          : 1;
    bool  m_has_subconditions : 1;

    QString m_name;

    QVector<const Category *> m_categories;

private:
    ItemType() = default;
    static bool lowerBound(const ItemType *itt, char id) { return itt->m_id < id; }

    friend class Core;
    friend class TextImport;
};


class Category
{
public:
    uint id() const       { return m_id; }
    QString name() const  { return m_name; }

    Category(std::nullptr_t) : Category() { } // for scripting only!

private:
    uint     m_id = 0;
    QString  m_name;

private:
    Category() = default;
    static bool lowerBound(const Category *cat, uint id) { return cat->m_id < id; }

    friend class Core;
    friend class TextImport;
};

class Color
{
public:
    uint id() const           { return m_id; }
    QString name() const      { return m_name; }
    QColor color() const      { return m_color; }

    int ldrawId() const       { return m_ldraw_id; }

    enum TypeFlag {
        Solid        = 0x0001,
        Transparent  = 0x0002,
        Glitter      = 0x0004,
        Speckle      = 0x0008,
        Metallic     = 0x0010,
        Chrome       = 0x0020,
        Pearl        = 0x0040,
        Milky        = 0x0080,
        Modulex      = 0x0100,
        Satin        = 0x0200,

        Mask         = 0x03ff
    };

    Q_DECLARE_FLAGS(Type, TypeFlag)
    Type type() const          { return m_type; }

    bool isSolid() const       { return m_type & Solid; }
    bool isTransparent() const { return m_type & Transparent; }
    bool isGlitter() const     { return m_type & Glitter; }
    bool isSpeckle() const     { return m_type & Speckle; }
    bool isMetallic() const    { return m_type & Metallic; }
    bool isChrome() const      { return m_type & Chrome; }
    bool isPearl() const       { return m_type & Pearl; }
    bool isMilky() const       { return m_type & Milky; }
    bool isModulex() const     { return m_type & Modulex; }
    bool isSatin() const       { return m_type & Satin; }

    qreal popularity() const   { return m_popularity < 0 ? 0 : m_popularity; }

    static QString typeName(TypeFlag t);

    Color(std::nullptr_t) : Color() { } // for scripting only!

private:
    QString m_name;
    uint    m_id = 0;
    int     m_ldraw_id = 0;
    QColor  m_color;
    Type    m_type = {};
    qreal   m_popularity = 0;
    quint16 m_year_from = 0;
    quint16 m_year_to = 0;

private:
    static bool lowerBound(const Color *color, uint id) { return color->m_id < id; }
    Color() = default;

    friend class Core;
    friend class TextImport;
};


class Item
{
public:
    QString id() const                     { return m_id; }
    QString name() const                   { return m_name; }
    const ItemType *itemType() const       { return m_item_type; }
    const Category *category() const       { return m_category; }
    bool hasInventory() const              { return (m_last_inv_update >= 0); }
    QDateTime inventoryUpdated() const     { QDateTime dt; if (m_last_inv_update >= 0) dt.setSecsSinceEpoch(uint(m_last_inv_update)); return dt; }
    const Color *defaultColor() const      { return m_color; }
    double weight() const                  { return double(m_weight); }
    int yearReleased() const               { return m_year ? m_year + 1900 : 0; }
    bool hasKnownColors() const            { return !m_known_colors.isEmpty(); }
    const QVector<const Color *> knownColors() const;

    ~Item();

    AppearsIn appearsIn(const Color *color = nullptr) const;
    InvItemList  consistsOf() const;

    uint index() const { return m_index; }   // only for internal use (picture/priceguide hashes)

    Item(std::nullptr_t) : Item() { } // for scripting only!

private:
    QString           m_name;
    QString           m_id;
    const ItemType *  m_item_type = nullptr;
    const Category *  m_category = nullptr;
    const Color *     m_color = nullptr;
    time_t            m_last_inv_update = -1;
    float             m_weight = 0;
    quint32           m_index : 24;
    quint32           m_year  : 8;
    QVector<uint>     m_known_colors;

    mutable quint32 * m_appears_in = nullptr;
    mutable quint64 * m_consists_of = nullptr;

private:
    Item() = default;
    Q_DISABLE_COPY(Item)

    void setAppearsIn(const AppearsIn &hash) const;
    void setConsistsOf(const InvItemList &items) const;

    struct appears_in_record {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint32  m12  : 12;
        quint32  m20  : 20;
#else
        quint32  m20  : 20;
        quint32  m12  : 12;
#endif
    };

    struct consists_of_record {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint64  m_qty      : 12;
        quint64  m_index    : 20;
        quint64  m_color    : 12;
        quint64  m_extra    : 1;
        quint64  m_isalt    : 1;
        quint64  m_altid    : 6;
        quint64  m_cpart    : 1;
        quint64  m_reserved : 11;
#else
        quint64  m_reserved : 11;
        quint64  m_cpart    : 1;
        quint64  m_altid    : 6;
        quint64  m_isalt    : 1;
        quint64  m_extra    : 1;
        quint64  m_color    : 12;
        quint64  m_index    : 20;
        quint64  m_qty      : 12;
#endif
    };

    static int compare(const Item **a, const Item **b);
    static bool lowerBound(const Item *item, const std::pair<char, QString> &ids);

    friend class Core;
    friend class TextImport;
};


class PartColorCode
{
public:
    uint id() const             { return m_id; }
    const Item *item() const    { return m_item; }
    const Color *color() const  { return m_color; }

private:
    uint         m_id = 0;
    const Item * m_item = nullptr;
    const Color *m_color = nullptr;

private:
    PartColorCode() = default;
    static bool lowerBound(const PartColorCode *pcc, uint id) { return pcc->m_id < id; }

    friend class Core;
    friend class TextImport;
};


class Picture : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }
    void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    const QImage image() const;

    int cost() const;

    Picture(std::nullptr_t) : Picture(nullptr, nullptr) { } // for scripting only!
    ~Picture() override;

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    UpdateStatus  m_update_status : 7;

    TransferJob * m_transferJob = nullptr;

    QImage        m_image;

private:
    Picture(const Item *item, const Color *color);

    QFile *file(QIODevice::OpenMode openMode) const;
    void loadFromDisk();

    friend class Core;
    friend class PictureLoaderJob;
};

class InvItem
{
public:
    InvItem(const Color *color = nullptr, const Item *item = nullptr);
    InvItem(const InvItem &copy);
    ~InvItem();

    InvItem &operator=(const InvItem &copy);
    bool operator==(const InvItem &cmp) const;
    bool operator!=(const InvItem &cmp) const;

    const Item *item() const           { return m_item; }
    void setItem(const Item *i)        { m_item = i; }
    const Category *category() const   { return m_item ? m_item->category() : nullptr; }
    const ItemType *itemType() const   { return m_item ? m_item->itemType() : nullptr; }
    const Color *color() const         { return m_color; }
    void setColor(const Color *c)      { m_color = c; }

    QString itemId() const             { return m_item ? m_item->id() : (m_incomplete ? m_incomplete->m_item_id : QString()); }
    QString itemName() const           { return m_item ? m_item->name() : (m_incomplete ? m_incomplete->m_item_name : QString()); }
    QString colorName() const          { return m_color ? m_color->name() : (m_incomplete ? m_incomplete->m_color_name : QString()); }
    QString categoryName() const       { return category() ? category()->name() : (m_incomplete ? m_incomplete->m_category_name : QString()); }
    QString itemTypeName() const       { return itemType() ? itemType()->name() : (m_incomplete ? m_incomplete->m_itemtype_name : QString()); }
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
    void setPrice(double p)            { m_price = p; }
    double tierPrice(int i) const      { return m_tier_price [qBound(0, i, 2)]; }
    bool setTierPrice(int i, double p) { if (p < 0) return false; m_tier_price [qBound(0, i, 2)] = p; return true; }
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

    struct Incomplete {
        QString m_item_id;
        QString m_item_name;
        QString m_itemtype_id;
        QString m_itemtype_name;
        QString m_color_name;
        QString m_category_name;
    };

    const Incomplete *isIncomplete() const { return m_incomplete.data(); }
    void setIncomplete(Incomplete *inc)    { m_incomplete.reset(inc); }

    bool mergeFrom(const InvItem &merge, bool useCostQtyAg = false);

    QImage image() const;

    void save(QDataStream &ds) const;
    static InvItem *restore(QDataStream &ds);

private:
    const Item *     m_item;
    const Color *    m_color;

    QScopedPointer<Incomplete> m_incomplete;

    Status           m_status    : 3;
    Condition        m_condition : 2;
    SubCondition     m_scondition: 3;
    bool             m_retain    : 1;
    Stockroom        m_stockroom : 5;
    bool             m_alternate : 1;
    uint             m_alt_id    : 6;
    bool             m_cpart     : 1;

    uint             m_lot_id = 0;
    QString          m_reserved;

    QString          m_comments;
    QString          m_remarks;

    int              m_quantity = 0;
    int              m_bulk_quantity = 1;
    int              m_tier_quantity[3] = { 0, 0, 0 };
    int              m_sale = 0;

    double           m_price = 0;
    double           m_cost = 0;
    double           m_tier_price[3] = { 0, 0, 0 };

    double           m_weight = 0;

    friend class Core;
};

class InvItemMimeData : public QMimeData
{
    Q_OBJECT
public:
    InvItemMimeData(const InvItemList &items);

    QStringList formats() const override;
    bool hasFormat(const QString &mimeType) const override;

    void setItems(const InvItemList &items);
    static InvItemList items(const QMimeData *md);

private:
    static const char *s_mimetype;
};

class Order
{
public:
    Order(const QString &id, OrderType type);

    QString id() const                { return m_id; }
    OrderType type() const            { return m_type; }
    QDate date() const                { return m_date.date(); }
    QDate statusChange() const        { return m_status_change.date(); }
    QString otherParty() const        { return m_other_party; }

    double shipping() const           { return m_shipping; }
    double insurance() const          { return m_insurance; }
    double additionalCharges1() const { return m_add_charges_1; }
    double additionalCharges2() const { return m_add_charges_2; }
    double credit() const             { return m_credit; }
    double creditCoupon() const       { return m_credit_coupon; }
    double orderTotal() const         { return m_order_total; }
    double salesTax() const           { return m_sales_tax; }
    double grandTotal() const         { return m_grand_total; }
    double vatCharges() const         { return m_vat_charges; }
    QString currencyCode() const      { return m_currencycode; }
    QString paymentCurrencyCode() const { return m_payment_currencycode; }

    int itemCount() const             { return m_items; }
    int lotCount() const              { return m_lots; }
    QString status() const            { return m_status; }
    QString paymentType() const       { return m_payment_type; }
    QString trackingNumber() const    { return m_tracking_number; }
    QString address() const           { return m_address; }
    QString countryName() const;
    QString countryCode() const;

    void setId(const QString &id)             { m_id = id; }
    void setDate(const QDate &dt)             { m_date.setDate(dt); }
    void setStatusChange(const QDate &dt)     { m_status_change.setDate(dt); }
    void setOtherParty(const QString &str)    { m_other_party = str; }

    void setShipping(double m)                { m_shipping = m; }
    void setInsurance(double m)               { m_insurance = m; }
    void setAdditionalCharges1(double m)      { m_add_charges_1 = m; }
    void setAdditionalCharges2(double m)      { m_add_charges_2 = m; }
    void setCredit(double m)                  { m_credit = m; }
    void setCreditCoupon(double m)            { m_credit_coupon = m; }
    void setOrderTotal(double m)              { m_order_total = m; }
    void setSalesTax(double m)                { m_sales_tax = m; }
    void setGrandTotal(double m)              { m_grand_total = m; }
    void setVatCharges(double m)              { m_vat_charges = m; }
    void setCurrencyCode(const QString &str)  { m_currencycode = str; }
    void setPaymentCurrencyCode(const QString &str)  { m_payment_currencycode = str; }

    void setItemCount(int i)                  { m_items = i; }
    void setLotCount(int i)                   { m_lots = i; }
    void setStatus(const QString &str)        { m_status = str; }
    void setPaymentType(const QString &str)   { m_payment_type = str; }
    void setTrackingNumber(const QString &str){ m_tracking_number = str; }
    void setAddress(const QString &str)       { m_address = str; }
    void setCountryName(const QString &str);
    void setCountryCode(const QString &str);

    Order(std::nullptr_t) : Order({ }, BrickLink::OrderType::Received) { } // for scripting only!

private:
    QString   m_id;
    OrderType m_type;
    QDateTime m_date;
    QDateTime m_status_change;
    QString   m_other_party;
    double    m_shipping = 0;
    double    m_insurance = 0;
    double    m_add_charges_1 = 0;
    double    m_add_charges_2 = 0;
    double    m_credit = 0;
    double    m_credit_coupon = 0;
    double    m_order_total = 0;
    double    m_sales_tax = 0;
    double    m_grand_total = 0;
    double    m_vat_charges = 0;
    QString   m_currencycode;
    QString   m_payment_currencycode;
    int       m_items = 0;
    int       m_lots = 0;
    QString   m_status;
    QString   m_payment_type;
    QString   m_tracking_number;
    QString   m_address;
    QChar     m_countryCode[2] = { 'U', 'S' };
};


class Cart
{
public:
    Cart();

    bool domestic() const             { return m_domestic; }
    int sellerId() const              { return m_sellerId; }
    QString sellerName() const        { return m_sellerName; }
    QString storeName() const         { return m_storeName; }
    QDate lastUpdated() const         { return m_lastUpdated.date(); }
    double cartTotal() const          { return m_cartTotal; }
    QString currencyCode() const      { return m_currencycode; }

    int itemCount() const             { return m_items; }
    int lotCount() const              { return m_lots; }
    QString countryName() const;
    QString countryCode() const;

    void setDomestic(bool domestic)           { m_domestic = domestic; }
    void setSellerId(int id)                  { m_sellerId = id; }
    void setSellerName(const QString &name)   { m_sellerName = name; }
    void setStoreName(const QString &name)    { m_storeName = name; }
    void setLastUpdated(const QDate &dt)      { m_lastUpdated.setDate(dt); }
    void setCartTotal(double m)               { m_cartTotal = m; }
    void setCurrencyCode(const QString &str)  { m_currencycode = str; }

    void setItemCount(int i)                  { m_items = i; }
    void setLotCount(int i)                   { m_lots = i; }
    void setCountryName(const QString &str);
    void setCountryCode(const QString &str);

private:
    bool      m_domestic = false;
    int       m_sellerId;
    QString   m_sellerName;
    QString   m_storeName;
    QDateTime m_lastUpdated;
    double    m_cartTotal = 0;
    QString   m_currencycode;
    int       m_items = 0;
    int       m_lots = 0;
    QChar     m_countryCode[2] = { 'U', 'S' };
};


class PriceGuide : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }
    void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    int quantity(Time t, Condition c) const           { return m_quantities[int(t)][int(c)]; }
    int lots(Time t, Condition c) const               { return m_lots[int(t)][int(c)]; }
    double price(Time t, Condition c, Price p) const  { return m_prices[int(t)][int(c)][int(p)]; }

    PriceGuide(std::nullptr_t) : PriceGuide(nullptr, nullptr) { } // for scripting only!
    ~PriceGuide() override;

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    UpdateStatus  m_update_status : 7;

    TransferJob * m_transferJob = nullptr;

    int           m_quantities [int(Time::Count)][int(Condition::Count)];
    int           m_lots       [int(Time::Count)][int(Condition::Count)];
    double        m_prices     [int(Time::Count)][int(Condition::Count)][int(Price::Count)];

    bool          m_scrapedHtml;
    static bool   s_scrapeHtml;

private:
    PriceGuide(const Item *item, const Color *color);

    QFile *file(QIODevice::OpenMode openMode) const;
    void loadFromDisk();
    void saveToDisk();

    void parse(const QByteArray &ba);
    void parseHtml(const QByteArray &ba);

    friend class Core;
};

class ChangeLogEntry
{
public:
    enum Type {
        Invalid,
        ItemId,
        ItemType,
        ItemMerge,
        CategoryName,
        CategoryMerge,
        ColorName,
        ColorMerge
    };

    ChangeLogEntry(const char *data)
        : m_data(QByteArray::fromRawData(data, int(qstrlen(data))))
    { }
    ~ChangeLogEntry() = default;

    Type type() const              { return m_data.isEmpty() ? Invalid : Type(m_data.at(0)); }
    QByteArray from(int idx) const { return (idx < 0 || idx >= 2) ? QByteArray() : m_data.split('\t')[idx+1]; }
    QByteArray to(int idx) const   { return (idx < 0 || idx >= 2) ? QByteArray() : m_data.split('\t')[idx+3]; }

private:
    Q_DISABLE_COPY(ChangeLogEntry)

    const QByteArray m_data;

    friend class Core;
};

class TextImport
{
public:
    TextImport();
    ~TextImport();

    bool import(const QString &path);
    void exportTo(Core *);

    bool importInventories(std::vector<const Item *> &items);
    void exportInventoriesTo(Core *);

    const std::vector<const Item *> &items() const { return m_items; }

private:
    void readColors(const QString &path);
    void readCategories(const QString &path);
    void readItemTypes(const QString &path);
    void readItems(const QString &path, const ItemType *itt);
    void readPartColorCodes(const QString &path);
    bool readInventory(const Item *item);
    void readLDrawColors(const QString &path);
    void readInventoryList(const QString &path);
    void readChangeLog(const QString &path);

    const Item *findItem(char type, const QString &id) const;
    const Color *findColor(uint id) const;
    const BrickLink::Category *findCategory(uint id) const;

    void calculateColorPopularity();

private:
    std::vector<const Color *>         m_colors;
    std::vector<const ItemType *>      m_item_types;
    std::vector<const Category *>      m_categories;
    std::vector<const Item *>          m_items;
    std::vector<QByteArray>            m_changelog;
    std::vector<const PartColorCode *> m_pccs;
    QHash<const Item *, AppearsIn>     m_appears_in_hash;
    QHash<const Item *, InvItemList>   m_consists_of_hash;

};


class Core : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDateTime databaseDate READ databaseDate NOTIFY databaseDateChanged)

public:
    ~Core() override;

    void openUrl(UrlList u, const void *opt = nullptr, const void *opt2 = nullptr);

    enum class DatabaseVersion {
        Invalid,
        Version_1,
        Version_2,
        Version_3,

        Latest = Version_3
    };

    QString defaultDatabaseName(DatabaseVersion version = DatabaseVersion::Latest) const;
    QDateTime databaseDate() const;
    bool isDatabaseUpdateNeeded() const;

    QString dataPath() const;
    QFile *dataFile(QStringView fileName, QIODevice::OpenMode openMode, const Item *item,
                    const Color *color = nullptr) const;

    const std::vector<const Color *> &colors() const;
    const std::vector<const Category *> &categories() const;
    const std::vector<const ItemType *> &itemTypes() const;
    const std::vector<const Item *> &items() const;

    const QImage noImage(const QSize &s) const;

    const QImage colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldrawId) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QString &id) const;

    const PartColorCode *partColorCode(uint id);

    PriceGuide *priceGuide(const Item *item, const Color *color, bool high_priority = false);
    void flushPriceGuidesToUpdate(); //TODO5: brickstock has this

    QSize standardPictureSize() const;
    Picture *picture(const Item *item, const Color *color, bool high_priority = false);
    Picture *largePicture(const Item *item, bool high_priority = false);

    qreal itemImageScaleFactor() const;
    void setItemImageScaleFactor(qreal f);

    bool isLDrawEnabled() const;
    QString ldrawDataPath() const;
    void setLDrawDataPath(const QString &ldrawDataPath);

    bool applyChangeLogToItem(BrickLink::InvItem *item);

    bool onlineStatus() const;

    Transfer *transfer() const;
    void setTransfer(Transfer *trans);

    bool readDatabase(QString *infoText = nullptr, const QString &filename = QString());
    bool writeDatabase(const QString &filename, BrickLink::Core::DatabaseVersion version,
                       const QString &infoText = QString()) const;

public slots:
    void setOnlineStatus(bool on);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void cancelPictureTransfers();
    void cancelPriceGuideTransfers();

signals:
    void databaseDateChanged(const QDateTime &date);
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void pictureUpdated(BrickLink::Picture *inv);
    void itemImageScaleFactorChanged(qreal f);

    void transferJobProgress(int, int);

private:
    Core(const QString &datadir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

private:
    void updatePriceGuide(BrickLink::PriceGuide *pg, bool high_priority = false);
    void updatePicture(BrickLink::Picture *pic, bool high_priority = false);
    friend void PriceGuide::update(bool);
    friend void Picture::update(bool);

    void cancelPriceGuideUpdate(BrickLink::PriceGuide *pg);
    void cancelPictureUpdate(BrickLink::Picture *pic);
    friend void PriceGuide::cancelUpdate();
    friend void Picture::cancelUpdate();

    static bool updateNeeded(bool valid, const QDateTime &last, int iv);

    static Color *readColorFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeColorToDatabase(const Color *color, QDataStream &dataStream, DatabaseVersion v);

    static Category *readCategoryFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeCategoryToDatabase(const Category *category, QDataStream &dataStream, DatabaseVersion v);

    static ItemType *readItemTypeFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeItemTypeToDatabase(const ItemType *itemType, QDataStream &dataStream, DatabaseVersion v);

    static Item *readItemFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeItemToDatabase(const Item *item, QDataStream &dataStream, DatabaseVersion v);

    PartColorCode *readPCCFromDatabase(QDataStream &dataStream, Core::DatabaseVersion v) const;
    static void writePCCToDatabase(const PartColorCode *pcc, QDataStream &dataStream, DatabaseVersion v);

private slots:
    void pictureJobFinished(TransferJob *j); //TODO5: timeout handling in brickstock updatePicturesTimeOut
    void priceGuideJobFinished(TransferJob *j);

    void pictureLoaded(BrickLink::Picture *pic);

    friend class PictureLoaderJob;

private:
    void clear();

    QString  m_datadir;
    bool     m_online = false;
    QLocale  m_c_locale;
    mutable QMutex m_corelock;

    mutable QHash<QString, QImage>  m_noimages;
    mutable QHash<QString, QImage>  m_colimages;

    std::vector<const Color *>      m_colors;      // id ->Color *
    std::vector<const Category *>   m_categories;  // id ->Category *
    std::vector<const ItemType *>   m_item_types;  // id ->ItemType *
    std::vector<const Item *>       m_items;       // sorted array of Item *
    std::vector<QByteArray>         m_changelog;
    std::vector<const PartColorCode *> m_pccs;

    QPointer<Transfer>  m_transfer = nullptr;

    int                          m_db_update_iv = 0;
    QDateTime                    m_databaseDate;

    int                          m_pg_update_iv = 0;
    Q3Cache<quint64, PriceGuide> m_pg_cache;

    int                          m_pic_update_iv = 0;
    QThreadPool                  m_pic_diskload;
    Q3Cache<quint64, Picture>    m_pic_cache;

    qreal m_item_image_scale_factor = 1.;

    QString m_ldraw_datadir;

    friend class TextImport;
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }


} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Color *)
Q_DECLARE_METATYPE(const BrickLink::Category *)
Q_DECLARE_METATYPE(const BrickLink::ItemType *)
Q_DECLARE_METATYPE(const BrickLink::Item *)
Q_DECLARE_METATYPE(const BrickLink::AppearsInItem *)
Q_DECLARE_METATYPE(const BrickLink::Order *)
Q_DECLARE_METATYPE(const BrickLink::Cart *)

Q_DECLARE_OPERATORS_FOR_FLAGS(BrickLink::Color::Type)


// tell Qt that Pictures and PriceGuides are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
template<> inline bool q3IsDetached<BrickLink::PriceGuide>(BrickLink::PriceGuide &c) { return c.refCount() == 0; }
