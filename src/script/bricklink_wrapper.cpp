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
#include "qqmlinfo.h"

#include "framework.h"
#include "view.h"
#include "documentio.h"
#include "utility/utility.h"
#include "utility/currency.h"

#include "bricklink_wrapper.h"


QmlBrickLink::QmlBrickLink(BrickLink::Core *core)
    : d(core)
{
    setObjectName("BrickLink"_l1);

    connect(core, &::BrickLink::Core::priceGuideUpdated,
            this, [this](::BrickLink::PriceGuide *pg) {
        emit priceGuideUpdated(QmlPriceGuide(pg));
    });
    connect(core, &::BrickLink::Core::pictureUpdated,
            this, [this](::BrickLink::Picture *pic) {
        emit pictureUpdated(QmlPicture(pic));
    });
}

QmlItem QmlBrickLink::noItem() const
{
    return QmlItem { };
}

QmlColor QmlBrickLink::noColor() const
{
    return QmlColor { };
}

QmlColor QmlBrickLink::color(int id) const
{
    return d->color(uint(id));
}

QmlColor QmlBrickLink::colorFromName(const QString &name) const
{
    return d->colorFromName(name);
}

QmlColor QmlBrickLink::colorFromLDrawId(int ldrawId) const
{
    return d->colorFromLDrawId(ldrawId);
}

QmlCategory QmlBrickLink::category(int id) const
{
    return d->category(uint(id));
}

QmlItemType QmlBrickLink::itemType(const QString &itemTypeId) const
{
    return d->itemType(firstCharInString(itemTypeId));
}

QmlItem QmlBrickLink::item(const QString &itemTypeId, const QString &itemId) const
{
    return d->item(firstCharInString(itemTypeId), itemId.toLatin1());
}

QmlPriceGuide QmlBrickLink::priceGuide(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPriceGuide { d->priceGuide(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

QmlPicture QmlBrickLink::picture(QmlItem item, QmlColor color, bool highPriority)
{
    return QmlPicture { d->picture(item.wrappedObject(), color.wrappedObject(), highPriority) };
}

QmlPicture QmlBrickLink::largePicture(QmlItem item, bool highPriority)
{
    return QmlPicture { d->largePicture(item.wrappedObject(), highPriority) };
}

void QmlBrickLink::cacheStat() const
{
    auto pic = BrickLink::core()->pictureCacheStats();
    auto pg = BrickLink::core()->priceGuideCacheStats();

    QByteArray picBar(int(double(pic.first) / pic.second * 16), '=');
    picBar.append(16 - picBar.length(), ' ');
    QByteArray pgBar(int(double(pg.first) / pg.second * 16), '=');
    pgBar.append(16 - pgBar.length(), ' ');

    qmlDebug(this) << "Cache stats:\n"
                   << "Pictures    : [" << picBar.constData() << "] " << (pic.first / 1000)
                   << " / " << (pic.second / 1000) << " MB\n"
                   << "Price guides: [" << pgBar.constData() << "] " << (pg.first)
                   << " / " << pg.second << " entries";
}

char QmlBrickLink::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlCategory::QmlCategory(const BrickLink::Category *cat)
    : QmlWrapperBase(cat)
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlItemType::QmlItemType(const BrickLink::ItemType *itt)
    : QmlWrapperBase(itt)
{ }

QVariantList QmlItemType::categories() const
{
    auto cats = wrapped->categories();
    QVariantList result;
    for (auto cat : cats)
        result.append(QVariant::fromValue(QmlCategory { cat }));
    return result;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlColor::QmlColor(const BrickLink::Color *col)
    : QmlWrapperBase(col)
{ }

QImage QmlColor::image() const
{
    return ::BrickLink::core()->colorImage(wrapped, 20, 20);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlItem::QmlItem(const BrickLink::Item *item)
    : QmlWrapperBase(item)
{ }

QString QmlItem::id() const
{
    return QString::fromLatin1(wrapped->id());
}

bool QmlItem::hasKnownColor(QmlColor color) const
{
    return wrapped->hasKnownColor(color.wrappedObject());
}

QVariantList QmlItem::knownColors() const
{
    auto known = wrapped->knownColors();
    QVariantList result;
    for (auto c : known)
        result.append(QVariant::fromValue(QmlColor { c }));
    return result;
}

QVariantList QmlItem::consistsOf() const
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


QmlPicture::QmlPicture(BrickLink::Picture *pic)
    : QmlWrapperBase(pic)
{
    if (!isNull())
        wrappedObject()->addRef();
}

QmlPicture::QmlPicture(const QmlPicture &copy)
    : QmlPicture(copy.wrappedObject())
{ }

QmlPicture &QmlPicture::operator=(const QmlPicture &assign)
{
    this->~QmlPicture();
    QmlWrapperBase::operator=(assign);
    return *new (this) QmlPicture(assign.wrappedObject());
}

QmlPicture::~QmlPicture()
{
    if (!isNull())
        wrappedObject()->release();
}

QmlBrickLink::UpdateStatus QmlPicture::updateStatus() const
{
    return static_cast<QmlBrickLink::UpdateStatus>(wrapped->updateStatus());
}

void QmlPicture::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlPriceGuide::QmlPriceGuide(BrickLink::PriceGuide *pg)
    : QmlWrapperBase(pg)
{
    if (!isNull())
        wrappedObject()->addRef();
}

QmlPriceGuide::QmlPriceGuide(const QmlPriceGuide &copy)
    : QmlPriceGuide(copy.wrappedObject())
{ }

QmlPriceGuide &QmlPriceGuide::operator=(const QmlPriceGuide &assign)
{
    this->~QmlPriceGuide();
    QmlWrapperBase::operator=(assign);
    return *new (this) QmlPriceGuide(assign.wrappedObject());
}

QmlPriceGuide::~QmlPriceGuide()
{
    if (!isNull())
        wrappedObject()->release();
}

QmlBrickLink::UpdateStatus QmlPriceGuide::updateStatus() const
{
    return static_cast<QmlBrickLink::UpdateStatus>(wrapped->updateStatus());
}

void QmlPriceGuide::update(bool highPriority)
{
    if (!isNull())
        wrappedObject()->update(highPriority);
}

int QmlPriceGuide::quantity(QmlBrickLink::Time time, QmlBrickLink::Condition condition) const
{
    return wrapped->quantity(static_cast<BrickLink::Time>(time),
                             static_cast<BrickLink::Condition>(condition));
}

int QmlPriceGuide::lots(QmlBrickLink::Time time, QmlBrickLink::Condition condition) const
{
    return wrapped->lots(static_cast<BrickLink::Time>(time),
                         static_cast<BrickLink::Condition>(condition));
}

double QmlPriceGuide::price(QmlBrickLink::Time time, QmlBrickLink::Condition condition,
                            QmlBrickLink::Price price) const
{
    return wrapped->price(static_cast<BrickLink::Time>(time),
                          static_cast<BrickLink::Condition>(condition),
                          static_cast<BrickLink::Price>(price));
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlLot::QmlLot(Lot *lot, QmlDocument *document)
    : QmlWrapperBase(lot)
    , doc(document)
{ }

QImage QmlLot::image() const
{
    static QImage dummy;
    auto pic = ::BrickLink::core()->picture(get()->item(), get()->color(), true);
    return pic ? pic->image() : dummy;
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

QmlLot::Setter::Setter(QmlLot *lot)
    : m_lot((lot && !lot->isNull()) ? lot : nullptr)
{
    if (m_lot)
        m_to = *m_lot->wrapped;
}

::Lot *QmlLot::Setter::to()
{
    return &m_to;
}

QmlLot::Setter::~Setter()
{
    if (m_lot && (*m_lot->wrapped != m_to))
        m_lot->doc->changeLot(m_lot, m_to);
}

QmlLot::Setter QmlLot::set()
{
    return Setter(this);
}

::Lot *QmlLot::get() const
{
    return wrapped;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlOrder::QmlOrder(const BrickLink::Order *order)
    : QmlWrapperBase(order)
{ }

QmlBrickLink::OrderType QmlOrder::type() const
{
    return static_cast<QmlBrickLink::OrderType>(wrapped->type());
}

QmlBrickLink::OrderStatus QmlOrder::status() const
{
    return static_cast<QmlBrickLink::OrderStatus>(wrapped->status());
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDocument::QmlDocument(View *_win)
    : d(_win->document())
    , win(_win)
{
    connect(d, &::Document::fileNameChanged,
            this, &QmlDocument::fileNameChanged);
    connect(d, &::Document::titleChanged,
            this, &QmlDocument::titleChanged);
    connect(d, &::Document::currencyCodeChanged,
            this, &QmlDocument::currencyCodeChanged);
//    connect(d, &::Document::filterChanged,
//            this, &QmlDocument::filterChanged);

    connect(d, &::Document::lotCountChanged,
            this, &QmlDocument::lotCountChanged);
}

bool QmlDocument::isWrapperFor(View *win) const
{
    return (this->win == win);
}

bool QmlDocument::changeLot(QmlLot *from, ::Lot &to)
{
    if (this != from->doc)
        return false;
    d->changeLot(from->wrapped, to);
    return true;
}

QmlLot QmlDocument::lot(int index)
{
    if (index < 0 || index >= d->rowCount())
        return QmlLot { };
    return QmlLot(d->lots().at(index), this);
}

void QmlDocument::deleteLot(QmlLot ii)
{
    if (!ii.isNull() && ii.doc == this)
        d->removeLot(static_cast<::Lot *>(ii.wrapped));
}

QmlLot QmlDocument::addLot(QmlItem item, QmlColor color)
{
    auto di = new ::Lot();
    di->setItem(item.wrappedObject());
    di->setColor(color.wrappedObject());
    d->appendLot(di);
    return QmlLot(di, this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlBrickStore::QmlBrickStore()
{
    setObjectName("BrickStore"_l1);

    auto checkActiveWindow = [this](View *win) {
        QmlDocument *doc = documentForWindow(win);
        if (doc != m_currentDocument) {
            m_currentDocument = doc;
            emit currentDocumentChanged(doc);
        }
    };

    connect(FrameWork::inst(), &FrameWork::viewActivated,
            this, checkActiveWindow);

    connect(FrameWork::inst(), &FrameWork::viewListChanged,
            this, [this, checkActiveWindow]() {
        QVector<QmlDocument *> newDocs;
        QVector<QmlDocument *> oldDocs = m_documents;
        const auto allWindows = FrameWork::inst()->allViews();
        for (auto win : allWindows) {
            auto doc = documentForWindow(win);
            if (doc) {
                oldDocs.removeOne(doc);
                newDocs.append(doc);
            } else {
                doc = new QmlDocument(win);
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
        checkActiveWindow(FrameWork::inst()->activeView());
    });

    connect(Config::inst(), &Config::defaultCurrencyCodeChanged,
            this, &QmlBrickStore::defaultCurrencyCodeChanged);
}

QVector<QmlDocument *> QmlBrickStore::documents() const
{
    return m_documents;
}

QmlDocument *QmlBrickStore::currentDocument() const
{
    return m_currentDocument;
}

QmlDocument *QmlBrickStore::newDocument(const QString &title)
{
    return setupDocument(nullptr, ::DocumentIO::create(), title);
}

QmlDocument *QmlBrickStore::openDocument(const QString &fileName)
{
    return setupDocument(::DocumentIO::open(fileName), nullptr);
}

QmlDocument *QmlBrickStore::importBrickLinkStore(const QString &title)
{
    return setupDocument(nullptr, ::DocumentIO::importBrickLinkStore(), title);
}

QString QmlBrickStore::defaultCurrencyCode() const
{
     return Config::inst()->defaultCurrencyCode();
}

QString QmlBrickStore::symbolForCurrencyCode(const QString &currencyCode) const
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

QString QmlBrickStore::toCurrencyString(double value, const QString &symbol, int precision) const
{
    return Currency::toString(value, symbol, precision);
}

QString QmlBrickStore::toWeightString(double value, bool showUnit) const
{
    return Utility::weightToString(value, Config::inst()->measurementSystem(), true, showUnit);
}

QmlDocument *QmlBrickStore::documentForWindow(View *win) const
{
    if (win) {
        for (auto doc : m_documents) {
            if (doc->isWrapperFor(win))
                return doc;
        }
    }
    return nullptr;
}

QmlDocument *QmlBrickStore::setupDocument(::View *win, ::Document *doc, const QString &title)
{
    if ((!win && !doc) || (win && doc))
        return nullptr;

    if (!win) {
        win = FrameWork::inst()->createView(doc);
    } else {
        FrameWork::inst()->setupView(win);
        doc = win->document();
    }

    if (!title.isEmpty())
        doc->setTitle(title);

    Q_ASSERT(currentDocument());
    Q_ASSERT(currentDocument()->isWrapperFor(win));

    return currentDocument();
}

#include "moc_bricklink_wrapper.cpp"


