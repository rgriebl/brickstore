/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QQmlExpression>
#include <QStringBuilder>

#include "common/application.h"
#include "qmlapi/common.h"
#include "qmlapi/bricklink_wrapper.h"
#include "qmlapi/brickstore_wrapper.h"
#include "utility/exception.h"
#include "printjob.h"
#include "script.h"
#include "scriptmanager.h"


Q_LOGGING_CATEGORY(LogScript, "script")


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
}

ScriptManager *ScriptManager::s_inst = nullptr;

ScriptManager *ScriptManager::inst()
{
    if (!s_inst)
        s_inst = new ScriptManager();
    return s_inst;
}

void ScriptManager::initialize()
{
    QmlBrickLink::registerTypes();
    QmlBrickStore::registerTypes();
    QmlPrintJob::registerTypes();

    qmlRegisterType<Script>("BrickStore", 1, 0, "Script");
    qmlRegisterType<ExtensionScriptAction>("BrickStore", 1, 0, "ExtensionScriptAction");
    qmlRegisterType<PrintingScriptAction>("BrickStore", 1, 0, "PrintingScriptAction");

    reload();
}

bool ScriptManager::reload()
{
    emit aboutToReload();

    clearScripts();

    QStringList spath = { ":/extensions"_l1 };
    QString dataloc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataloc.isEmpty())
        spath.prepend(dataloc + "/extensions"_l1);

    for (const QString &path : qAsConst(spath)) {
        QDir dir(path);
        if (!path.startsWith(':'_l1)) {
            qCDebug(LogScript) << "Loading scripts from directory:" << path;
            if (!dir.exists())
                dir.mkpath('.'_l1);
        }

        const QFileInfoList fis = dir.entryInfoList(QStringList("*.bs.qml"_l1), QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : fis) {
            QString filePath = fi.absoluteFilePath();
            if (filePath.startsWith(u":"))
                filePath = u"qrc://" % filePath.mid(1);

            try {
                loadScript(filePath);

                qCDebug(LogScript).noquote() << "  [ ok ]" << fi.fileName();

            } catch (const Exception &e) {
                qCWarning(LogScript).noquote() << "  [fail]" << fi.fileName() << ":" << e.what();
            }
        }
    }
    emit reloaded();

    return !m_scripts.isEmpty();
}

void ScriptManager::loadScript(const QString &fileName)
{
    QScopedPointer<QQmlEngine> engine(new QQmlEngine(this));

    redirectQmlEngineWarnings(engine.get(), LogScript());

    auto comp = new QQmlComponent(engine.get(), fileName, engine.get());
    auto ctx = new QQmlContext(engine.get(), engine.get());
    QScopedPointer<QObject> root(comp->create(ctx));
    if (!root)
        throw QmlException(comp->errors(), "Could not load QML file %1").arg(fileName);
    if (root && !qobject_cast<Script *>(root.data()))
        throw Exception("The root element of the script %1 is not 'Script'").arg(fileName);

    auto script = static_cast<Script *>(root.take());
    script->m_engine.reset(engine.take());
    script->m_fileName = fileName;
    script->m_context = ctx;
    script->m_component = comp;

    m_scripts.append(script);
}

QVector<Script *> ScriptManager::scripts() const
{
    return m_scripts;
}

bool ScriptManager::executeString(const QString &s)
{
    if (!m_engine) {
        m_engine.reset(new QQmlEngine(this));
        redirectQmlEngineWarnings(m_engine.data(), LogScript());

        const char script[] =
                "import BrickStore 1.0\n"
                "import QtQml 2.12\n"
                "QtObject {\n"
                "    property var bl: BrickLink\n"
                "    property var bs: BrickStore\n"
                "    property string help: \"Use 'bl'/'bs' to access the BrickLink/BrickStore singletons\"\n"
                "}\n";

        QQmlComponent component(m_engine.data());
        component.setData(script, QUrl());
        if (component.status() == QQmlComponent::Error)
            qCWarning(LogScript) << "JS compile error:" << component.errorString();
        m_rootObject = component.create();
    }
    QQmlExpression e(m_engine->rootContext(), m_rootObject, s);
    qCDebug(LogScript).noquote() << "#" << s;
    auto result = e.evaluate();
    if (e.hasError()) {
        qCWarning(LogScript).noquote() << "> ERROR:" << e.error().toString();
        return false;
    } else {
        auto s = result.toString();
        if (!s.isEmpty())
            qCDebug(LogScript).noquote() << ">" << s;
        return true;
    }
}

void ScriptManager::clearScripts()
{
    qDeleteAll(m_scripts);
    m_scripts.clear();
}

#include "moc_scriptmanager.cpp"
