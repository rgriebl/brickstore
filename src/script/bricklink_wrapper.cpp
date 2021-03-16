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

#include <QQmlEngine>
#include <QQmlContext>

#include "framework.h"
#include "window.h"
#include "documentio.h"
#include "utility.h"
#include "currency.h"

#include "bricklink_wrapper.h"


namespace QmlWrapper {

static bool isReadOnly(QObject *obj)
{
    //TODO: engine == null for Document, which makes this mostly useless
    //      BrickStore does have an engine and a context, so maybe we can have two sets of
    //      Document wrappers: one read-write and the other read-only?
    auto engine = obj ? qmlEngine(obj) : nullptr;
    return engine ? engine->rootContext()->property("readOnlyContext").toBool() : false;
}


BrickLink::BrickLink(::BrickLink::Core *core)
    : d(core)
{
    setObjectName(QLatin1String("BrickLink"));

    connect(core, &::BrickLink::Core::priceGuideUpdated,
            this, [this](::BrickLink::PriceGuide *pg) {
        emit priceGuideUpdated(PriceGuide(pg));
    });
    connect(core, &::BrickLink::Core::pictureUpdated,
            this, [this](::BrickLink::Picture *pic) {
        emit pictureUpdated(Picture(pic));
    });
}

Item BrickLink::noItem() const
{
    return Item { };
}

Color BrickLink::noColor() const
{
    return Color { };
}

Color BrickLink::color(int id) const
{
    return d->color(uint(id));
}

Color BrickLink::colorFromName(const QString &name) const
{
    return d->colorFromName(name);
}

Color BrickLink::colorFromLDrawId(int ldrawId) const
{
    return d->colorFromLDrawId(ldrawId);
}

Category BrickLink::category(int id) const
{
    return d->category(uint(id));
}

ItemType BrickLink::itemType(const QString &itemTypeId) const
{
    return d->itemType(firstCharInString(itemTypeId));
}

Item BrickLink::item(const QString &itemTypeId, const QString &itemId) const
{
    return d->item(firstCharInString(itemTypeId), itemId.toLatin1());
}

PriceGuide BrickLink::priceGuide(Item item, Color color, bool highPriority)
{
    return PriceGuide { d->priceGuide(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

Picture BrickLink::picture(Item item, Color color, bool highPriority)
{
    return Picture { d->picture(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

Picture BrickLink::largePicture(Item item, bool highPriority)
{
    return Picture { d->largePicture(item.wrappedObject(), highPriority) };
}

char BrickLink::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Category::Category(const ::BrickLink::Category *cat)
    : WrapperBase(cat)
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ItemType::ItemType(const ::BrickLink::ItemType *itt)
    : WrapperBase(itt)
{ }

QVariantList ItemType::categories() const
{
    auto cats = wrapped->categories();
    QVariantList result;
    for (auto cat : cats)
        result.append(QVariant::fromValue(Category { cat }));
    return result;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Color::Color(const ::BrickLink::Color *col)
    : WrapperBase(col)
{ }

QImage Color::image() const
{
    return ::BrickLink::core()->colorImage(wrapped, 20, 20);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Item::Item(const ::BrickLink::Item *item)
    : WrapperBase(item)
{ }

QString Item::id() const
{
    return QString::fromLatin1(wrapped->id());
}

QVariantList Item::knownColors() const
{
    auto known = wrapped->knownColors();
    QVariantList result;
    for (auto c : known)
        result.append(QVariant::fromValue(Color { c }));
    return result;
}

QVariantList Item::consistsOf() const
{
//    const auto consists = wrapped->consistsOf();
//    QVariantList result;
//    for (auto auto &co : consists)
//        result << QVariant::fromValue(Lot { nullptr, nullptr });
//    return result;
    return { };
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Picture::Picture(::BrickLink::Picture *pic)
    : WrapperBase(pic)
{
    if (!isNull())
        wrappedObject()->addRef();
}

Picture::Picture(const Picture &copy)
    : Picture(copy.wrappedObject())
{ }

Picture &Picture::operator=(const Picture &assign)
{
    this->~Picture();
    WrapperBase::operator=(assign);
    return *new (this) Picture(assign.wrappedObject());
}

Picture::~Picture()
{
    if (!isNull())
        wrappedObject()->release();
}

BrickLink::UpdateStatus Picture::updateStatus() const
{
    return static_cast<BrickLink::UpdateStatus>(wrapped->updateStatus());
}

void Picture::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


PriceGuide::PriceGuide(::BrickLink::PriceGuide *pg)
    : WrapperBase(pg)
{
    if (!isNull())
        wrappedObject()->addRef();
}

PriceGuide::PriceGuide(const PriceGuide &copy)
    : PriceGuide(copy.wrappedObject())
{ }

PriceGuide &PriceGuide::operator=(const PriceGuide &assign)
{
    this->~PriceGuide();
    WrapperBase::operator=(assign);
    return *new (this) PriceGuide(assign.wrappedObject());
}

PriceGuide::~PriceGuide()
{
    if (!isNull())
        wrappedObject()->release();
}

BrickLink::UpdateStatus PriceGuide::updateStatus() const
{
    return static_cast<BrickLink::UpdateStatus>(wrapped->updateStatus());
}

void PriceGuide::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}

int PriceGuide::quantity(::BrickLink::Time time, ::BrickLink::Condition condition) const
{
    return wrapped->quantity(time, condition);
}

int PriceGuide::lots(::BrickLink::Time time, ::BrickLink::Condition condition) const
{
    return wrapped->lots(time, condition);
}

double PriceGuide::price(::BrickLink::Time time, ::BrickLink::Condition condition,
                         ::BrickLink::Price price) const
{
    return wrapped->price(time, condition, price);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Lot::Lot(::Lot *lot, Document *document)
    : WrapperBase(lot)
    , doc(document)
{ }

QImage Lot::image() const
{
    auto pic = ::BrickLink::core()->picture(get()->item(), get()->color(), true);
    return pic->image();
}

//Item Lot::item() const
//{
//    return wrapped->item();
//}

//void Lot::setItem(Item item)
//{
//    set().to().setItem(item.wrappedObject());
//}

//Color Lot::color() const
//{
//    return wrapped->color();
//}

//void Lot::setColor(Color color)
//{
//    set().to().setColor(color.wrappedObject());
//}

//int Lot::quantity() const
//{
//    return wrapped->quantity();
//}

//void Lot::setQuantity(int q)
//{
//    set().to().setQuantity(q);
//}

Lot::Setter::Setter(Lot *lot)
    : m_lot((lot && !lot->isNull()) ? lot : nullptr)
{
    if (m_lot)
        m_to = *m_lot->wrapped;
}

::Lot *Lot::Setter::to()
{
    return &m_to;
}

Lot::Setter::~Setter()
{
    if (m_lot && (*m_lot->wrapped != m_to))
        m_lot->doc->changeLot(m_lot, m_to);
}

Lot::Setter Lot::set()
{
    return Setter(this);
}

::Lot *Lot::get() const
{
    return wrapped;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Order::Order(const ::BrickLink::Order *order)
    : WrapperBase(order)
{ }

BrickLink::OrderType Order::type() const
{
    return static_cast<BrickLink::OrderType>(wrapped->type());
}

BrickLink::OrderStatus Order::status() const
{
    return static_cast<BrickLink::OrderStatus>(wrapped->status());
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Document::Document(::Document *doc)
    : d(doc)
{
    connect(doc, &::Document::fileNameChanged,
            this, &Document::fileNameChanged);
    connect(doc, &::Document::titleChanged,
            this, &Document::titleChanged);
    connect(doc, &::Document::currencyCodeChanged,
            this, &Document::currencyCodeChanged);

    connect(doc, &::Document::filterChanged,
            this, &Document::filterChanged);

    connect(doc, &QAbstractItemModel::rowsInserted,
            this, [this]() { emit countChanged(d->rowCount()); });
    connect(doc, &QAbstractItemModel::rowsRemoved,
            this, [this]() { emit countChanged(d->rowCount()); });
    connect(doc, &QAbstractItemModel::layoutChanged,
            this, [this]() { emit countChanged(d->rowCount()); });
}

bool Document::isWrapperFor(::Document *doc) const
{
    return (d == doc);
}

bool Document::changeLot(Lot *from, ::Lot &to)
{
    if (isReadOnly(this))
        return false;
    if (this != from->doc)
        return false;
    d->changeLot(from->wrapped, to);
    return true;
}

int Document::count() const
{
    return d->rowCount();
}

Lot Document::lot(int index)
{
    if (index < 0 || index >= d->rowCount())
        return Lot { };
    return Lot(d->lots().at(index), this);
}

void Document::deleteLot(Lot ii)
{
    if (isReadOnly(this))
        return;

    if (!ii.isNull() && ii.doc == this)
        d->removeLot(static_cast<::Lot *>(ii.wrapped));
}

Lot Document::addLot(Item item, Color color)
{
    if (isReadOnly(this))
        return Lot { };

    auto di = new ::Lot();
    di->setItem(item.wrappedObject());
    di->setColor(color.wrappedObject());
    d->appendLot(di);
    return Lot(di, this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


BrickStore::BrickStore()
{
    setObjectName(QLatin1String("BrickStore"));

    auto checkActiveWindow = [this](Window *win) {
        Document *doc = documentForWindow(win);
        if (doc != m_currentDocument) {
            m_currentDocument = doc;
            emit currentDocumentChanged(doc);
        }
    };

    connect(FrameWork::inst(), &FrameWork::windowActivated,
            this, checkActiveWindow);

    connect(FrameWork::inst(), &FrameWork::windowListChanged,
            this, [this, checkActiveWindow]() {
        QVector<Document *> newDocs;
        QVector<Document *> oldDocs = m_documents;
        const auto allWindows = FrameWork::inst()->allWindows();
        for (auto win : allWindows) {
            auto doc = documentForWindow(win);
            if (doc) {
                oldDocs.removeOne(doc);
                newDocs.append(doc);
            } else {
                doc = new Document(win->document());
                QQmlEngine::setObjectOwnership(doc, QQmlEngine::CppOwnership);
                newDocs.append(doc);
            }
        }
        while (!oldDocs.isEmpty()) {
            delete oldDocs.takeLast();
        }
        if (newDocs != m_documents) {
            m_documents = newDocs;
            emit documentsChanged(m_documents);
        }

        // the windowActivated signal for new documents is sent way too early
        // before the windowListChanged signal. Check if the current still is valid.
        checkActiveWindow(FrameWork::inst()->activeWindow());
    });

    connect(Config::inst(), &Config::defaultCurrencyCodeChanged,
            this, &BrickStore::defaultCurrencyCodeChanged);
}

QVector<Document *> BrickStore::documents() const
{
    return m_documents;
}

Document *BrickStore::currentDocument() const
{
    return m_currentDocument;
}

Document *BrickStore::newDocument(const QString &title)
{
    if (isReadOnly(this))
        return nullptr;

    return setupDocument(nullptr, ::DocumentIO::create(), title);
}

Document *BrickStore::openDocument(const QString &fileName)
{
    if (isReadOnly(this))
        return nullptr;

    return setupDocument(::DocumentIO::open(fileName), nullptr);
}

Document *BrickStore::importBrickLinkStore(const QString &title)
{
    if (isReadOnly(this))
        return nullptr;

    return setupDocument(nullptr, ::DocumentIO::importBrickLinkStore(), title);
}

QString BrickStore::defaultCurrencyCode() const
{
     return Config::inst()->defaultCurrencyCode();
}

QString BrickStore::symbolForCurrencyCode(const QString &currencyCode) const
{
    static QHash<QString, QString> cache;
    QString s = cache.value(currencyCode);
    if (s.isEmpty()) {
         const auto allLoc = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                      QLocale::AnyCountry);

         for (auto &loc : allLoc) {
             if (loc.currencySymbol(QLocale::CurrencyIsoCode) == currencyCode) {
                 s = loc.currencySymbol(QLocale::CurrencySymbol);
                 break;
             }
         }
         cache.insert(currencyCode, s.isEmpty() ? currencyCode : s);
    }
    return s;
}

QString BrickStore::toCurrencyString(double value, const QString &symbol, int precision) const
{
    return QLocale::system().toCurrencyString(value, symbol, precision);
}

QString BrickStore::toWeightString(double value, bool showUnit) const
{
    return Utility::weightToString(value, Config::inst()->measurementSystem(), true, showUnit);
}

Document *BrickStore::documentForWindow(Window *win) const
{
    if (win) {
        for (auto doc : m_documents) {
            if (doc->isWrapperFor(win->document()))
                return doc;
        }
    }
    return nullptr;
}

Document *BrickStore::setupDocument(::Window *win, ::Document *doc, const QString &title)
{
    if ((!win && !doc) || (win && doc))
        return nullptr;

    if (!win) {
        win = FrameWork::inst()->createWindow(doc);
    } else {
        FrameWork::inst()->setupWindow(win);
        doc = win->document();
    }

    if (!title.isEmpty())
        doc->setTitle(title);

    Q_ASSERT(currentDocument());
    Q_ASSERT(currentDocument()->isWrapperFor(win->document()));

    return currentDocument();
}

} // namespace QmlWrapper

#include "moc_bricklink_wrapper.cpp"


