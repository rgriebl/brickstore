// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtQml/QQmlParserStatus>
#include <QtQml/QQmlInfo>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>
#include <QAction> // moved from QtWidgets to QtGui in Qt 6

QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

class Document;
class Script;


class ExtensionScriptAction : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
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

    ExtensionScriptAction(QObject *parent = nullptr);

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
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QJSValue printFunction READ printFunction WRITE setPrintFunction NOTIFY printFunctionChanged)

public:
    PrintingScriptAction(QObject *parent = nullptr);

    QString text() const;
    void setText(const QString &text);

    QJSValue printFunction() const;
    void setPrintFunction(const QJSValue &function);

    void executePrint(QPaintDevice *pd, Document *doc, bool selectionOnly, const QList<uint> &pages, uint *maxPageCount = nullptr);

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
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString author READ author WRITE setAuthor NOTIFY authorChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)

public:
    Script(QQuickItem *parent = nullptr);

    QString name() const;
    void setName(const QString &name);
    QString author() const;
    void setAuthor(const QString &author);
    QString version() const;
    void setVersion(const QString &version);

    void addExtensionAction(ExtensionScriptAction *extensionAction);
    void addPrintingAction(PrintingScriptAction *printingAction);

    QVector<ExtensionScriptAction *> extensionActions() const;
    QVector<PrintingScriptAction *> printingActions() const;

    QQmlContext *qmlContext() const;

signals:
    void nameChanged(QString name);
    void authorChanged(QString author);
    void versionChanged(QString version);

private:
    QQmlComponent *qmlComponent() const;

    QString m_name;
    QString m_author;
    QString m_version;

    QString m_fileName;
    QQmlContext *m_context = nullptr;
    QQmlComponent *m_component = nullptr;

    QVector<ExtensionScriptAction *> m_extensionActions;
    QVector<PrintingScriptAction *> m_printingActions;

    friend class ScriptManager;
};
