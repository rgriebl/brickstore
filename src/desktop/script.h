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

#include <QtQml/QQmlParserStatus>
#include <QtQuick/QQuickItem>
#include <QAction> // moved from QtWidgets to QtGui in Qt 6

QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

class QmlPrintJob;
class QmlBrickStore;

class View;
class Script;


class ExtensionScriptAction : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(Location location READ location WRITE setLocation NOTIFY locationChanged)
    Q_PROPERTY(QJSValue actionFunction READ actionFunction WRITE setActionFunction NOTIFY actionFunctionChanged)

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
    QJSValue actionFunction() const;
    void setActionFunction(const QJSValue &actionFunction);

    void executeAction();

signals:
    void textChanged(const QString &text);
    void locationChanged(ExtensionScriptAction::Location type);
    void actionFunctionChanged(const QJSValue &actionFunction);

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    QString m_text;
    Location m_type = Location::ExtrasMenu;
    QJSValue m_actionFunction;
    Script *m_script = nullptr;

    QAction *m_action = nullptr;
};


class PrintingScriptAction : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QJSValue printFunction READ printFunction WRITE setPrintFunction NOTIFY printFunctionChanged)

public:
    QString text() const;
    void setText(const QString &text);

    QJSValue printFunction() const;
    void setPrintFunction(const QJSValue &function);

    void executePrint(QPaintDevice *pd, View *win, bool selectionOnly, uint *maxPageCount = nullptr);

signals:
    void textChanged(const QString &text);
    void printFunctionChanged(const QJSValue &function);

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    QString m_text;
    QJSValue m_printFunction;
    Script *m_script = nullptr;
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

    void addExtensionAction(ExtensionScriptAction *extensionAction);
    void addPrintingAction(PrintingScriptAction *printingAction);

    QVector<ExtensionScriptAction *> extensionActions() const;
    QVector<PrintingScriptAction *> printingActions() const;

    QQmlEngine *qmlEngine() const;
    QQmlContext *qmlContext() const;
    QmlBrickStore *brickStoreWrapper() const;

signals:
    void nameChanged(QString name);
    void authorChanged(QString author);
    void versionChanged(QString version);
    void typeChanged(Script::Type type);

private:
    QString m_name;
    QString m_author;
    QString m_version;
    Type m_type = Type::ExtensionScript;

    QString m_fileName;
    QScopedPointer<QQmlEngine> m_engine;
    QQmlContext *m_context = nullptr;
    QQmlComponent *m_component = nullptr;

    QVector<ExtensionScriptAction *> m_extensionActions;
    QVector<PrintingScriptAction *> m_printingActions;

    friend class ScriptManager;
};

Q_DECLARE_METATYPE(ExtensionScriptAction *)
Q_DECLARE_METATYPE(PrintingScriptAction *)
Q_DECLARE_METATYPE(Script *)
