// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QIdentityProxyModel>
#include <QtQml/QQmlEngine>

#include "core.h"
#include "color.h"
#include "itemtype.h"
#include "category.h"
#include "item.h"
#include "lot.h"
#include "model.h"
#include "store.h"
#include "order.h"
#include "wantedlist.h"
#include "cart.h"


class QmlDocumentLots;

namespace BrickLink {

class QmlColor;
class QmlItemType;
class QmlCategory;
class QmlItem;
class QmlLot;

//namespace QmlTime {
//Q_NAMESPACE
//QML_NAMED_ELEMENT(Time)
//enum class Time          { PastSix, Current };
//Q_ENUM_NS(Time)
//}

class QmlBrickLink : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BrickLink)
    QML_SINGLETON
    QML_EXTENDED_NAMESPACE(BrickLink)
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    Q_CLASSINFO("bsNoStringify", "noItem,noColor,noLot")

    Q_PRIVATE_PROPERTY(core(), QString cachePath READ dataPath CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), QSize standardPictureSize READ standardPictureSize CONSTANT FINAL)

    Q_PROPERTY(BrickLink::QmlItem noItem READ noItem CONSTANT FINAL)
    Q_PROPERTY(BrickLink::QmlColor noColor READ noColor CONSTANT FINAL)
    Q_PROPERTY(BrickLink::QmlLot noLot READ noLot CONSTANT FINAL)
    Q_PROPERTY(QVariantList colorTypes READ colorTypes CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), BrickLink::Store *store READ store CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), BrickLink::Orders *orders READ orders CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), BrickLink::Carts *carts READ carts CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), BrickLink::WantedLists *wantedLists READ wantedLists CONSTANT FINAL)
    Q_PRIVATE_PROPERTY(core(), BrickLink::Database *database READ database CONSTANT FINAL)
    Q_PROPERTY(BrickLink::VatType currentVatType READ currentVatType WRITE setCurrentVatType NOTIFY currentVatTypeChanged FINAL)
    Q_PROPERTY(QVariantList supportedVatTypes READ supportedVatTypes CONSTANT FINAL)
    Q_PROPERTY(QVariantList knownApiQuirks READ knownApiQuirks CONSTANT FINAL)

public:
    QmlBrickLink();

    QmlItem noItem() const;
    QmlColor noColor() const;
    QmlLot noLot() const;
    QVariantList colorTypes() const;

    Q_INVOKABLE QImage noImage(int width, int height) const;
    Q_INVOKABLE QString colorTypeName(BrickLink::ColorTypeFlag colorType) const;

    Q_INVOKABLE BrickLink::QmlColor color(const QVariant &v) const;
    Q_INVOKABLE BrickLink::QmlColor colorFromLDrawId(int ldrawId) const;
    Q_INVOKABLE BrickLink::QmlCategory category(const QVariant &v) const;
    Q_INVOKABLE BrickLink::QmlItemType itemType(const QVariant &v) const;
    Q_INVOKABLE BrickLink::QmlItem item(const QVariant &v) const;
    Q_INVOKABLE BrickLink::QmlItem item(const QString &itemTypeId, const QString &itemId) const;

    Q_INVOKABLE BrickLink::PriceGuide *priceGuide(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                                  bool highPriority = false);
    Q_INVOKABLE BrickLink::PriceGuide *priceGuide(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                                  BrickLink::VatType vatType, bool highPriority = false);

    Q_INVOKABLE BrickLink::Picture *picture(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                            bool highPriority = false);

    Q_INVOKABLE BrickLink::QmlLot lot(const QVariant &v) const;

    Q_INVOKABLE BrickLink::InventoryModel *inventoryModel(BrickLink::InventoryModel::Mode mode,
                                                          const QVariantList &simpleLots);

    Q_INVOKABLE QString itemHtmlDescription(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                            const QColor &highlight) const;

    BrickLink::VatType currentVatType() const;
    void setCurrentVatType(BrickLink::VatType vatType);
    QVariantList supportedVatTypes() const;
    Q_INVOKABLE QString descriptionForVatType(BrickLink::VatType vatType) const;
    Q_INVOKABLE QString iconForVatType(BrickLink::VatType vatType) const;

    QVariantList knownApiQuirks() const;
    Q_INVOKABLE bool isApiQuirkActive(BrickLink::ApiQuirk apiQuirk) const;
    Q_INVOKABLE QString apiQuirkDescription(BrickLink::ApiQuirk apiQuirk) const;
    Q_INVOKABLE void setApiQuirkActive(BrickLink::ApiQuirk apiQuirk, bool active);

signals:
    void priceGuideUpdated(BrickLink::PriceGuide *priceGuide);
    void pictureUpdated(BrickLink::Picture *picture);
    void currentVatTypeChanged(BrickLink::VatType vatType);
    void transferProgress(int progress, int total);

private:
    static char firstCharInString(const QString &str);
    inline Core *core() { return BrickLink::core(); }
    inline const Core *core() const { return BrickLink::core(); }
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


template <typename T> class QmlWrapperBase
{
public:
    inline T *wrappedObject() const
    {
        return (wrapped == wrappedNull()) ? nullptr : wrapped;
    }
    inline bool isNull() const { return !wrappedObject(); }

    inline bool operator==(const QmlWrapperBase &other) const = default;

protected:
    QmlWrapperBase(T *_wrappedObject)
        : wrapped(_wrappedObject ? _wrappedObject : wrappedNull())
    { }

    static T *wrappedNull()
    {
        static T t_null(nullptr);
        return &t_null;
    }
    T *wrapped;
};

template <typename T>
QDebug &operator<<(QDebug &dbg, const QmlWrapperBase<T> &wrapper)
{
    return dbg << "W(" << wrapper.wrappedObject() << ")";
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlColor : public QmlWrapperBase<const Color>
{
    Q_GADGET
    QML_NAMED_ELEMENT(Color)
    QML_UNCREATABLE("")
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QColor color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int ldrawId READ ldrawId CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QColor ldrawColor READ ldrawColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QColor ldrawEdgeColor READ ldrawEdgeColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool solid READ isSolid CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool transparent READ isTransparent CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool glitter READ isGlitter CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool speckle READ isSpeckle CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool metallic READ isMetallic CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool chrome READ isChrome CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool pearl READ isPearl CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool milky READ isMilky CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool modulex READ isModulex CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool satin READ isSatin CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double popularity READ popularity CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, float luminance READ luminance CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool particles READ hasParticles CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, float particleMinSize READ particleMinSize CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, float particleMaxSize READ particleMaxSize CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, float particleFraction READ particleFraction CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, float particleVFraction READ particleVFraction CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QColor particleColor READ particleColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, BrickLink::ColorType type READ type CONSTANT)

    Q_PROPERTY(QImage image READ image CONSTANT) // for print-scripts backwards compatibility

public:
    QmlColor(const Color *col = nullptr);

    QImage image() const;
    Q_INVOKABLE QImage sampleImage(int width, int height) const;

    friend class QmlBrickLink;
    friend class QmlLot;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlColorModel : public QIdentityProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ColorModel)
    Q_PROPERTY(float popularityFilter READ popularityFilter WRITE setPopuplarityFilter NOTIFY popularityFilterChanged)
    Q_PROPERTY(BrickLink::ColorType colorTypeFilter READ colorTypeFilter WRITE setColorTypeFilter NOTIFY colorTypeFilterChanged)
    Q_PROPERTY(QVariantList colorListFilter READ colorListFilter WRITE setColorListFilter NOTIFY colorListFilterChanged)

public:
    QmlColorModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QModelIndex indexOfColor(BrickLink::QmlColor color);

    Q_INVOKABLE void sortByName();
    Q_INVOKABLE void sortByHue();

    Q_INVOKABLE void clearFilters();

    float popularityFilter() const;
    void setPopuplarityFilter(float p);
    ColorType colorTypeFilter() const;
    void setColorTypeFilter(ColorType ct);
    QVariantList colorListFilter() const;
    void setColorListFilter(const QVariantList &colors);

signals:
    void popularityFilterChanged();
    void colorTypeFilterChanged();
    void colorListFilterChanged();

private:
    ColorModel *m_model;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlItemType : public QmlWrapperBase<const ItemType>
{
    Q_GADGET
    QML_NAMED_ELEMENT(ItemType)
    QML_UNCREATABLE("")
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasInventories READ hasInventories CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasColors READ hasColors CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasWeight READ hasWeight CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasSubConditions READ hasSubConditions CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QSize pictureSize READ pictureSize CONSTANT)

public:
    QmlItemType(const ItemType *itt = nullptr);

    QString id() const;
    QVariantList categories() const;

    friend class QmlBrickLink;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlCategory : public QmlWrapperBase<const Category>
{
    Q_GADGET
    QML_NAMED_ELEMENT(Category)
    QML_UNCREATABLE("")
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(wrapped, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)

public:
    QmlCategory(const Category *cat = nullptr);

    friend class QmlBrickLink;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlItem : public QmlWrapperBase<const Item>
{
    Q_GADGET
    QML_NAMED_ELEMENT(Item)
    QML_UNCREATABLE("")
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, BrickLink::QmlItemType itemType READ itemType CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, BrickLink::QmlCategory category READ category CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, bool hasInventory READ hasInventory CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, BrickLink::QmlColor defaultColor READ defaultColor CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, double weight READ weight CONSTANT)
    Q_PRIVATE_PROPERTY(wrapped, int yearReleased READ yearReleased CONSTANT)
    Q_PROPERTY(QVariantList knownColors READ knownColors CONSTANT)
    Q_PROPERTY(QStringList alternateIds READ alternateIds CONSTANT)

public:
    QmlItem(const Item *item = nullptr);

    QString id() const;
    Q_INVOKABLE bool hasKnownColor(BrickLink::QmlColor color) const;
    QVariantList knownColors() const;
    QStringList alternateIds() const;

    Q_INVOKABLE QVariantList consistsOf() const;
    Q_INVOKABLE BrickLink::PartOutTraits partOutTraits() const;

    // tough .. BrickLink::AppearsIn appearsIn(const Color *color = nullptr) const;

    friend class QmlBrickLink;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class QmlBrickLink;

class QmlLot : public QmlWrapperBase<Lot>
{
    Q_GADGET
    QML_NAMED_ELEMENT(Lot)
    QML_UNCREATABLE("")
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(BrickLink::QmlItem item READ item WRITE setItem)
    Q_PROPERTY(BrickLink::QmlColor color READ color WRITE setColor)
    Q_PROPERTY(BrickLink::QmlCategory category READ category)
    Q_PROPERTY(BrickLink::QmlItemType itemType READ itemType)
    Q_PROPERTY(QString itemId READ itemId)
    Q_PROPERTY(QString id READ itemId)
    Q_PROPERTY(QString itemName READ itemName)
    Q_PROPERTY(QString name READ itemName)
    Q_PROPERTY(QString colorName READ colorName)
    Q_PROPERTY(QString categoryName READ categoryName)
    Q_PROPERTY(QString itemTypeName READ itemTypeName)
    Q_PROPERTY(int itemYearReleased READ itemYearReleased)
    Q_PROPERTY(int itemYearLastProduced READ itemYearLastProduced)

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
    Q_PROPERTY(double cost READ cost WRITE setCost)

    Q_PROPERTY(uint lotId READ lotId WRITE setLotId)
    Q_PROPERTY(bool retain READ retain WRITE setRetain)
    Q_PROPERTY(BrickLink::Stockroom stockroom READ stockroom WRITE setStockroom)

    Q_PROPERTY(double totalWeight READ totalWeight WRITE setTotalWeight)
    Q_PROPERTY(double weight READ weight WRITE setWeight)
    Q_PROPERTY(QString reserved READ reserved WRITE setReserved)
    Q_PROPERTY(bool alternate READ alternate WRITE setAlternate)
    Q_PROPERTY(uint alternateId READ alternateId WRITE setAlternateId)
    Q_PROPERTY(bool counterPart READ counterPart WRITE setCounterPart)

    Q_PROPERTY(QDateTime dateAdded READ dateAdded WRITE setDateAdded)
    Q_PROPERTY(QDateTime dateLastSold READ dateLastSold WRITE setDateLastSold)

    Q_PROPERTY(bool isMarked READ isMarked)
    Q_PROPERTY(QString markerText READ markerText WRITE setMarkerText)
    Q_PROPERTY(QColor markerColor READ markerColor WRITE setMarkerColor)

    Q_PROPERTY(bool incomplete READ incomplete)

    Q_PROPERTY(QImage image READ image)

public:
    QmlLot(Lot *lot = nullptr, ::QmlDocumentLots *documentLots = nullptr);
    QmlLot(const Lot *lot);
    QmlLot(const QmlLot &copy);
    QmlLot(QmlLot &&move) noexcept;
    ~QmlLot();

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
    int itemYearLastProduced() const   { return get()->itemYearLastProduced(); }

    BrickLink::Status status() const                { return get()->status(); }
    void setStatus(BrickLink::Status s)             { set().to()->setStatus(s); }
    BrickLink::Condition condition() const          { return get()->condition(); }
    void setCondition(BrickLink::Condition c)       { set().to()->setCondition(c); }
    BrickLink::SubCondition subCondition() const    { return get()->subCondition(); }
    void setSubCondition(BrickLink::SubCondition c) { set().to()->setSubCondition(c); }
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
    double cost() const                { return get()->cost(); }
    void setCost(double c)             { set().to()->setCost(c); }

    uint lotId() const                 { return get()->lotId(); }
    void setLotId(uint lid)            { set().to()->setLotId(lid); }

    bool retain() const                { return get()->retain(); }
    void setRetain(bool r)             { set().to()->setRetain(r); }
    BrickLink::Stockroom stockroom() const     { return get()->stockroom(); }
    void setStockroom(BrickLink::Stockroom sr) { set().to()->setStockroom(sr); }

    double weight() const              { return get()->weight(); }
    double totalWeight() const         { return get()->totalWeight(); }
    void setWeight(double w)           { set().to()->setWeight(w); }
    void setTotalWeight(double w)      { set().to()->setTotalWeight(w); }

    QString reserved() const           { return get()->reserved(); }
    void setReserved(const QString &r) { set().to()->setReserved(r); }

    bool alternate() const             { return get()->alternate(); }
    void setAlternate(bool a)          { set().to()->setAlternate(a); }
    uint alternateId() const           { return get()->alternateId(); }
    void setAlternateId(uint aid)      { set().to()->setAlternateId(aid); }

    bool counterPart() const           { return get()->counterPart(); }
    void setCounterPart(bool b)        { set().to()->setCounterPart(b); }

    QDateTime dateAdded() const        { return get()->dateAdded(); }
    void setDateAdded(const QDateTime &dt)    { set().to()->setDateAdded(dt); }
    QDateTime dateLastSold() const     { return get()->dateLastSold(); }
    void setDateLastSold(const QDateTime &dt) { set().to()->setDateLastSold(dt); }

    bool isMarked() const              { return !markerText().isEmpty() || markerColor().isValid(); }
    QString markerText() const         { return get()->markerText(); }
    QColor markerColor() const         { return get()->markerColor(); }
    void setMarkerText(const QString &text)  { set().to()->setMarkerText(text); }
    void setMarkerColor(const QColor &color) { set().to()->setMarkerColor(color); }

    bool incomplete() const            { return get()->isIncomplete(); }

    QImage image() const;

    using QmlSetterCallback = std::function<void(::QmlDocumentLots * /*lots*/, Lot * /*which*/,
                                                 const Lot & /*value*/)> ;

    static void setQmlSetterCallback(const QmlSetterCallback &callback);

private:
    class Setter
    {
    public:
        Setter(QmlLot *lot);
        Setter(const Setter &copy) = delete;
        Setter(Setter &&move) = delete;
        Lot *to();
        ~Setter();

    private:
        QmlLot *m_lot;
        Lot m_to;
    };
    Setter set();
    Lot *get() const;

    static QmlSetterCallback s_changeLot;
    QmlDocumentLots *m_documentLots = nullptr;
    constexpr static quintptr Owning = 1u;

    friend class Setter;
    friend class ::QmlDocumentLots;
};

} // namespace BrickLink
