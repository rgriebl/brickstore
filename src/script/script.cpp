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
#include <QQmlInfo>
#include <QQmlEngine>
#include <QQmlContext>

#include "script.h"
#include "bricklink_wrapper.h"
#include "printjob.h"
#include "window.h"
#include "exception.h"


static QString formatJSError(const QJSValue &error)
{
    if (!error.isError())
        return { };

    QString msg = QString::fromLatin1("<b>%1</b><br/>%2<br/><br/>%3, line %4<br/><br/>Stacktrace:<br/>%5")
            .arg(error.property("name").toString())
            .arg(error.property("message").toString())
            .arg(error.property("fileName").toString())
            .arg(error.property("lineNumber").toInt())
            .arg(error.property("stack").toString());
    return msg;
}


QString ExtensionScriptAction::text() const
{
    return m_text;
}

void ExtensionScriptAction::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    emit textChanged(text);

    if (m_action)
        m_action->setText(text);
}

ExtensionScriptAction::Location ExtensionScriptAction::location() const
{
    return m_type;
}

void ExtensionScriptAction::setLocation(ExtensionScriptAction::Location type)
{
    if (type == m_type)
        return;
    m_type = type;
    emit locationChanged(type);
}

QJSValue ExtensionScriptAction::actionFunction() const
{
    return m_actionFunction;
}

void ExtensionScriptAction::setActionFunction(const QJSValue &actionFunction)
{
    if (!actionFunction.strictlyEquals(m_actionFunction)) {
        m_actionFunction = actionFunction;
        emit actionFunctionChanged(actionFunction);
    }
}

void ExtensionScriptAction::executeAction()
{
    if (!m_actionFunction.isCallable())
        throw Exception(tr("The extension script does not define an 'actionFunction'."));

    m_script->qmlEngine()->rootContext()->setProperty("isExtensionContext", true);

    QJSValueList args = { };
    QJSValue result = m_actionFunction.call(args);

    if (result.isError()) {
        throw Exception(tr("Extension script aborted with error:")
                        + QLatin1String("<br/><br/>") + formatJSError(result));

    }
}

void ExtensionScriptAction::classBegin()
{ }

void ExtensionScriptAction::componentComplete()
{
    if (auto *script = qobject_cast<Script *>(parent())) {
        script->addExtensionAction(this);
        m_script = script;
    } else {
        qmlWarning(this) << "ExtensionScriptAction objects need to be nested inside Script objects";
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QString PrintingScriptTemplate::text() const
{
    return m_text;
}

void PrintingScriptTemplate::setText(const QString &text)
{
    if (text != m_text) {
        m_text = text;
        emit textChanged(text);
    }
}

QJSValue PrintingScriptTemplate::printFunction() const
{
    return m_printFunction;
}

void PrintingScriptTemplate::setPrintFunction(const QJSValue &function)
{
    if (!function.strictlyEquals(m_printFunction)) {
        m_printFunction = function;
        emit printFunctionChanged(function);
    }
}

void PrintingScriptTemplate::classBegin()
{ }

void PrintingScriptTemplate::componentComplete()
{
    if (auto *script = qobject_cast<Script *>(parent())) {
        script->addPrintingTemplate(this);
        m_script = script;
    } else {
        qmlWarning(this) << "PrintingScriptTemplate objects need to be nested inside Script objects";
    }
}

void PrintingScriptTemplate::executePrint(QPaintDevice *pd, Window *win, bool selectionOnly)
{
    QScopedPointer<QmlWrapper::PrintJob> job(new QmlWrapper::PrintJob(pd));

    if (!m_printFunction.isCallable())
        throw Exception(tr("The printing script does not define a 'printFunction'."));

    auto wrappedDocView = m_script->brickStoreWrapper()->documentViewForWindow(win);

    if (!wrappedDocView)
        throw Exception(tr("Cannot print without a document."));

    const auto items = win->documentView()->sortItemList(selectionOnly ? win->selection()
                                                                       : win->document()->items());
    QVariantList itemList;
    for (auto item : items)
        itemList << QVariant::fromValue(QmlWrapper::InvItem(item, wrappedDocView->document()));

    QQmlEngine *engine = m_script->qmlEngine();
    QJSValueList args = { engine->toScriptValue(job.data()),
                          engine->toScriptValue(itemList),
                          engine->toScriptValue(wrappedDocView) };
    QJSValue result = m_printFunction.call(args);

    if (result.isError()) {
        throw Exception(tr("Print script aborted with error:")
                        + QLatin1String("<br/><br/>") + formatJSError(result));
    }

    if (job->isAborted())
        throw Exception(tr("Print job was aborted."));

    //job->dump();

    if (!job->print(0, job->pageCount() - 1))
        throw Exception(tr("Failed to start the print job."));
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

void Script::addExtensionAction(ExtensionScriptAction *extensionAction)
{
    m_extensionActions << extensionAction;
}

void Script::addPrintingTemplate(PrintingScriptTemplate *printingTemplate)
{
    m_printingTemplates << printingTemplate;
}

QVector<ExtensionScriptAction *> Script::extensionActions() const
{
    return m_extensionActions;
}

QVector<PrintingScriptTemplate *> Script::printingTemplates() const
{
    return m_printingTemplates;
}

QQmlEngine *Script::qmlEngine() const
{
    return m_engine.data();
}

QQmlContext *Script::qmlContext() const
{
    return m_context;
}

QmlWrapper::BrickStore *Script::brickStoreWrapper() const
{
    return m_brickStore;
}

#include "moc_script.cpp"
