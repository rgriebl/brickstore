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

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlInfo>
#include <QMessageLogger>
#include <QLoggingCategory>

#include "application.h"
#include "exception.h"
#include "framework.h"
#include "document.h"
#include "window.h"

#include "qml_bricklink_wrapper.h"


class QmlException : public Exception
{
public:
    QmlException(const QList<QQmlError> &errors, const char *msg)
        : Exception(msg)
    {
        for (auto error : errors) {
            m_message.append("\n");
            m_message.append(error.toString());
        }
    }
};


namespace QmlWrapper {


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
    return d->color(id);
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
    return d->category(id);
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

::BrickLink::InvItem &InvItem::Setter::to()
{
    return m_to;
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


Document::Document(::Document *doc, ::DocumentProxyModel *view)
    : d(doc)
    , dpm(view)
{
    connect(doc, &::Document::itemsAdded, this,
            [this]() { emit countChanged(d->rowCount()); });
    connect(doc, &::Document::itemsRemoved, this,
            [this]() { emit countChanged(d->rowCount()); });
}

bool Document::isWrapperFor(::Document *doc, ::DocumentProxyModel *view) const
{
    return (d == doc) && (dpm == view);
}

bool Document::changeItem(InvItem *from, ::BrickLink::InvItem &to)
{
    if (this != from->doc)
        return false;
    return d->changeItem(from->wrapped, to);
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
    if (!ii.isNull() && ii.doc == this) {
        d->removeItem(static_cast<::Document::Item *>(ii.wrapped));
    }
}

InvItem Document::addInvItem(Item item, Color color)
{
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
                doc = new Document(win->document(), win->documentView());
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
    return setupDocument(::Document::fileNew(), title);
}

Document *BrickStore::openDocument(const QString &fileName)
{
    return setupDocument(::Document::fileOpen(fileName));
}

Document *BrickStore::importBrickLinkStore(const QString &title)
{
    return setupDocument(::Document::fileImportBrickLinkStore(), title);
}

void BrickStore::classBegin()
{ }

void BrickStore::componentComplete()
{ }

Document *BrickStore::documentForWindow(Window *win) const
{
    if (win) {
        for (auto doc : m_documents) {
            if (doc->isWrapperFor(win->document(), win->documentView()))
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
        Q_ASSERT(currentDocument()->isWrapperFor(doc, win->documentView()));

        return currentDocument();
    }
    return nullptr;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QString ScriptMenuAction::text() const
{
    return m_text;
}

void ScriptMenuAction::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    emit textChanged(text);

    if (m_action)
        m_action->setText(text);
}

ScriptMenuAction::Location ScriptMenuAction::location() const
{
    return m_type;
}

void ScriptMenuAction::setLocation(ScriptMenuAction::Location type)
{
    if (type == m_type)
        return;
    m_type = type;
    emit locationChanged(type);
}

void ScriptMenuAction::classBegin()
{ }

void ScriptMenuAction::componentComplete()
{
    if (auto *script = qobject_cast<Script *>(parent())) {
        script->m_menuEntries << this;
    } else {
        qmlWarning(this) << "ScriptMenuAction objects need to be nested inside Script objects";
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QString Script::name() const
{
    return m_name;
}

void Script::setName(QString name)

{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

QString Script::author() const
{
    return m_author;
}

void Script::setAuthor(QString author)
{
    if (m_author != author) {
        m_author = author;
        emit authorChanged(m_author);
    }
}

QString Script::version() const
{
    return m_version;
}

void Script::setVersion(QString version)
{
    if (m_version != version) {
        m_version = version;
        emit versionChanged(m_version);
    }
}

Script::Type Script::type() const
{
    return m_type;
}

void Script::setType(Type type)
{
    if (m_type != type) {
        m_type = type;
        emit typeChanged(m_type);
    }
}

Script *Script::load(QQmlEngine *engine, const QString &filename)

{
    QScopedPointer<QQmlComponent> comp(new QQmlComponent(engine, filename));
    QScopedPointer<QQmlContext> ctx(new QQmlContext(engine));
    QScopedPointer<QObject> root(comp->create(ctx.data()));
    if (!root)
        throw QmlException(comp->errors(), "Could not load QML file %1").arg(filename);
    if (root && !qobject_cast<Script *>(root.data()))
        throw Exception("The root element of the script %1 is not 'Script'").arg(filename);

    auto script = static_cast<Script *>(root.take());
    script->m_component = comp.take();
    script->m_context = ctx.take();
    script->m_fileName = filename;

    return script;
}

QList<ScriptMenuAction *> Script::menuEntries() const
{
    return m_menuEntries;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ScriptManager::ScriptManager()
    : m_engine(new QQmlEngine(this))
{
    connect(m_engine, &QQmlEngine::warnings,
            this, [this](const QList<QQmlError> &list) {
        static QLoggingCategory logQml("qml");

        if (!logQml.isWarningEnabled())
            return;

        for (auto err : list) {
            QByteArray func;
            if (err.object())
                func = err.object()->objectName().toLocal8Bit();
            QByteArray file;
            if (err.url().scheme() == "file")
                file = err.url().toLocalFile().toLocal8Bit();
            else
                file = err.url().toDisplayString().toLocal8Bit();

            QMessageLogger ml(file, err.line(), func, logQml.categoryName());
            ml.warning().nospace().noquote() << err.description();
        }
    });
}

ScriptManager::~ScriptManager()
{
    qDeleteAll(m_scripts);
    s_inst = nullptr;
}

ScriptManager *ScriptManager::s_inst = nullptr;

ScriptManager *ScriptManager::inst()
{
    if (!s_inst) {
        s_inst = new ScriptManager();

    }
    return s_inst;
}

bool ScriptManager::initialize(::BrickLink::Core *core)
{
    if (m_bricklink || !core)
        return false;

    m_bricklink = new BrickLink(core);
    m_brickstore = new BrickStore();

    QString cannotCreate = tr("Cannot create objects of type %1");

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterSingletonInstance("BrickStore", 1, 0, "BrickLink", m_bricklink);
    qmlRegisterSingletonInstance("BrickStore", 1, 0, "BrickStore", m_brickstore);
#else
    static auto staticBrickLink = m_bricklink;
    QQmlEngine::setObjectOwnership(staticBrickLink, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<QmlWrapper::BrickLink>("BrickStore", 1, 0, "BrickLink",
                             [](QQmlEngine *, QJSEngine *) -> QObject * {
        return staticBrickLink;
    });
    static auto staticBrickStore = m_brickstore;
    QQmlEngine::setObjectOwnership(staticBrickStore, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<QmlWrapper::BrickStore>("BrickStore", 1, 0, "BrickStore",
                             [](QQmlEngine *, QJSEngine *) -> QObject * {
        return staticBrickStore;
    });
#endif

    qRegisterMetaType<QmlWrapper::Color>("Color");
    qRegisterMetaType<QmlWrapper::ItemType>("ItemType");
    qRegisterMetaType<QmlWrapper::Category>("Category");
    qRegisterMetaType<QmlWrapper::Item>("Item");
    qRegisterMetaType<QmlWrapper::InvItem>("InvItem");
    qRegisterMetaType<QmlWrapper::PriceGuide>("PriceGuide");
    qRegisterMetaType<QmlWrapper::Picture>("Picture");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::Time");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::Price");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::Condition");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::SubCondition");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::Stockroom");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::Status");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::UpdateStatus");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::OrderType");

    qmlRegisterUncreatableType<QmlWrapper::Document>("BrickStore", 1, 0, "Document",
                                                     cannotCreate.arg("Document"));

    qmlRegisterType<QmlWrapper::Script>("BrickStore", 1, 0, "Script");
    qmlRegisterType<QmlWrapper::ScriptMenuAction>("BrickStore", 1, 0, "ScriptMenuAction");

    reload();
    return true;
}

bool ScriptManager::reload()
{
    qDeleteAll(m_scripts);
    m_scripts.clear();

    QStringList spath = { QStringLiteral(":/scripts") };

    spath << Application::inst()->externalResourceSearchPath("scripts");

    QString dataloc = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!dataloc.isEmpty())
        spath.prepend(dataloc + QLatin1String("/scripts"));

    for (const QString &dir : qAsConst(spath)) {
        qDebug() << "Loading scripts in directory" << dir;

        const QFileInfoList fis = QDir(dir).entryInfoList(QStringList("*.bs.qml"), QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : fis) {
            try {

                Script *script = Script::load(m_engine, fi.absoluteFilePath());
                if (script)
                    m_scripts.append(script);

                qDebug() << "Loaded script" << fi.absoluteFilePath();

            } catch (const Exception &e) {
                qWarning() << "Could not load script:\n" << e.what();
            }
        }
    }
    return !m_scripts.isEmpty();
}

QList<Script *> ScriptManager::scripts() const
{
    return m_scripts;
}

} // namespace QmlWrapper

#include "moc_qml_bricklink_wrapper.cpp"


