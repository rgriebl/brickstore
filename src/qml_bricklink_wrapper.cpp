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

#include "application.h"
#include "exception.h"
#include "framework.h"
#include "document.h"
#include "window.h"

#include "qml_bricklink_wrapper.h"


namespace QmlWrapper {


BrickLink::BrickLink(::BrickLink::Core *core)
    : d(core)
{ }

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
    return PriceGuide { d->priceGuide(item.d, color.d, highPriority) };
}

Picture BrickLink::picture(Item item, Color color, bool highPriority)
{
    return Picture { d->picture(item.d, color.d, highPriority) };
}

Picture BrickLink::largePicture(Item item, bool highPriority)
{
    return Picture { d->largePicture(item.d, highPriority) };
}

char BrickLink::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}


QVariantList Item::consistsOf() const
{
    QVariantList result;
    const auto invItems = d->consistsOf();
    for (auto invItem : invItems)
        result << QVariant::fromValue(InvItem { invItem, nullptr });
    return result;
}

QVector<Color> Item::knownColors() const
{
    auto known = d->knownColors();
    QVector<Color> result;
    for (auto c : known)
        result.append(Color { c });
    return result;
}

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

    if (m_action)
        attachAction();
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

void ScriptMenuAction::attachAction()
{
    //TODO: remove from current menu and attach to m_type
}

QVector<Category> ItemType::categories() const
{
    auto cats = d->categories();
    QVector<Category> result;
    for (auto cat : cats)
        result.append(Category { cat });
    return result;
}



ScriptManager::ScriptManager()
    : m_engine(new QQmlEngine(this))
{ }

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

    qmlRegisterSingletonInstance("BrickStore", 1, 0, "BrickLink", m_bricklink);
    qmlRegisterSingletonInstance("BrickStore", 1, 0, "BrickStore", m_brickstore);

    qRegisterMetaType<Color>();
    qRegisterMetaType<ItemType>();
    qRegisterMetaType<Category>();
    qRegisterMetaType<Item>();
    qRegisterMetaType<InvItem>();
    qRegisterMetaType<PriceGuide>();
    qRegisterMetaType<Picture>();

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

Document *BrickStore::setupDocument(::Document *doc, const QString &title = { })
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

QVector<Document *> BrickStore::documents() const
{
    return m_documents;
}

Document *BrickStore::currentDocument() const
{
    return m_currentDocument;
}

void BrickStore::classBegin()
{ }

void BrickStore::componentComplete()
{ }

Document::Document(::Document *doc, ::DocumentProxyModel *view)
    : d(doc)
    , dpm(view)
{
    connect(doc, &::Document::itemsAdded, this, [this]() { emit countChanged(d->rowCount()); });
    connect(doc, &::Document::itemsRemoved, this, [this]() { emit countChanged(d->rowCount()); });
}

bool Document::isWrapperFor(::Document *doc, ::DocumentProxyModel *view) const
{
    return (d == doc) && (dpm == view);
}

bool Document::changeItem(InvItem *from, ::BrickLink::InvItem &to)
{
    if (this != from->doc)
        return false;
    return d->changeItem(from->d, to);
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
        d->removeItem(static_cast<::Document::Item *>(ii.d));
    }
}

InvItem Document::addInvItem(Item item, Color color)
{
    auto di = new ::Document::Item();
    di->setItem(item.d_ptr());
    di->setColor(color.d_ptr());
    d->appendItem(di);
    return InvItem(di, this);
}

void InvItem::setItem(Item item)
{
    if (d->item() != item.d_ptr())
        setter().to().setItem(item.d_ptr());
}

void InvItem::setColor(Color color)
{
    if (d->color() != color.d_ptr())
        setter().to().setColor(color.d_ptr());
}

void InvItem::setQuantity(int q)
{
    if (d->quantity() != q)
        setter().to().setQuantity(q);
}

InvItem::Setter InvItem::setter()
{
    return Setter(this);
}

InvItem::Setter::Setter(InvItem *invItem)
    : m_invItem(invItem)
{
    m_to = *m_invItem->d;
}

::BrickLink::InvItem &InvItem::Setter::to()
{
    return m_to;
}

InvItem::Setter::~Setter()
{
    m_invItem->doc->changeItem(m_invItem, m_to);
}

} // namespace QmlWrapper

#include "moc_qml_bricklink_wrapper.cpp"


