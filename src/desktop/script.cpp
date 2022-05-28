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
#include <QQmlInfo>
#include <QQmlEngine>
#include <QQmlContext>

#include "qmlapi/brickstore_wrapper.h"
#include "utility/exception.h"
#include "utility/utility.h"
#include "printjob.h"
#include "script.h"
#include "view.h"


static QString formatJSError(const QJSValue &error)
{
    if (!error.isError())
        return { };

    QString msg = QString::fromLatin1("<b>%1</b><br/>%2<br/><br/>%3, line %4<br/><br/>Stacktrace:<br/>%5")
            .arg(error.property("name"_l1).toString(),
                 error.property("message"_l1).toString(),
                 error.property("fileName"_l1).toString(),
                 error.property("lineNumber"_l1).toString(),
                 error.property("stack"_l1).toString());
    return msg;
}


/*! \qmltype ExtensionScriptAction
    \inqmlmodule BrickStore
    \ingroup qml-extension
    \brief Use this type to add an UI action to your extension.

    \note The documentation is missing on purpose - the API is not set in stone yet.
*/

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

    qmlEngine(m_script)->rootContext()->setProperty("isExtensionContext", true);

    QJSValueList args = { };
    QJSValue result = m_actionFunction.call(args);

    if (result.isError()) {
        throw Exception(tr("Extension script aborted with error:")
                        + "<br/><br/>"_l1 + formatJSError(result));

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

/*! \qmltype PrintingScriptAction
    \inqmlmodule BrickStore
    \ingroup qml-extension
    \brief Use this type to add a print action to your extension.
*/
/*! \qmlproperty string PrintingScriptAction::text
    The user visible text of the menu entry in the \c Extras menu, that triggers the printFunction.
*/
/*! \qmlproperty string PrintingScriptAction::printFunction
    This property holds a reference to the JavaScript function which should be called for printing.
    The function is called with three parameters: \c{(job, document, lots)}.

    \table
    \header
      \li Parameter
      \li Type
      \li Description
    \row
      \li \c job
      \li PrintJob
      \li The current print job.
    \row
      \li \c document
      \li Document
      \li The document that gets printed.
    \row
      \li \c lots
      \li list<\l{Lot}>
      \li The selected lots, or all lots if there is no selection.
    \endtable

    For example, the classic print script looks like this:
    \badcode
    PrintingScriptAction {
        text: "Print: Classic layout"
        printFunction: printJob
    }

    function printJob(job, doc, lots)
    { ... }
    \endcode
*/


QString PrintingScriptAction::text() const
{
    return m_text;
}

void PrintingScriptAction::setText(const QString &text)
{
    if (text != m_text) {
        m_text = text;
        emit textChanged(text);
    }
}

QJSValue PrintingScriptAction::printFunction() const
{
    return m_printFunction;
}

void PrintingScriptAction::setPrintFunction(const QJSValue &function)
{
    if (!function.strictlyEquals(m_printFunction)) {
        m_printFunction = function;
        emit printFunctionChanged(function);
    }
}

void PrintingScriptAction::classBegin()
{ }

void PrintingScriptAction::componentComplete()
{
    if (auto *script = qobject_cast<Script *>(parent())) {
        script->addPrintingAction(this);
        m_script = script;
    } else {
        qmlWarning(this) << "PrintingScriptAction objects need to be nested inside Script objects";
    }
}

void PrintingScriptAction::executePrint(QPaintDevice *pd, View *view, bool selectionOnly,
                                        const QList<uint> &pages, uint *maxPageCount)
{
    if (maxPageCount)
        *maxPageCount = 0;

    std::unique_ptr<QmlPrintJob> job(new QmlPrintJob(pd));

    if (!m_printFunction.isCallable())
        throw Exception(tr("The printing script does not define a 'printFunction'."));

    const auto lots = view->model()->sortLotList(selectionOnly ? view->selectedLots()
                                                               : view->model()->lots());
    QVariantList itemList;
    for (auto lot : lots)
        itemList << QVariant::fromValue(BrickLink::QmlLot(lot));

    QQmlEngine *engine = qmlEngine(m_script);
    QJSValueList args = { engine->toScriptValue(job.get()),
                          engine->toScriptValue(view->document()),
                          engine->toScriptValue(itemList) };
    QJSValue result = m_printFunction.call(args);

    if (result.isError()) {
        throw Exception(tr("Print script aborted with error:")
                        + "<br/><br/>"_l1 + formatJSError(result));
    }

    if (job->isAborted())
        throw Exception(tr("Print job was aborted."));

    //job->dump();

    if (!job->print(pages))
        throw Exception(tr("Failed to start the print job."));

    if (maxPageCount)
        *maxPageCount = uint(job->pageCount());
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Script
    \inqmlmodule BrickStore
    \ingroup qml-extension
    \brief The root element of any BrickStore extension file.

    The Script type is root element for any extension. The \c name, \c author and \c version
    properties are optional meta-data that should make it easier to manage extensions.

    There are currently two different types of extensions: UI extensions and print scripts. Any
    extension file can implement one or multiple types, although is practice is probably doesn't
    make too much sense to mix an UI extension implementation with a printing script.

    Print scripts have to add one or more PrintingScriptAction child elements to implement the
    actual printing.

    UI extensions have to add one or more ExtensionScriptAction child elements to create hooks
    into BrickStore's main UI.
*/
/*! \qmlproperty string Script::name
    \e {(Optional)} The name of this extension. This string is not user visible, but should correspond to the
    author's preferred file name (without the \c .bs.qml extension).
*/
/*! \qmlproperty string Script::author
    \e {(Optional)} The author's name and/or contact details.
*/
/*! \qmlproperty string Script::version
    \e {(Optional)} A version string for this script.
*/

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

void Script::addExtensionAction(ExtensionScriptAction *extensionAction)
{
    m_extensionActions << extensionAction;
}

void Script::addPrintingAction(PrintingScriptAction *printingAction)
{
    m_printingActions << printingAction;
}

QVector<ExtensionScriptAction *> Script::extensionActions() const
{
    return m_extensionActions;
}

QVector<PrintingScriptAction *> Script::printingActions() const
{
    return m_printingActions;
}

QQmlContext *Script::qmlContext() const
{
    return m_context;
}

QmlBrickStore *Script::brickStoreWrapper() const
{
    return QmlBrickStore::inst();
}

#include "moc_script.cpp"
