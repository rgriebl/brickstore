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
    return d->item(firstCharInString(itemTypeId), itemId);
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Item::Item(const ::BrickLink::Item *item)
    : WrapperBase(item)
{ }

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
    const auto invItems = wrapped->consistsOf();
    QVariantList result;
    for (auto invItem : invItems)
        result << QVariant::fromValue(InvItem { invItem, nullptr });
    return result;
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


InvItem::InvItem(::BrickLink::InvItem *invItem, Document *document)
    : WrapperBase(invItem)
    , doc(document)
{ }

//Item InvItem::item() const
//{
//    return wrapped->item();
//}

//void InvItem::setItem(Item item)
//{
//    set().to().setItem(item.wrappedObject());
//}

//Color InvItem::color() const
//{
//    return wrapped->color();
//}

//void InvItem::setColor(Color color)
//{
//    set().to().setColor(color.wrappedObject());
//}

//int InvItem::quantity() const
//{
//    return wrapped->quantity();
//}

//void InvItem::setQuantity(int q)
//{
//    set().to().setQuantity(q);
//}

InvItem::Setter::Setter(InvItem *invItem)
    : m_invItem((invItem && !invItem->isNull()) ? invItem : nullptr)
{
    if (m_invItem)
        m_to = *m_invItem->wrapped;
}

::BrickLink::InvItem *InvItem::Setter::to()
{
    return &m_to;
}

InvItem::Setter::~Setter()
{
    if (m_invItem && (*m_invItem->wrapped != m_to))
        m_invItem->doc->changeItem(m_invItem, m_to);
}

InvItem::Setter InvItem::set()
{
    return Setter(this);
}

::BrickLink::InvItem *InvItem::get() const
{
    return wrapped;
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

bool Document::changeItem(InvItem *from, ::BrickLink::InvItem &to)
{
    if (isReadOnly(this))
        return false;
    if (this != from->doc)
        return false;
    d->changeItem(from->wrapped, to);
    return true;
}

int Document::count() const
{
    return d->rowCount();
}

InvItem Document::invItem(int index)
{
    if (index < 0 || index >= d->rowCount())
        return InvItem { };
    return InvItem(d->items().at(index), this);
}

void Document::deleteInvItem(InvItem ii)
{
    if (isReadOnly(this))
        return;

    if (!ii.isNull() && ii.doc == this)
        d->removeItem(static_cast<::Document::Item *>(ii.wrapped));
}

InvItem Document::addInvItem(Item item, Color color)
{
    if (isReadOnly(this))
        return InvItem { };

    auto di = new ::Document::Item();
    di->setItem(item.wrappedObject());
    di->setColor(color.wrappedObject());
    d->appendItem(di);
    return InvItem(di, this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


BrickStore::BrickStore()
{
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

    return setupDocument(::DocumentIO::create(), title);
}

Document *BrickStore::openDocument(const QString &fileName)
{
    if (isReadOnly(this))
        return nullptr;

    return setupDocument(::DocumentIO::open(fileName));
}

Document *BrickStore::importBrickLinkStore(const QString &title)
{
    if (isReadOnly(this))
        return nullptr;

    return setupDocument(::DocumentIO::importBrickLinkStore(), title);
}

void BrickStore::classBegin()
{ }

void BrickStore::componentComplete()
{ }

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

Document *BrickStore::setupDocument(::Document *doc, const QString &title)
{
    if (doc) {
        auto win = FrameWork::inst()->createWindow(doc);
        if (!title.isEmpty())
            doc->setTitle(title);

        Q_ASSERT(currentDocument());
        Q_ASSERT(currentDocument()->isWrapperFor(win->document()));

        return currentDocument();
    }
    return nullptr;
}

} // namespace QmlWrapper

#include "moc_bricklink_wrapper.cpp"


