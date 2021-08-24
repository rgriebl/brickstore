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
#include <QLocale>
#include <QHash>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QUrl>
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
QT_FORWARD_DECLARE_CLASS(QSaveFile)


namespace BrickLink {

class ItemType
{
public:
    static constexpr uint InvalidId = 0;

    char id() const                 { return m_id; }
    QString name() const            { return m_name; }

    const QVector<const Category *> categories() const;
    bool hasInventories() const     { return m_has_inventories; }
    bool hasColors() const          { return m_has_colors; }
    bool hasYearReleased() const    { return m_has_year; }
    bool hasWeight() const          { return m_has_weight; }
    bool hasSubConditions() const   { return m_has_subconditions; }
    char pictureId() const          { return m_picture_id; }
    QSize pictureSize() const;
    QSize rawPictureSize() const;

    ItemType() = default;
    ItemType(std::nullptr_t) : ItemType() { } // for scripting only!

private:
    char  m_id = InvalidId;
    char  m_picture_id = 0;

    bool  m_has_inventories   : 1;
    bool  m_has_colors        : 1;
    bool  m_has_weight        : 1;
    bool  m_has_year          : 1;
    bool  m_has_subconditions : 1;

    // 5 bytes padding here

    QString m_name;
    std::vector<quint16> m_categoryIndexes;

private:
    static bool lessThan(const ItemType &itt, char id) { return itt.m_id < id; }

    friend class Core;
    friend class TextImport;
};


class Category
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const       { return m_id; }
    QString name() const  { return m_name; }

    Category() = default;
    Category(std::nullptr_t) : Category() { } // for scripting only!

private:
    uint     m_id = InvalidId;
    // 4 bytes padding here
    QString  m_name;

private:
    static bool lessThan(const Category &cat, uint id) { return cat.m_id < id; }

    friend class Core;
    friend class TextImport;
};

class Color
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

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

    Color() = default;
    Color(std::nullptr_t) : Color() { } // for scripting only!

private:
    QString m_name;
    uint    m_id = 0;
    int     m_ldraw_id = 0;
    QColor  m_color;
    Type    m_type = {};
    float   m_popularity = 0;
    quint16 m_year_from = 0;
    quint16 m_year_to = 0;
    // 4 bytes padding here

private:
    static bool lessThan(const Color &color, uint id) { return color.m_id < id; }

    friend class Core;
    friend class TextImport;
};


class Item
{
public:
    inline QByteArray id() const           { return m_id; }
    inline QString name() const            { return m_name; }
    inline char itemTypeId() const         { return m_itemTypeId; }
    const ItemType *itemType() const;
    const Category *category() const;
    inline bool hasInventory() const       { return (m_lastInventoryUpdate >= 0); }
    QDateTime inventoryUpdated() const;
    const Color *defaultColor() const;
    double weight() const                  { return double(m_weight); }
    int yearReleased() const               { return m_year ? m_year + 1900 : 0; }
    bool hasKnownColor(const Color *col) const;
    const QVector<const Color *> knownColors() const;

    ~Item();

    AppearsIn appearsIn(const Color *color = nullptr) const;

    class ConsistsOf {
    public:
        const Item *item() const;
        const Color *color() const;
        int quantity() const        { return m_qty; }
        bool isExtra() const        { return m_extra; }
        bool isAlternate() const    { return m_isalt; }
        int alternateId() const     { return m_altid; }
        bool isCounterPart() const  { return m_cpart; }

    private:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint64  m_qty      : 12;
        quint64  m_itemIndex  : 20;
        quint64  m_colorIndex : 12;
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
        quint64  m_colorIndex : 12;
        quint64  m_itemIndex  : 20;
        quint64  m_qty      : 12;
#endif

        friend class TextImport;
        friend class Core;
    };
    Q_STATIC_ASSERT(sizeof(ConsistsOf) == 8);

    const QVector<ConsistsOf> &consistsOf() const;

    uint index() const;   // only for internal use (picture/priceguide hashes)

    Item() = default;
    Item(std::nullptr_t) : Item() { } // for scripting only!

private:
    QString    m_name;
    QByteArray m_id;
    qint16     m_itemTypeIndex = -1;
    qint16     m_categoryIndex = -1;
    qint16     m_defaultColorIndex = -1;
    char       m_itemTypeId; // the same itemType()->id()
    quint8     m_year = 0;
    qint64     m_lastInventoryUpdate = -1;
    float      m_weight = 0;
    // 4 bytes padding here
    std::vector<quint16> m_knownColorIndexes;

    struct AppearsInRecord {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        quint32  m12  : 12;
        quint32  m20  : 20;
#else
        quint32  m20  : 20;
        quint32  m12  : 12;
#endif
    };
    Q_STATIC_ASSERT(sizeof(AppearsInRecord) == 4);

    std::vector<AppearsInRecord> m_appears_in;
    QVector<ConsistsOf> m_consists_of;

private:
    void setAppearsIn(const QHash<uint, QVector<QPair<int, uint>>> &appearHash);
    void setConsistsOf(const QVector<ConsistsOf> &items);


    static int compare(const Item **a, const Item **b);
    static bool lessThan(const Item &item, const std::pair<char, QByteArray> &ids);

    friend class Core;
    friend class ItemType;
    friend class TextImport;
};


class PartColorCode
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const             { return m_id; }
    const Item *item() const;
    const Color *color() const;

    PartColorCode();

private:
    uint m_id = InvalidId;
    signed int m_itemIndex  : 20;
    signed int m_colorIndex : 12;

private:
    static bool lessThan(const PartColorCode &pcc, uint id) { return pcc.m_id < id; }

    friend class Core;
    friend class TextImport;
};


class Picture : public Ref
{
public:
    static quint64 key(const Item *item, const Color *color);

    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool highPriority = false);
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

    bool          m_valid;
    bool          m_updateAfterLoad;
    // 2 bytes padding here
    UpdateStatus  m_update_status;

    TransferJob * m_transferJob = nullptr;

    QImage        m_image;

private:
    Picture(const Item *item, const Color *color);

    QFile *readFile() const;
    QSaveFile *saveFile() const;
    bool loadFromDisk(QDateTime &fetched, QImage &image);

    friend class Core;
    friend class PictureLoaderJob;
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
    OrderStatus status() const        { return m_status; }
    QString paymentType() const       { return m_payment_type; }
    QString trackingNumber() const    { return m_tracking_number; }
    QString address() const           { return m_address; }
    QString countryCode() const       { return m_countryCode; }

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
    void setStatus(OrderStatus status)        { m_status = status; }
    void setPaymentType(const QString &str)   { m_payment_type = str; }
    void setTrackingNumber(const QString &str){ m_tracking_number = str; }
    void setAddress(const QString &str)       { m_address = str; }
    void setCountryCode(const QString &str)   { m_countryCode = str; }

    Order(std::nullptr_t) : Order({ }, OrderType::Received) { } // for scripting only!

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
    OrderStatus m_status;
    QString   m_payment_type;
    QString   m_tracking_number;
    QString   m_address;
    QString   m_countryCode;
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
    QString countryCode() const       { return m_countryCode; }

    void setDomestic(bool domestic)           { m_domestic = domestic; }
    void setSellerId(int id)                  { m_sellerId = id; }
    void setSellerName(const QString &name)   { m_sellerName = name; }
    void setStoreName(const QString &name)    { m_storeName = name; }
    void setLastUpdated(const QDate &dt)      { m_lastUpdated.setDate(dt); }
    void setCartTotal(double m)               { m_cartTotal = m; }
    void setCurrencyCode(const QString &str)  { m_currencycode = str; }

    void setItemCount(int i)                  { m_items = i; }
    void setLotCount(int i)                   { m_lots = i; }
    void setCountryCode(const QString &str)   { m_countryCode = str; }

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
    QString   m_countryCode;
};


class PriceGuide : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool highPriority = false);
    QDateTime lastUpdate() const      { return m_fetched; }
    void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    int quantity(Time t, Condition c) const           { return m_data.quantities[int(t)][int(c)]; }
    int lots(Time t, Condition c) const               { return m_data.lots[int(t)][int(c)]; }
    double price(Time t, Condition c, Price p) const  { return m_data.prices[int(t)][int(c)][int(p)]; }

    PriceGuide(std::nullptr_t) : PriceGuide(nullptr, nullptr) { } // for scripting only!
    ~PriceGuide() override;

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid;
    bool          m_updateAfterLoad;
    bool          m_scrapedHtml = s_scrapeHtml;
    static bool   s_scrapeHtml;
    // 1 byte padding here
    UpdateStatus  m_update_status;

    TransferJob * m_transferJob = nullptr;

    struct Data {
        int    quantities [int(Time::Count)][int(Condition::Count)] = { };
        int    lots       [int(Time::Count)][int(Condition::Count)] = { };
        double prices     [int(Time::Count)][int(Condition::Count)][int(Price::Count)] = { };
    } m_data;

private:
    PriceGuide(const Item *item, const Color *color);

    bool loadFromDisk(QDateTime &fetched, Data &data) const;
    void saveToDisk(const QDateTime &fetched, const Data &data);

    bool parse(const QByteArray &ba, Data &result) const;
    bool parseHtml(const QByteArray &ba, Data &result);

    friend class Core;
    friend class PriceGuideLoaderJob;
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

    bool importInventories(std::vector<bool> &processedInvs);

    const std::vector<Item> &items() const { return m_items; }

private:
    void readColors(const QString &path);
    void readCategories(const QString &path);
    void readItemTypes(const QString &path);
    void readItems(const QString &path, ItemType *itt);
    void readPartColorCodes(const QString &path);
    bool readInventory(const Item *item);
    void readLDrawColors(const QString &path);
    void readInventoryList(const QString &path);
    void readChangeLog(const QString &path);

    int findItemIndex(char type, const QByteArray &id) const;
    int findColorIndex(uint id) const;
    int findCategoryIndex(uint id) const;

    void calculateColorPopularity();
    void addToKnownColors(int itemIndex, int colorIndex);

private:
    std::vector<Color>         m_colors;
    std::vector<ItemType>      m_item_types;
    std::vector<Category>      m_categories;
    std::vector<Item>          m_items;
    std::vector<QByteArray>    m_changelog;
    std::vector<PartColorCode> m_pccs;
    // item-idx -> { color-idx -> { vector < qty, item-idx > } }
    QHash<uint, QHash<uint, QVector<QPair<int, uint>>>> m_appears_in_hash;
    // item-idx -> { vector < consists-of > }
    QHash<uint, QVector<Item::ConsistsOf>>   m_consists_of_hash;
};


class Incomplete
{
public:
    QByteArray m_item_id;
    char       m_itemtype_id = ItemType::InvalidId;
    uint       m_category_id = Category::InvalidId;
    uint       m_color_id = Color::InvalidId;
    QString    m_item_name;
    QString    m_itemtype_name;
    QString    m_category_name;
    QString    m_color_name;

    bool operator==(const Incomplete &other) const; //TODO: = default in C++20
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
        Version_1, // deprecated
        Version_2, // deprecated
        Version_3,

        Version_4,

        Latest = Version_4
    };

    QString defaultDatabaseName(DatabaseVersion version = DatabaseVersion::Latest) const;
    QDateTime databaseDate() const;
    bool isDatabaseUpdateNeeded() const;

    QString dataPath() const;
    QFile *dataReadFile(QStringView fileName, const Item *item,
                        const Color *color = nullptr) const;
    QSaveFile *dataSaveFile(QStringView fileName, const Item *item,
                            const Color *color = nullptr) const;

    void setCredentials(const QPair<QString, QString> &credentials);

    bool isAuthenticated() const;
    void retrieveAuthenticated(TransferJob *job);

    inline const std::vector<Color> &colors() const         { return m_colors; }
    inline const std::vector<Category> &categories() const  { return m_categories; }
    inline const std::vector<ItemType> &itemTypes() const   { return m_item_types; }
    inline const std::vector<Item> &items() const           { return m_items; }

    const QImage noImage(const QSize &s) const;

    const QImage colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldrawId) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QByteArray &id) const;
    const Item *item(const std::string &tids, const QByteArray &id) const;

    const PartColorCode *partColorCode(uint id);

    PriceGuide *priceGuide(const Item *item, const Color *color, bool highPriority = false);

    QSize standardPictureSize() const;
    Picture *picture(const Item *item, const Color *color, bool highPriority = false);
    Picture *largePicture(const Item *item, bool highPriority = false);

    qreal itemImageScaleFactor() const;
    void setItemImageScaleFactor(qreal f);

    bool isLDrawEnabled() const;
    QString ldrawDataPath() const;
    void setLDrawDataPath(const QString &ldrawDataPath);

    bool applyChangeLog(const Item *&item, const Color *&color, Incomplete *inc);

    bool onlineStatus() const;

    QString countryIdFromName(const QString &name) const;

    bool readDatabase(const QString &filename = QString());
    bool writeDatabase(const QString &filename, BrickLink::Core::DatabaseVersion version) const;

public slots:
    void setOnlineStatus(bool on);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void cancelTransfers();

signals:
    void databaseDateChanged(const QDateTime &date);
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void pictureUpdated(BrickLink::Picture *inv);
    void itemImageScaleFactorChanged(qreal f);

    void transferProgress(int progress, int total);
    void authenticatedTransferProgress(int progress, int total);
    void authenticatedTransferJobProgress(TransferJob *job, int progress, int total);
    void authenticatedTransferFinished(TransferJob *job);

    void authenticationChanged(bool auth);
    void authenticationFailed(const QString &userName, const QString &error);

private:
    Core(const QString &datadir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

    QString dataFileName(QStringView fileName, const Item *item, const Color *color) const;

private:
    void updatePriceGuide(BrickLink::PriceGuide *pg, bool highPriority = false);
    void updatePicture(BrickLink::Picture *pic, bool highPriority = false);
    friend void PriceGuide::update(bool);
    friend void Picture::update(bool);

    void cancelPriceGuideUpdate(BrickLink::PriceGuide *pg);
    void cancelPictureUpdate(BrickLink::Picture *pic);
    friend void PriceGuide::cancelUpdate();
    friend void Picture::cancelUpdate();

    static bool updateNeeded(bool valid, const QDateTime &last, int iv);

    static void readColorFromDatabase(Color &col, QDataStream &dataStream, DatabaseVersion v);
    static void writeColorToDatabase(const Color &color, QDataStream &dataStream, DatabaseVersion v);

    static void readCategoryFromDatabase(Category &cat, QDataStream &dataStream, DatabaseVersion v);
    static void writeCategoryToDatabase(const Category &category, QDataStream &dataStream, DatabaseVersion v);

    static void readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, DatabaseVersion v);
    static void writeItemTypeToDatabase(const ItemType &itemType, QDataStream &dataStream, DatabaseVersion v);

    static void readItemFromDatabase(Item &item, QDataStream &dataStream, DatabaseVersion v);
    static void writeItemToDatabase(const Item &item, QDataStream &dataStream, DatabaseVersion v);

    void readPCCFromDatabase(PartColorCode &pcc, QDataStream &dataStream, Core::DatabaseVersion v) const;
    static void writePCCToDatabase(const PartColorCode &pcc, QDataStream &dataStream, DatabaseVersion v);

private slots:
    void pictureJobFinished(TransferJob *j, BrickLink::Picture *pic);
    void priceGuideJobFinished(TransferJob *j, BrickLink::PriceGuide *pg);

    void priceGuideLoaded(BrickLink::PriceGuide *pg);
    void pictureLoaded(BrickLink::Picture *pic);

    friend class PriceGuideLoaderJob;
    friend class PictureLoaderJob;

public: // semi-public for the QML wrapper
    QPair<int, int> pictureCacheStats() const;
    QPair<int, int> priceGuideCacheStats() const;

private:
    void clear();

    QString  m_datadir;
    bool     m_online = false;

    QIcon                           m_noImageIcon;
    mutable QHash<uint, QImage>     m_noImageCache;
    mutable QHash<uint, QImage>     m_colorImageCache;

    std::vector<Color>         m_colors;
    std::vector<Category>      m_categories;
    std::vector<ItemType>      m_item_types;
    std::vector<Item>          m_items;
    std::vector<QByteArray>    m_changelog;
    std::vector<PartColorCode> m_pccs;

    QPointer<Transfer>         m_transfer = nullptr;
    QPointer<Transfer>         m_authenticatedTransfer = nullptr;
    bool                       m_authenticated = false;
    QPair<QString, QString>    m_credentials;
    TransferJob *              m_loginJob = nullptr;

    int                          m_db_update_iv = 0;
    QDateTime                    m_databaseDate;

    int                          m_pg_update_iv = 0;
    Q3Cache<quint64, PriceGuide> m_pg_cache;

    int                          m_pic_update_iv = 0;
    QThreadPool                  m_diskloadPool;
    Q3Cache<quint64, Picture>    m_pic_cache;

    qreal m_item_image_scale_factor = 1.;

    QString m_ldraw_datadir;

    friend class TextImport;
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

inline const ItemType *Item::itemType() const
{ return (m_itemTypeIndex != -1) ? &core()->itemTypes()[m_itemTypeIndex] : nullptr; }
inline const Category *Item::category() const
{ return (m_categoryIndex != -1) ? &core()->categories()[m_categoryIndex] : nullptr; }
inline const Color *Item::defaultColor() const
{ return (m_defaultColorIndex != -1) ? &core()->colors()[m_defaultColorIndex] : nullptr; }

inline const Item *PartColorCode::item() const
{ return (m_itemIndex != -1) ? &core()->items()[m_itemIndex] : nullptr; }
inline const Color *PartColorCode::color() const
{ return (m_colorIndex != -1) ? &core()->colors()[m_colorIndex] : nullptr; }

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
