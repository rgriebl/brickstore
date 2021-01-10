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
#include <QQmlListProperty>
#include <QQuickItem>
#include <QAction>

#include "bricklink.h"
#include "document.h"

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)
QT_FORWARD_DECLARE_CLASS(QQmlContext)

namespace QmlWrapper {

class Color;
class Category;
class ItemType;
class Item;
class InvItem;
class PriceGuide;
class Picture;
class Document;

class BrickLink : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QString cachePath READ dataPath CONSTANT)
    Q_PRIVATE_PROPERTY(d, QSize standardPictureSize READ standardPictureSize CONSTANT)
    Q_PRIVATE_PROPERTY(d, qreal itemImageScaleFactor READ itemImageScaleFactor NOTIFY itemImageScaleFactorChanged)
    Q_PRIVATE_PROPERTY(d, bool ldrawEnabled READ isLDrawEnabled CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString ldrawPath READ ldrawDataPath)
    Q_PRIVATE_PROPERTY(d, bool online READ onlineStatus)

    // const QImage noImage(const QSize &s) const;
    // const QImage colorImage(const Color *col, int w, int h) const;

    Q_PROPERTY(Item noItem READ noItem CONSTANT)
    Q_PROPERTY(Color noColor READ noColor CONSTANT)

public:
    BrickLink(::BrickLink::Core *core);

    enum class UpdateStatus  { Ok, Updating, UpdateFailed };
    Q_ENUM(UpdateStatus)

    Item noItem() const;
    Color noColor() const;

    //Q_ENUM(::BrickLink::UpdateStatus)

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
    void itemImageScaleFactorChanged(qreal scaleFactor);
    void priceGuideUpdated(PriceGuide pg);
    void pictureUpdated(Picture inv);

private:
    static char firstCharInString(const QString &str);

    ::BrickLink::Core *d;
};

class Category
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString name READ name CONSTANT)

public:
    Category(const ::BrickLink::Category *cat = nullptr)
        : d(cat ? cat : dummyD())
    { }

private:
    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::Category *dummyD()
    {
        static ::BrickLink::Category dd(nullptr);
        return &dd;
    }

private:
    const ::BrickLink::Category *d;
    friend class BrickLink;
};

class ItemType
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString name READ name CONSTANT)
    Q_PROPERTY(QVector<Category> categories READ categories CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasInventories READ hasInventories CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasColors READ hasColors CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasYearReleased READ hasYearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasWeight READ hasWeight CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasSubConditions READ hasSubConditions CONSTANT)
    Q_PRIVATE_PROPERTY(d, char pictureId READ pictureId CONSTANT)
    Q_PRIVATE_PROPERTY(d, QSize pictureSize READ pictureSize CONSTANT)

public:
    ItemType(const ::BrickLink::ItemType *itt = nullptr)
        : d(itt ? itt : dummyD())
    { }

private:
    QVector<Category> categories() const;

    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::ItemType *dummyD()
    {
        static ::BrickLink::ItemType dd(nullptr);
        return &dd;
    }

private:
    const ::BrickLink::ItemType *d;
    friend class BrickLink;
};


class Color
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, int id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(d, QColor color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(d, int ldrawId READ ldrawId CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool solid READ isSolid CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool transparent READ isTransparent CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool glitter READ isGlitter CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool speckle READ isSpeckle CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool metallic READ isMetallic CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool chrome READ isChrome CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool milky READ isMilky CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool modulex READ isModulex CONSTANT)
    Q_PRIVATE_PROPERTY(d, double popularity READ popularity CONSTANT)

public:
    inline const ::BrickLink::Color *d_ptr() const { return isNull() ? nullptr : d; }

    Color(const ::BrickLink::Color *col = nullptr)
        : d(col ? col : dummyD())
    { }

private:
    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::Color *dummyD()
    {
        static ::BrickLink::Color dd(nullptr);
        return &dd;
    }

private:
    const ::BrickLink::Color *d;
    friend class BrickLink;
    friend class Document;
};

class InvItem;

class Item
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, QString id READ id CONSTANT)
    Q_PRIVATE_PROPERTY(d, QString name READ name CONSTANT)
    Q_PRIVATE_PROPERTY(d, ItemType itemType READ itemType CONSTANT)
    Q_PRIVATE_PROPERTY(d, Category category READ category CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasInventory READ hasInventory CONSTANT)
    Q_PRIVATE_PROPERTY(d, QDateTime inventoryUpdated READ inventoryUpdated CONSTANT)
    Q_PRIVATE_PROPERTY(d, Color defaultColor READ defaultColor CONSTANT)
    Q_PRIVATE_PROPERTY(d, double weight READ weight CONSTANT)
    Q_PRIVATE_PROPERTY(d, int yearReleased READ yearReleased CONSTANT)
    Q_PRIVATE_PROPERTY(d, bool hasKnownColors READ hasKnownColors CONSTANT)
    Q_PROPERTY(QVector<Color> knownColors READ knownColors CONSTANT)

    // tough .. BrickLink::AppearsIn appearsIn(const Color *color = nullptr) const;
    Q_INVOKABLE QVariantList consistsOf() const;

    QVector<Color> knownColors() const;

public:
    inline const ::BrickLink::Item *d_ptr() const { return isNull() ? nullptr : d; }

    Item(const ::BrickLink::Item *item = nullptr)
        : d(item ? item : dummyD())
    { }

private:
    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::Item *dummyD()
    {
        static ::BrickLink::Item dd(nullptr);
        return &dd;
    }

private:
    const ::BrickLink::Item *d;
    friend class BrickLink;
    friend class Document;
};


class Picture
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(d, Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(d, QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(d, bool valid READ valid)
    Q_PRIVATE_PROPERTY(d, QImage image READ image)

    BrickLink::UpdateStatus updateStatus() const
    {
        return static_cast<BrickLink::UpdateStatus>(d->updateStatus());
    }

public:
    Q_INVOKABLE void update(bool highPriority = false)
    {
        d->update(highPriority);
    }

    explicit Picture(::BrickLink::Picture *pic = nullptr)
        : d(pic ? pic : dummyD())
    {
        if (!isNull())
            d->addRef();
    }

    virtual ~Picture()
    {
        if (!isNull())
            d->release();
    }

private:
    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::Picture *dummyD()
    {
        static ::BrickLink::Picture dd(nullptr);
        return &dd;
    }

private:
    ::BrickLink::Picture *d;
};



class PriceGuide
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PRIVATE_PROPERTY(d, Item item READ item CONSTANT)
    Q_PRIVATE_PROPERTY(d, Color color READ color CONSTANT)
    Q_PRIVATE_PROPERTY(d, QDateTime lastUpdate READ lastUpdate)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus)
    Q_PRIVATE_PROPERTY(d, bool valid READ valid)

    BrickLink::UpdateStatus updateStatus() const
    {
        return static_cast<BrickLink::UpdateStatus>(d->updateStatus());
    }

public:
    Q_INVOKABLE void update(bool highPriority = false)
    {
        d->update(highPriority);
    }

    Q_INVOKABLE int quantity(::BrickLink::Time time, ::BrickLink::Condition condition) const
    {
        return d->quantity(time, condition);
    }

    Q_INVOKABLE int lots(::BrickLink::Time time, ::BrickLink::Condition condition) const
    {
        return d->lots(time, condition);
    }

    Q_INVOKABLE double price(::BrickLink::Time time, ::BrickLink::Condition condition,
                             ::BrickLink::Price price) const
    {
        return d->price(time, condition, price);
    }

    explicit PriceGuide(::BrickLink::PriceGuide *pic = nullptr)
        : d(pic ? pic : dummyD())
    {
        if (!isNull())
            d->addRef();
    }

    virtual ~PriceGuide()
    {
        if (!isNull())
            d->release();
    }

private:
    bool isNull() const { return d == dummyD(); }
    static ::BrickLink::PriceGuide *dummyD()
    {
        static ::BrickLink::PriceGuide dd(nullptr);
        return &dd;
    }

private:
    ::BrickLink::PriceGuide *d;
};


class InvItem
{
    Q_GADGET
    Q_PROPERTY(bool isNull READ isNull)

    Q_PROPERTY(Item item READ item WRITE setItem)
    Q_PROPERTY(Color color READ color WRITE setColor)

    Q_PROPERTY(int quantity READ quantity WRITE setQuantity)



public:
    InvItem(::BrickLink::InvItem *invItem = nullptr, Document *document = nullptr)
        : d(invItem ? invItem : dummyD())
        , doc(document)
    { }

    inline Item item() const  { return d->item(); }
    inline Color color() const  { return d->color(); }

    inline int quantity() const { return d->quantity(); }

public slots:
    void setItem(Item item);
    void setColor(Color item);
    void setQuantity(int q);

private:
    bool isNull() const { return d == dummyD(); }
    static ::Document::Item *dummyD()
    {
        static ::Document::Item dd;
        return &dd;
    }
    class Setter
    {
    public:
        Setter(InvItem *invItem);
        ::BrickLink::InvItem &to();
        ~Setter();

    private:
        InvItem *m_invItem;
        ::BrickLink::InvItem m_to;
    };
    Setter setter();

private:
    ::BrickLink::InvItem *d;
    Document *doc = nullptr;

    friend class Document;
    friend class Setter;
};





#if 0
    const Category *category() const   { return m_item ? m_item->category() : nullptr; }
    const ItemType *itemType() const   { return m_item ? m_item->itemType() : nullptr; }

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

    typedef void *Diff; // opaque handle

    Diff *createDiff(const InvItem &diffto) const;
    bool applyDiff(Diff *diff);
#endif

class Document : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PRIVATE_PROPERTY(d, QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PRIVATE_PROPERTY(dpm, QString filter READ filterExpression WRITE setFilterExpression NOTIFY filterExpressionChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    Document(::Document *doc, ::DocumentProxyModel *view);
    bool isWrapperFor(::Document *doc, ::DocumentProxyModel *view) const;

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
    void filterExpressionChanged(const QString &filterExpression);
    void countChanged(int count);

private:
    ::Document *d;
    ::DocumentProxyModel *dpm;
};

class BrickStore : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QVector<Document *> documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(Document *currentDocument READ currentDocument NOTIFY currentDocumentChanged)

public:
    BrickStore();

    QVector<Document *> documents() const;
    Document * currentDocument() const;

    // the QmlWrapper:: prefix is needed, otherwise moc/qml get the return type wrong
    Q_INVOKABLE QmlWrapper::Document *newDocument(const QString &title);
    Q_INVOKABLE QmlWrapper::Document *openDocument(const QString &fileName);
    Q_INVOKABLE QmlWrapper::Document *importBrickLinkStore(const QString &title = { });

signals:
    void documentsChanged(QVector<Document *> documents);
    void currentDocumentChanged(Document * currentDocument);

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    Document *documentForWindow(Window *win) const;
    Document *setupDocument(::Document *doc, const QString &title);

    QVector<Document *> m_documents;
    Document * m_currentDocument = nullptr;
};

class ScriptMenuAction : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(Location location READ location WRITE setLocation NOTIFY locationChanged)

public:
    enum class Location {
        ExtrasMenu,
        ContextMenu,
    };
    Q_ENUM(Location)

    QString text() const;
    void setText(const QString &text);
    Location location() const;
    void setLocation(Location type);


signals:
    void textChanged(const QString &text);
    void locationChanged(Location type);

    void triggered();

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    void attachAction();

private:
    QString m_text;
    Location m_type = Location::ExtrasMenu;
    QAction *m_action = nullptr;
};


class Script : public QQuickItem
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString author READ author WRITE setAuthor NOTIFY authorChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)

public:
    enum class Type {
        ExtensionScript,
        PrintingScript,
    };
    Q_ENUM(Type)

    QString name() const;
    QString author() const;
    void setName(QString name);
    void setAuthor(QString author);
    QString version() const;
    void setVersion(QString version);
    Type type() const;
    void setType(Type type);

    static Script *load(QQmlEngine *engine, const QString &filename);

    QList<ScriptMenuAction *> menuEntries() const;

signals:
    void nameChanged(QString name);
    void authorChanged(QString author);
    void versionChanged(QString version);
    void typeChanged(Type type);

private:
    QString m_name;
    QString m_author;
    QString m_version;
    Type m_type;

    // C++ side
    QQmlContext *m_context = nullptr;
    QQmlComponent *m_component = nullptr;
    QString m_fileName;

    QList<ScriptMenuAction *> m_menuEntries;
    friend class ScriptMenuAction;
};


class ScriptManager : public QObject
{
    Q_OBJECT

private:
    ScriptManager();
    static ScriptManager *s_inst;

public:
    ~ScriptManager();
    static ScriptManager *inst();

    bool initialize(::BrickLink::Core *core);

    bool reload();

    QList<Script *> scripts() const;

private:
    Q_DISABLE_COPY(ScriptManager)

    QQmlEngine *m_engine;
    QList<Script *> m_scripts;

    BrickLink *m_bricklink = nullptr;
    BrickStore *m_brickstore = nullptr;
};

Q_DECLARE_METATYPE(Color)
Q_DECLARE_METATYPE(Category)
Q_DECLARE_METATYPE(ItemType)
Q_DECLARE_METATYPE(Item)
Q_DECLARE_METATYPE(InvItem)
Q_DECLARE_METATYPE(QmlWrapper::Picture)
Q_DECLARE_METATYPE(QmlWrapper::PriceGuide)

} // namespace QmlWrapper




Q_DECLARE_METATYPE(QmlWrapper::BrickLink *)
Q_DECLARE_METATYPE(QmlWrapper::Document *)
Q_DECLARE_METATYPE(QmlWrapper::ScriptMenuAction *)
Q_DECLARE_METATYPE(QmlWrapper::Script *)
