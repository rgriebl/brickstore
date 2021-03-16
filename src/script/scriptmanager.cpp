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
#include <QMessageLogger>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlInfo>
#include <QStringBuilder>

#include "scriptmanager.h"
#include "script.h"
#include "printjob.h"
#include "bricklink_wrapper.h"
#include "exception.h"
#include "application.h"


class QmlException : public Exception
{
public:
    QmlException(const QList<QQmlError> &errors, const char *msg)
        : Exception(msg)
    {
        for (auto &error : errors) {
            m_message.append("\n"_l1);
            m_message.append(error.toString());
        }
    }
};


ScriptManager::ScriptManager()
{ }

ScriptManager::~ScriptManager()
{
    clearScripts();
    s_inst = nullptr;

    delete m_brickLink;
    delete m_brickStore;
}

ScriptManager *ScriptManager::s_inst = nullptr;

ScriptManager *ScriptManager::inst()
{
    if (!s_inst)
        s_inst = new ScriptManager();
    return s_inst;
}

bool ScriptManager::initialize(::BrickLink::Core *core)
{
    if (m_brickLink || !core)
        return false;

    m_brickLink = new QmlWrapper::BrickLink(core);
    m_brickStore = new QmlWrapper::BrickStore();

    static auto *that = this; // workaround for qmlRegisterSingleton in Qt < 5.14

    QString cannotCreate = tr("Cannot create objects of type %1");

    QQmlEngine::setObjectOwnership(m_brickLink, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<QmlWrapper::BrickLink>("BrickStore", 1, 0, "BrickLink",
                             [](QQmlEngine *, QJSEngine *) -> QObject * {
        return that->m_brickLink;
    });
    QQmlEngine::setObjectOwnership(m_brickStore, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<QmlWrapper::BrickStore>("BrickStore", 1, 0, "BrickStore",
                             [](QQmlEngine *, QJSEngine *) -> QObject * {
        return that->m_brickStore;
    });

    qRegisterMetaType<QmlWrapper::Color>("Color");
    qRegisterMetaType<QmlWrapper::ItemType>("ItemType");
    qRegisterMetaType<QmlWrapper::Category>("Category");
    qRegisterMetaType<QmlWrapper::Item>("Item");
    qRegisterMetaType<QmlWrapper::Lot>("Lot");
    qRegisterMetaType<QmlWrapper::PriceGuide>("PriceGuide");
    qRegisterMetaType<QmlWrapper::Picture>("Picture");
    qRegisterMetaType<QmlWrapper::Order>("Order");
    qRegisterMetaType<QmlWrapper::BrickLink::Time>("BrickLink::Time");
    qRegisterMetaType<QmlWrapper::BrickLink::Price>("BrickLink::Price");
    qRegisterMetaType<QmlWrapper::BrickLink::Condition>("BrickLink::Condition");
    qRegisterMetaType<QmlWrapper::BrickLink::SubCondition>("BrickLink::SubCondition");
    qRegisterMetaType<QmlWrapper::BrickLink::Stockroom>("BrickLink::Stockroom");
    qRegisterMetaType<QmlWrapper::BrickLink::Status>("BrickLink::Status");
    qRegisterMetaType<QmlWrapper::BrickLink::UpdateStatus>("BrickLink::UpdateStatus");
    qRegisterMetaType<QmlWrapper::BrickLink::OrderType>("BrickLink::OrderType");

    qmlRegisterUncreatableType<QmlWrapper::Document>("BrickStore", 1, 0, "Document",
                                                     cannotCreate.arg("Document"_l1));

    qmlRegisterUncreatableType<QmlWrapper::PrintJob>("BrickStore", 1, 0, "PrintJob",
                                                     cannotCreate.arg("PrintJob"_l1));
    qmlRegisterUncreatableType<QmlWrapper::PrintPage>("BrickStore", 1, 0, "Page",
                                                      cannotCreate.arg("Page"_l1));
    qRegisterMetaType<QmlWrapper::PrintPage::Alignment>("PrintPage::Alignment");
    qRegisterMetaType<QmlWrapper::PrintPage::LineStyle>("PrintPage::LineStyle");

    qmlRegisterType<Script>("BrickStore", 1, 0, "Script");
    qmlRegisterType<ExtensionScriptAction>("BrickStore", 1, 0, "ExtensionScriptAction");
    qmlRegisterType<PrintingScriptTemplate>("BrickStore", 1, 0, "PrintingScriptTemplate");

    reload();
    return true;
}

bool ScriptManager::reload()
{
    emit aboutToReload();

    clearScripts();

    QStringList spath = { QStringLiteral(":/extensions") };

    spath << Application::inst()->externalResourceSearchPath("extensions"_l1);

    QString dataloc = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!dataloc.isEmpty())
        spath.prepend(dataloc + "/extensions"_l1);

    for (const QString &dir : qAsConst(spath)) {
        qDebug() << "Loading scripts from directory:" << dir;

        const QFileInfoList fis = QDir(dir).entryInfoList(QStringList("*.bs.qml"_l1), QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : fis) {
            QString filePath = fi.absoluteFilePath();
            if (filePath.startsWith(u":"))
                filePath = u"qrc://" % filePath.mid(1);

            try {
                loadScript(filePath);

                qDebug() << "  [ ok ]" << filePath;

            } catch (const Exception &e) {
                qWarning() << "  [fail]" << filePath << ":" << e.what();
            }
        }
    }
    emit reloaded();

    return !m_scripts.isEmpty();
}

void ScriptManager::loadScript(const QString &fileName)
{
    QScopedPointer<QQmlEngine> engine(new QQmlEngine(this));
    auto comp = new QQmlComponent(engine.data(), fileName, engine.data());
    auto ctx = new QQmlContext(engine.data(), engine.data());
    QScopedPointer<QObject> root(comp->create(ctx));
    if (!root)
        throw QmlException(comp->errors(), "Could not load QML file %1").arg(fileName);
    if (root && !qobject_cast<Script *>(root.data()))
        throw Exception("The root element of the script %1 is not 'Script'").arg(fileName);

    auto script = static_cast<Script *>(root.take());
    engine->rootContext()->setProperty("readOnlyContext",
                                       script->type() == Script::Type::PrintingScript);
    script->m_engine.reset(engine.take());
    script->m_fileName = fileName;
    script->m_context = ctx;
    script->m_component = comp;
    script->m_brickStore = m_brickStore;

    redirectQmlEngineWarnings(script->m_engine.data());

    m_scripts.append(script);
}

void ScriptManager::redirectQmlEngineWarnings(QQmlEngine *engine)
{
    engine->setOutputWarningsToStandardError(false);

    connect(engine, &QQmlEngine::warnings,
            this, [](const QList<QQmlError> &list) {
        static QLoggingCategory logQml("qml-engine");

        if (!logQml.isWarningEnabled())
            return;

        for (auto &err : list) {
            QByteArray func;
            if (err.object())
                func = err.object()->objectName().toLocal8Bit();
            QByteArray file;
            if (err.url().scheme() == "file"_l1)
                file = err.url().toLocalFile().toLocal8Bit();
            else
                file = err.url().toDisplayString().toLocal8Bit();

            QMessageLogger ml(file, err.line(), func, logQml.categoryName());
            ml.warning().nospace().noquote() << err.description();
        }
    });
}

QVector<Script *> ScriptManager::scripts() const
{
    return m_scripts;
}

QVector<Script *> ScriptManager::extensionScripts() const
{
    QVector<Script *> result;
    for (auto s : m_scripts) {
        if (s->type() == Script::Type::ExtensionScript)
            result << s;
    }
    return result;
}

QVector<Script *> ScriptManager::printingScripts() const
{
    QVector<Script *> result;
    for (auto s : m_scripts) {
        if (s->type() == Script::Type::PrintingScript)
            result << s;
    }
    return result;
}

void ScriptManager::clearScripts()
{
    qDeleteAll(m_scripts);
    m_scripts.clear();
}

#include "moc_scriptmanager.cpp"
