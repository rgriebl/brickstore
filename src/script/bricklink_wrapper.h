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

#include <QQmlParserStatus>

#include "bricklink.h"
#include "document.h"
#include "lot.h"

class Window;

namespace QmlWrapper {

class Color;
class Category;
class ItemType;
class Item;
class Lot;
class PriceGuide;
class Picture;
class Document;


template <typename T> class WrapperBase
{
public:
    inline T *wrappedObject() const
    {
        return (wrapped == wrappedNull()) ? nullptr : wrapped;
    }

protected:
    WrapperBase(T *_wrappedObject)
        : wrapped(_wrappedObject ? _wrappedObject : wrappedNull())
    { }

    static T *wrappedNull()
    {
        static T t_null(nullptr);
        return &t_null;
    }

    bool isNull() const
    {
        return !wrappedObject();
    }

    T *wrapped;
};


class BrickLink : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QString cachePath READ dataPath CONSTANT)
    Q_PRIVATE_PROPERTY(d, QSize standardPictureSize READ standardPictureSize CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool ldrawEnabled READ isLDrawEnabled CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString ldrawPath READ ldrawDataPath)
    Q_PRIVATE_PROPERTY(d, bool online READ onlineStatus)

    // const QImage noImage(const QSize &s) const;
    // const QImage colorImage(const Color *col, int w, int h) const;

    Q_PROPERTY(Item noItem READ noItem CONSTANT)
    Q_PROPERTY(Color noColor READ noColor CONSTANT)

public:
    BrickLink(::BrickLink::Core *core);

    // copied from namespace BrickLink
    enum class Time          { PastSix, Current };
    enum class Price         { Lowest, Average, WAverage, Highest };
    enum class Condition     { New, Used };
    enum class SubCondition  { None, Complete, Incomplete, Sealed };
    enum class Stockroom     { None, A, B, C };
    enum class Status        { Include, Exclude, Extra };
    enum class UpdateStatus  { Ok, Loading, Updating, UpdateFailed };

    enum class OrderType     { Received, Placed, Any };
    enum class OrderStatus   { Unknown, Pending, Updated, Processing, Ready, Paid, Packed, Shipped,
                               Received, Completed, OCR, NPB, NPX, NRS, NSS, Cancelled, Count };

    Q_ENUM(Time)
    Q_ENUM(Price)
    Q_ENUM(Condition)
    Q_ENUM(SubCondition)
    Q_ENUM(Stockroom)
    Q_ENUM(Status)
    Q_ENUM(UpdateStatus)
    Q_ENUM(OrderType)
    Q_ENUM(OrderStatus)

    Item noItem() const;
    Color noColor() const;

    Q_INVOKABLE Color color(int id) const;
    Q_INVOKABLE Color colorFromName(const QString &name) const;
    Q_INVOKABLE Color colorFromLDrawId(int ldrawId) const;
    Q_INVOKABLE Category category(int id) const;
    Q_INVOKABLE ItemType itemType(const QString &itemTypeId) const;
    Q_INVOKABLE Item item(const QString &itemTypeId, const QString &itemId) const;

    Q_INVOKABLE PriceGuide priceGuide(Item item, Color color, bool highPriority = false);

    Q_INVOKABLE Picture picture(Item item, Color color, bool highPriority = false);
    Q_INVOKABLE Picture largePicture(Item item, bool highPriority = false);

signals:
    void priceGuideUpdated(QmlWrapper::PriceGuide pg);
    void pictureUpdated(QmlWrapper::Picture inv);

private:
    static char firstCharInString(const QString &str);

    ::BrickLink::Core *d;
};


class Category : public WrapperBase<const ::BrickLink::Category>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)

public:
    Category(const ::BrickLink::Category *cat = nullptr);

    friend class BrickLink;
};


class ItemType : public WrapperBase<const ::BrickLink::ItemType>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasInventories READ hasInventories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasColors READ hasColors CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasYearReleased READ hasYearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasWeight READ hasWeight CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasSubConditions READ hasSubConditions CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, char pictureId READ pictureId CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QSize pictureSize READ pictureSize CONSTANT)

public:
    ItemType(const ::BrickLink::ItemType *itt = nullptr);

    QVariantList categories() const;

    friend class BrickLink;
};


class Color : public WrapperBase<const ::BrickLink::Color>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QColor color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int ldrawId READ ldrawId CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool solid READ isSolid CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool transparent READ isTransparent CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool glitter READ isGlitter CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool speckle READ isSpeckle CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool metallic READ isMetallic CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool chrome READ isChrome CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool milky READ isMilky CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool modulex READ isModulex CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double popularity READ popularity CONSTANT)

    Q_PROPERTY(QImage image READ image)

public:
    Color(const ::BrickLink::Color *col = nullptr);

    QImage image() const;

    friend class BrickLink;
    friend class Document;
    friend class Lot;
};


class Item : public WrapperBase<const ::BrickLink::Item>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, ItemType itemType READ itemType CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, Category category READ category CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasInventory READ hasInventory CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QDateTime inventoryUpdated READ inventoryUpdated CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, Color defaultColor READ defaultColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double weight READ weight CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int yearReleased READ yearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasKnownColors READ hasKnownColors CONSTANT)
    Q_PROPERTY(QVariantList knownColors READ knownColors CONSTANT)

public:
    Item(const ::BrickLink::Item *item = nullptr);

    QVariantList knownColors() const;

    Q_INVOKABLE QVariantList consistsOf() const;

    // tough .. BrickLink::AppearsIn appearsIn(const Color *color = nullptr) const;

    friend class BrickLink;
    friend class Document;
};


class Picture : public WrapperBase<::BrickLink::Picture>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(wrapped, bool isValid READ isValid)
    Q_PRIVATE_PROPERTY(wrapped, QImage image READ image)

public:
    explicit Picture(::BrickLink::Picture *pic = nullptr);
    Picture(const Picture &copy);
    Picture &operator=(const Picture &assign);
    virtual ~Picture();

    BrickLink::UpdateStatus updateStatus() const;

    Q_INVOKABLE void update(bool highPriority = false);

    friend class BrickLink;
};


class PriceGuide : public WrapperBase<::BrickLink::PriceGuide>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(wrapped, bool isValid READ isValid)

public:
    explicit PriceGuide(::BrickLink::PriceGuide *pg = nullptr);
    PriceGuide(const PriceGuide &copy);
    PriceGuide &operator=(const PriceGuide &assign);
    virtual ~PriceGuide();

    BrickLink::UpdateStatus updateStatus() const;

    Q_INVOKABLE void update(bool highPriority = false);

    Q_INVOKABLE int quantity(::BrickLink::Time time, ::BrickLink::Condition condition) const;
    Q_INVOKABLE int lots(::BrickLink::Time time, ::BrickLink::Condition condition) const;
    Q_INVOKABLE double price(::BrickLink::Time time, ::BrickLink::Condition condition,
                             ::BrickLink::Price price) const;

    friend class BrickLink;
};


class Lot : WrapperBase<::Lot>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(Item item READ item WRITE setItem)
    Q_PROPERTY(Color color READ color WRITE setColor)
    Q_PROPERTY(Category category READ category)
    Q_PROPERTY(ItemType itemType READ itemType)
    Q_PROPERTY(QString itemId READ itemId)
    Q_PROPERTY(QString id READ itemId)
    Q_PROPERTY(QString itemName READ itemName)
    Q_PROPERTY(QString name READ itemName)
    Q_PROPERTY(QString colorName READ colorName)
    Q_PROPERTY(QString categoryName READ categoryName)
    Q_PROPERTY(QString itemTypeName READ itemTypeName)
    Q_PROPERTY(int itemYearReleased READ itemYearReleased)

    Q_PROPERTY(BrickLink::Status status READ status WRITE setStatus)
    Q_PROPERTY(BrickLink::Condition condition READ condition WRITE setCondition)
    Q_PROPERTY(BrickLink::SubCondition subCondition READ subCondition WRITE setSubCondition)

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
    Q_PROPERTY(BrickLink::Stockroom stockroom READ stockroom WRITE setStockroom)

    Q_PROPERTY(double totalWeight READ totalWeight WRITE setTotalWeight)
    Q_PROPERTY(QString reserved READ reserved WRITE setReserved)
    Q_PROPERTY(bool alternate READ alternate WRITE setAlternate)
    Q_PROPERTY(uint alternateId READ alternateId WRITE setAlternateId)
    Q_PROPERTY(bool counterPart READ counterPart WRITE setCounterPart)

    Q_PROPERTY(bool incomplete READ incomplete)

    Q_PROPERTY(QImage image READ image)

public:
    Lot(::Lot *lot = nullptr, Document *document = nullptr);

    Item item() const                  { return get()->item(); }
    void setItem(Item item)            { set().to()->setItem(item.wrappedObject()); }
    Color color() const                { return get()->color(); }
    void setColor(Color color)         { set().to()->setColor(color.wrappedObject()); }
    Category category() const          { return get()->category(); }
    ItemType itemType() const          { return get()->itemType(); }

    QString itemId() const             { return get()->itemId(); }
    QString itemName() const           { return get()->itemName(); }
    QString colorName() const          { return get()->colorName(); }
    QString categoryName() const       { return get()->categoryName(); }
    QString itemTypeName() const       { return get()->itemTypeName(); }
    int itemYearReleased() const       { return get()->itemYearReleased(); }

    BrickLink::Status status() const                { return static_cast<BrickLink::Status>(get()->status()); }
    void setStatus(BrickLink::Status s)             { set().to()->setStatus(static_cast<::BrickLink::Status>(s)); }
    BrickLink::Condition condition() const          { return static_cast<BrickLink::Condition>(get()->condition()); }
    void setCondition(BrickLink::Condition c)       { set().to()->setCondition(static_cast<::BrickLink::Condition>(c)); }
    BrickLink::SubCondition subCondition() const    { return static_cast<BrickLink::SubCondition>(get()->subCondition()); }
    void setSubCondition(BrickLink::SubCondition c) { set().to()->setSubCondition(static_cast<::BrickLink::SubCondition>(c)); }
    QString comments() const           { return get()->comments(); }
    void setComments(const QString &n) { set().to()->setComments(n); }
    QString remarks() const            { return get()->remarks(); }
    void setRemarks(const QString &r)  { set().to()->setComments(r); }

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
    BrickLink::Stockroom stockroom() const     { return static_cast<BrickLink::Stockroom>(get()->stockroom()); }
    void setStockroom(BrickLink::Stockroom sr) { set().to()->setStockroom(static_cast<::BrickLink::Stockroom>(sr)); }

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
        Setter(Lot *lot);
        ::Lot *to();
        ~Setter();

    private:
        Lot *m_lot;
        ::Lot m_to;
    };
    Setter set();
    ::Lot *get() const;

    Document *doc = nullptr;

    friend class Document;
    friend class Setter;
};


class Order : public WrapperBase<const ::BrickLink::Order>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, QString id READ id CONSTANT)
    Q_PROPERTY(BrickLink::OrderType type READ type CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QDate date READ date CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QDate statusChange READ statusChange CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString otherParty READ otherParty CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double shipping READ shipping CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double insurance READ insurance CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double additionalCharges1 READ additionalCharges1 CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double additionalCharges2 READ additionalCharges2 CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double credit READ credit CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double creditCoupon READ creditCoupon CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double orderTotal READ orderTotal CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double salesTax READ salesTax CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double grandTotal READ grandTotal CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double vatCharges READ vatCharges CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString currencyCode READ currencyCode CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString paymentCurrencyCode READ paymentCurrencyCode CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int itemCount READ itemCount CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int lotCount READ lotCount CONSTANT)
    Q_PROPERTY(BrickLink::OrderStatus status READ status CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString paymentType READ paymentType CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString trackingNumber READ trackingNumber CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString address READ address CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString countryCode READ countryCode CONSTANT)

public:
    Order(const ::BrickLink::Order *order = nullptr);

    BrickLink::OrderType type() const;
    BrickLink::OrderStatus status() const;

    friend class BrickLink;
    friend class Document;
};


class Document : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PRIVATE_PROPERTY(d, QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PRIVATE_PROPERTY(d, QString currencyCode READ currencyCode NOTIFY currencyCodeChanged)
    Q_PRIVATE_PROPERTY(d, QString filter READ filter NOTIFY filterChanged)
    Q_PRIVATE_PROPERTY(d, Order order READ order CONSTANT)
    //TODO: missing: statistics

public:
    Document(::Document *doc);
    bool isWrapperFor(::Document *doc) const;

    bool changeLot(Lot *from, ::Lot &to);

    int count() const;

    Q_INVOKABLE Lot lot(int index);
    Q_INVOKABLE void deleteLot(Lot ii);
    Q_INVOKABLE Lot addLot(Item item, Color color);

//    Q_INVOKABLE Lot addItem(Lot lot, Flags consolidate)
//    {
//        if (m_lot.doc != this) {
//            ...
//        }
//    }

signals:
    void titleChanged(const QString &title);
    void fileNameChanged(const QString &fileName);
    void countChanged(int count);
    void currencyCodeChanged(const QString &currencyCode);
    void filterChanged(const QString &filter);

private:
    ::Document *d;
};


class BrickStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<Document *> documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(Document *currentDocument READ currentDocument NOTIFY currentDocumentChanged)
    Q_PROPERTY(QString defaultCurrencyCode READ defaultCurrencyCode NOTIFY defaultCurrencyCodeChanged)

public:
    BrickStore();

    QVector<Document *> documents() const;
    Document *currentDocument() const;

    Document *documentForWindow(Window *win) const;

    // the QmlWrapper:: prefix is needed, otherwise moc/qml get the return type wrong
    Q_INVOKABLE QmlWrapper::Document *newDocument(const QString &title);
    Q_INVOKABLE QmlWrapper::Document *openDocument(const QString &fileName);
    Q_INVOKABLE QmlWrapper::Document *importBrickLinkStore(const QString &title = { });

    QString defaultCurrencyCode() const;
    Q_INVOKABLE QString symbolForCurrencyCode(const QString &currencyCode) const;
    Q_INVOKABLE QString toCurrencyString(double value, const QString &symbol = { }, int precision = 3) const;
    Q_INVOKABLE QString toWeightString(double value, bool showUnit = false) const;

signals:
    void documentsChanged(QVector<Document *> documents);
    void currentDocumentChanged(Document *currentDocument);
    void defaultCurrencyCodeChanged(const QString &defaultCurrencyCode);

private:
    Document *setupDocument(Window *win, ::Document *doc, const QString &title = { });

    QVector<Document *> m_documents;
    Document *m_currentDocument = nullptr;
};

} // namespace QmlWrapper

Q_DECLARE_METATYPE(QmlWrapper::Color)
Q_DECLARE_METATYPE(QmlWrapper::Category)
Q_DECLARE_METATYPE(QmlWrapper::ItemType)
Q_DECLARE_METATYPE(QmlWrapper::Item)
Q_DECLARE_METATYPE(QmlWrapper::Lot)
Q_DECLARE_METATYPE(QmlWrapper::Picture)
Q_DECLARE_METATYPE(QmlWrapper::PriceGuide)
Q_DECLARE_METATYPE(QmlWrapper::Order)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Time)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Price)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Condition)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Stockroom)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Status)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::UpdateStatus)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::OrderType)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink *)
Q_DECLARE_METATYPE(QmlWrapper::Document *)
