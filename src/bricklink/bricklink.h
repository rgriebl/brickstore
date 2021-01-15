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
#include <QList>
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

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const ItemType *itt);
    friend QDataStream &operator >> (QDataStream &ds, ItemType *itt);
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

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator << (QDataStream &ds, const Category *cat);
    friend QDataStream &operator >> (QDataStream &ds, Category *cat);
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

        Mask         = 0x01ff
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

    qreal popularity() const   { return m_popularity < 0 ? 0 : m_popularity; }

    static QString typeName(TypeFlag t);

    Color(std::nullptr_t) : Color() { } // for scripting only!

private:
    QString m_name;
    uint    m_id = 0;
    int     m_ldraw_id = 0;
    QColor  m_color;
    Type    m_type = 0;
    qreal   m_popularity = 0;
    quint16 m_year_from = 0;
    quint16 m_year_to = 0;

private:
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
    const Category *category() const       { return m_categories.isEmpty() ? nullptr : m_categories.constFirst(); }
    const QVector<const Category *> allCategories() const  { return m_categories; }
    bool hasCategory(const Category *cat) const;
    bool hasInventory() const              { return (m_last_inv_update >= 0); }
    QDateTime inventoryUpdated() const     { QDateTime dt; if (m_last_inv_update >= 0) dt.setTime_t(uint(m_last_inv_update)); return dt; }
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
    QVector<const Category *> m_categories;
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
    static bool lessThan(const Item *a, const Item *b);

    friend class Core;
    friend class TextImport;
    friend QDataStream &operator<<(QDataStream &ds, const Item *item);
    friend QDataStream &operator>>(QDataStream &ds, Item *item);
};


class Picture : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }

    bool valid() const                { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    //static int maximumSize()

    const QImage image() const;

    int cost() const;

    Picture(std::nullptr_t) : Picture(nullptr, nullptr) { } // for scripting only!

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    UpdateStatus  m_update_status : 7;

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

    InvItem &operator = (const InvItem &copy);
    bool operator == (const InvItem &cmp) const;
    bool operator != (const InvItem &cmp) const;

    const Item *item() const           { return m_item; }
    void setItem(const Item *i)        { /*can be 0*/ m_item = i; }
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
    int origQuantity() const           { return m_orig_quantity; }
    void setOrigQuantity(int q)        { m_orig_quantity = q; }
    int bulkQuantity() const           { return m_bulk_quantity; }
    void setBulkQuantity(int q)        { m_bulk_quantity = qMax(1, q); }
    int tierQuantity(int i) const      { return m_tier_quantity [qBound(0, i, 2)]; }
    void setTierQuantity(int i, int q) { m_tier_quantity [qBound(0, i, 2)] = q; }
    double price() const               { return m_price; }
    void setPrice(double p)            { m_price = p; }
    double origPrice() const           { return m_orig_price; }
    void setOrigPrice(double p)        { m_orig_price = p; }
    double tierPrice(int i) const      { return m_tier_price [qBound(0, i, 2)]; }
    bool setTierPrice(int i, double p) { if (p < 0) return false; m_tier_price [qBound(0, i, 2)] = p; return true; }
    int sale() const                   { return m_sale; }
    void setSale(int s)                { m_sale = qMax(-99, qMin(100, s)); }
    double total() const               { return m_price * m_quantity; }

    uint lotId() const                 { return m_lot_id; }
    void setLotId(uint lid)            { m_lot_id = lid; }

    bool retain() const                { return m_retain; }
    void setRetain(bool r)             { m_retain = r; }
    Stockroom stockroom() const        { return m_stockroom; }
    void setStockroom(Stockroom sr)    { m_stockroom = sr; }

    double weight() const              { return !qFuzzyIsNull(m_weight) ? m_weight : (m_item ? m_item->weight() * m_quantity : 0); }
    void setWeight(double w)           { m_weight = w; }

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

    const Incomplete *isIncomplete() const { return m_incomplete; }
    void setIncomplete(Incomplete *inc)    { delete m_incomplete; m_incomplete = inc; }

    bool mergeFrom(const InvItem &merge, bool prefer_from = false);

    QImage image() const;

private:
    const Item *     m_item;
    const Color *    m_color;

    Incomplete *     m_incomplete;

    Status           m_status    : 3;
    Condition        m_condition : 2;
    SubCondition     m_scondition: 3;
    bool             m_alternate : 1;
    bool             m_cpart     : 1;
    uint             m_alt_id    : 6;
    bool             m_retain    : 1;
    Stockroom        m_stockroom : 5;
    int              m_xreserved : 10;

    QString          m_comments;
    QString          m_remarks;
    QString          m_reserved;

    int              m_quantity;
    int              m_bulk_quantity;
    int              m_tier_quantity[3];
    int              m_sale;

    double           m_price;
    double           m_tier_price[3];

    double           m_weight;
    uint             m_lot_id;

    double           m_orig_price;
    int              m_orig_quantity;

    friend QDataStream &operator << (QDataStream &ds, const InvItem &ii);
    friend QDataStream &operator >> (QDataStream &ds, InvItem &ii);

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

    QString id() const          { return m_id; }
    OrderType type() const      { return m_type; }
    QDateTime date() const      { return m_date; }
    QDateTime statusChange() const  { return m_status_change; }
    //QString buyer() const     { return m_type == OrderType::Received ? m_other : QString(); }
    //QString seller() const    { return m_type == OrderType::Placed ? m_other : QString(); }
    QString other() const       { return m_other; }
    double shipping() const     { return m_shipping; }
    double insurance() const    { return m_insurance; }
    double delivery() const     { return m_delivery; }
    double credit() const       { return m_credit; }
    double grandTotal() const   { return m_grand_total; }
    QString status() const      { return m_status; }
    QString payment() const     { return m_payment; }
    QString remarks() const     { return m_remarks; }
    QString address() const     { return m_address; }
    QString countryName() const;
    QString countryCode() const;
    QString currencyCode() const { return m_currencycode; }

    void setId(const QString &id)             { m_id = id; }
    void setDate(const QDateTime &dt)         { m_date = dt; }
    void setStatusChange(const QDateTime &dt) { m_status_change = dt; }
    void setBuyer(const QString &str)         { m_other = str; m_type = OrderType::Received; }
    void setSeller(const QString &str)        { m_other = str; m_type = OrderType::Placed; }
    void setShipping(double m)                { m_shipping = m; }
    void setInsurance(double m)               { m_insurance = m; }
    void setDelivery(double m)                { m_delivery = m; }
    void setCredit(double m)                  { m_credit = m; }
    void setGrandTotal(double m)              { m_grand_total = m; }
    void setStatus(const QString &str)        { m_status = str; }
    void setPayment(const QString &str)       { m_payment = str; }
    void setRemarks(const QString &str)       { m_remarks = str; }
    void setAddress(const QString &str)       { m_address = str; }
    void setCountryName(const QString &str);
    void setCountryCode(const QString &str);
    void setCurrencyCode(const QString &str)  { m_currencycode = str; }

private:
    QString   m_id;
    OrderType m_type;
    QDateTime m_date;
    QDateTime m_status_change;
    QString   m_other;
    double    m_shipping;
    double    m_insurance;
    double    m_delivery;
    double    m_credit;
    double    m_grand_total;
    QString   m_status;
    QString   m_payment;
    QString   m_remarks;
    QString   m_address;
    QString   m_currencycode;
    QChar     m_countryCode[2];
};

class PriceGuide : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool high_priority = false);
    QDateTime lastUpdate() const      { return m_fetched; }

    bool valid() const                { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    int quantity(Time t, Condition c) const           { return m_quantities[int(t)][int(c)]; }
    int lots(Time t, Condition c) const               { return m_lots[int(t)][int(c)]; }
    double price(Time t, Condition c, Price p) const  { return m_prices[int(t)][int(c)][int(p)]; }

    PriceGuide(std::nullptr_t) : PriceGuide(nullptr, nullptr) { } // for scripting only!

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid         : 1;
    UpdateStatus  m_update_status : 7;

    int           m_quantities [int(Time::Count)][int(Condition::Count)];
    int           m_lots       [int(Time::Count)][int(Condition::Count)];
    double        m_prices     [int(Time::Count)][int(Condition::Count)][int(Price::Count)];

private:
    PriceGuide(const Item *item, const Color *color);

    QFile *file(QIODevice::OpenMode openMode) const;
    void loadFromDisk();
    void saveToDisk();

    void parse(const QByteArray &ba);

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

    bool importInventories(QVector<const Item *> &items);
    void exportInventoriesTo(Core *);

    const QHash<uint, const Color *>    &colors() const     { return m_colors; }
    const QHash<uint, const Category *> &categories() const { return m_categories; }
    const QHash<char, const ItemType *> &itemTypes() const  { return m_item_types; }
    const QVector<const Item *>       &items() const        { return m_items; }

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

    const Category *findCategoryByName(const QStringRef &name) const;
    const Item *findItem(char type, const QString &id);

    void parseXML(const QString &path, const char *rootNodeName, const char *elementNodeName,
                  std::function<void (QDomElement)> callback);

    void calculateColorPopularity();

private:
    QHash<uint, const Color *>    m_colors;
    QHash<char, const ItemType *> m_item_types;
    QHash<uint, const Category *> m_categories;
    QVector<const Item *>         m_items;

    QHash<const Item *, AppearsIn>   m_appears_in_hash;
    QHash<const Item *, InvItemList> m_consists_of_hash;
    QVector<QByteArray>              m_changelog;

};


class Core : public QObject
{
    Q_OBJECT
public:
    ~Core();

    QUrl url(UrlList u, const void *opt = nullptr, const void *opt2 = nullptr);

    enum class DatabaseVersion {
        Invalid,
        Version_1,
        Version_2,

        Latest = Version_2
    };

    QString defaultDatabaseName(DatabaseVersion version = DatabaseVersion::Latest) const;

    QString dataPath() const;
    QFile *dataFile(QStringView fileName, QIODevice::OpenMode openMode, const Item *item,
                    const Color *color = nullptr) const;

    const QHash<uint, const Color *>    &colors() const;
    const QHash<uint, const Category *> &categories() const;
    const QHash<char, const ItemType *> &itemTypes() const;
    const QVector<const Item *>         &items() const;

    const QImage noImage(const QSize &s) const;

    const QImage colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldraw_id) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QString &id) const;

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

    struct ParseItemListXMLResult
    {
        ParseItemListXMLResult() = default;

        InvItemList *items = nullptr;
        uint invalidItemCount = 0;
        QString currencyCode;
        bool multipleCurrencies = false;
    };

    ParseItemListXMLResult parseItemListXML(const QDomElement &root, ItemListXMLHint hint);
    QDomElement createItemListXML(QDomDocument doc, ItemListXMLHint hint, const InvItemList &items, const QString &currencyCode = QString(), QMap<QString, QString> *extra = nullptr);

    bool parseLDrawModel(QFile &file, InvItemList &items, uint *invalid_items = nullptr);

    int applyChangeLogToItems(BrickLink::InvItemList &bllist);

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

    bool updateNeeded(bool valid, const QDateTime &last, int iv);
    bool parseLDrawModelInternal(QFile &file, const QString &model_name, InvItemList &items, uint *invalid_items, QHash<QString, InvItem *> &mergehash, QStringList &recursion_detection);

    static Color *readColorFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeColorToDatabase(const Color *color, QDataStream &dataStream, DatabaseVersion v);

    static Category *readCategoryFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeCategoryToDatabase(const Category *category, QDataStream &dataStream, DatabaseVersion v);

    static ItemType *readItemTypeFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeItemTypeToDatabase(const ItemType *itemType, QDataStream &dataStream, DatabaseVersion v);

    static Item *readItemFromDatabase(QDataStream &dataStream, DatabaseVersion v);
    static void writeItemToDatabase(const Item *item, QDataStream &dataStream, DatabaseVersion v);

private slots:
    void pictureJobFinished(TransferJob *j); //TODO5: timeout handling in brickstock updatePicturesTimeOut
    void priceGuideJobFinished(TransferJob *j);

    void pictureLoaded(Picture *pic);

    friend class PictureLoaderJob;

private:
    void clear();

    QString  m_datadir;
    bool     m_online = false;
    QLocale  m_c_locale;
    mutable QMutex m_corelock;

    mutable QHash<QString, QImage>  m_noimages;
    mutable QHash<QString, QImage>  m_colimages;

    QHash<uint, const Color *>      m_colors;      // id ->Color *
    QHash<uint, const Category *>   m_categories;  // id ->Category *
    QHash<char, const ItemType *>   m_item_types;  // id ->ItemType *
    QVector<const Item *>           m_items;       // sorted array of Item *
    QVector<QByteArray>             m_changelog;

    QPointer<Transfer>  m_transfer = nullptr;

    //Transfer                   m_pg_transfer;
    int                          m_pg_update_iv = 0;
    Q3Cache<quint64, PriceGuide> m_pg_cache;

    //Transfer                   m_pic_transfer;
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

Q_DECLARE_OPERATORS_FOR_FLAGS(BrickLink::Color::Type)


// tell Qt that Pictures and PriceGuides are shared and can't simply be deleted
// (QCache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
template<> inline bool q3IsDetached<BrickLink::PriceGuide>(BrickLink::PriceGuide &c) { return c.refCount() == 0; }
