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

class Window;

namespace QmlWrapper {

class Color;
class Category;
class ItemType;
class Item;
class InvItem;
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
    enum class Time      { PastSix, Current, Count };
    enum class Price     { Lowest, Average, WAverage, Highest, Count };
    enum class Condition { New, Used, Count };
    enum class SubCondition  { None, Complete, Incomplete, Sealed, Count };
    enum class Stockroom { None, A, B, C };
    enum class Status    { Include, Exclude, Extra, Unknown };
    enum class UpdateStatus  { Ok, Updating, UpdateFailed };
    enum class OrderType { Received, Placed, Any };

    Q_ENUM(Time)
    Q_ENUM(Price)
    Q_ENUM(Condition)
    Q_ENUM(SubCondition)
    Q_ENUM(Stockroom)
    Q_ENUM(Status)
    Q_ENUM(UpdateStatus)
    Q_ENUM(OrderType)

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

    Q_PRIVATE_PROPERTY(wrappedObject(), QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QString name READ name CONSTANT)

public:
    Category(const ::BrickLink::Category *cat = nullptr);

    friend class BrickLink;
};


class ItemType : public WrapperBase<const ::BrickLink::ItemType>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrappedObject(), QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QString name READ name CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasInventories READ hasInventories CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasColors READ hasColors CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasYearReleased READ hasYearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasWeight READ hasWeight CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasSubConditions READ hasSubConditions CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), char pictureId READ pictureId CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QSize pictureSize READ pictureSize CONSTANT)

public:
    ItemType(const ::BrickLink::ItemType *itt = nullptr);

    QVariantList categories() const;

    friend class BrickLink;
};


class Color : public WrapperBase<const ::BrickLink::Color>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrappedObject(), int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QColor color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), int ldrawId READ ldrawId CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool solid READ isSolid CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool transparent READ isTransparent CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool glitter READ isGlitter CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool speckle READ isSpeckle CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool metallic READ isMetallic CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool chrome READ isChrome CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool milky READ isMilky CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool modulex READ isModulex CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), double popularity READ popularity CONSTANT)

public:
    Color(const ::BrickLink::Color *col = nullptr);

    friend class BrickLink;
    friend class Document;
    friend class InvItem;
};


class Item : public WrapperBase<const ::BrickLink::Item>
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrappedObject(), QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), ItemType itemType READ itemType CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), Category category READ category CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasInventory READ hasInventory CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QDateTime inventoryUpdated READ inventoryUpdated CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), Color defaultColor READ defaultColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), double weight READ weight CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), int yearReleased READ yearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool hasKnownColors READ hasKnownColors CONSTANT)
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

    Q_PRIVATE_PROPERTY(wrappedObject(), Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool valid READ valid)
    Q_PRIVATE_PROPERTY(wrappedObject(), QImage image READ image)

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

    Q_PRIVATE_PROPERTY(wrappedObject(), Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrappedObject(), QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(wrappedObject(), bool valid READ valid)

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


class InvItem : WrapperBase<::BrickLink::InvItem>
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
    InvItem(::BrickLink::InvItem *invItem = nullptr, Document *document = nullptr);

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

    QImage image() const               { return get()->image(); }

private:
    class Setter
    {
    public:
        Setter(InvItem *invItem);
        ::BrickLink::InvItem *to();
        ~Setter();

    private:
        InvItem *m_invItem;
        ::BrickLink::InvItem m_to;
    };
    Setter set();
    ::BrickLink::InvItem *get() const;

    Document *doc = nullptr;

    friend class Document;
    friend class Setter;
};


class Document : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PRIVATE_PROPERTY(d, QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PRIVATE_PROPERTY(d, QString currencyCode READ currencyCode NOTIFY currencyCodeChanged)

    //TODO: missing: statistics & order

public:
    Document(::Document *doc);
    bool isWrapperFor(::Document *doc) const;

    bool changeItem(InvItem *from, ::BrickLink::InvItem &to);

    int count() const;

    Q_INVOKABLE InvItem invItem(int index);
    Q_INVOKABLE void deleteInvItem(InvItem ii);
    Q_INVOKABLE InvItem addInvItem(Item item, Color color);

//    Q_INVOKABLE InvItem addItem(InvItem invItem, Flags consolidate)
//    {
//        if (invItem.doc != this) {
//            ...
//        }
//    }

signals:
    void titleChanged(const QString &title);
    void fileNameChanged(const QString &fileName);
    void countChanged(int count);
    void currencyCodeChanged(const QString &currencyCode);

private:
    ::Document *d;
};

class DocumentView : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(m_view, QString filter READ filterExpression NOTIFY filterExpressionChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(Document *document READ document CONSTANT)

    //TODO: missing: selection handling (and statistics for selection)

public:
    DocumentView(Document *wrappedDoc, ::DocumentProxyModel *view);
    bool isWrapperFor(::DocumentProxyModel *view) const;

    Q_INVOKABLE int toDocumentIndex(int viewIndex) const;
    Q_INVOKABLE int toViewIndex(int documentIndex) const;

    int count() const;
    Document *document() const;

signals:
    void filterExpressionChanged(const QString &filterExpression);
    void countChanged(int count);

private:
    Document *m_wrappedDoc;
    ::DocumentProxyModel *m_view;
};


class BrickStore : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QVector<DocumentView *> documentViews READ documentViews NOTIFY documentViewsChanged)
    Q_PROPERTY(DocumentView *currentDocumentView READ currentDocumentView NOTIFY currentDocumentViewChanged)

public:
    BrickStore();

    QVector<DocumentView *> documentViews() const;
    DocumentView *currentDocumentView() const;

    DocumentView *documentViewForWindow(Window *win) const;

    // the QmlWrapper:: prefix is needed, otherwise moc/qml get the return type wrong
    Q_INVOKABLE QmlWrapper::DocumentView *newDocument(const QString &title);
    Q_INVOKABLE QmlWrapper::DocumentView *openDocument(const QString &fileName);
    Q_INVOKABLE QmlWrapper::DocumentView *importBrickLinkStore(const QString &title = { });

signals:
    void documentViewsChanged(QVector<DocumentView *> documents);
    void currentDocumentViewChanged(DocumentView *currentDocument);

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    DocumentView *setupDocumentView(::Document *doc, const QString &title = { });

    QVector<DocumentView *> m_documentViews;
    DocumentView *m_currentDocumentView = nullptr;
};

} // namespace QmlWrapper

Q_DECLARE_METATYPE(QmlWrapper::Color)
Q_DECLARE_METATYPE(QmlWrapper::Category)
Q_DECLARE_METATYPE(QmlWrapper::ItemType)
Q_DECLARE_METATYPE(QmlWrapper::Item)
Q_DECLARE_METATYPE(QmlWrapper::InvItem)
Q_DECLARE_METATYPE(QmlWrapper::Picture)
Q_DECLARE_METATYPE(QmlWrapper::PriceGuide)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Time)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Price)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Condition)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Stockroom)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::Status)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::UpdateStatus)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink::OrderType)
Q_DECLARE_METATYPE(QmlWrapper::BrickLink *)
Q_DECLARE_METATYPE(QmlWrapper::Document *)
Q_DECLARE_METATYPE(QmlWrapper::DocumentView *)
