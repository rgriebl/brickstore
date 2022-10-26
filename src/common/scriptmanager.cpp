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

#include "utility/exception.h"
#include "script.h"
#include "scriptmanager.h"


Q_LOGGING_CATEGORY(LogScript, "script")


class QmlException : public Exception
{
public:
    QmlException(const QList<QQmlError> &errors, const char *msg)
        : Exception(msg)
    {
        for (auto &error : errors)
            m_message = m_message % u'\n' % error.toString();
    }
};

static QString stringifyType(QMetaType mt)
{
    if (mt.metaObject()) {
        int qei = mt.metaObject()->indexOfClassInfo("QML.Element");
        if (qei >= 0) {
            const auto name = QString::fromLatin1(mt.metaObject()->classInfo(qei).value());
            if (name == u"auto")
                return QString::fromLatin1(mt.metaObject()->className()).section(u"::"_qs, -1);
            else
                return name;
        }
    }
    switch (mt.id()) {
    case QMetaType::QString     : return u"string"_qs;
    case QMetaType::QStringList : return u"list<string>"_qs;
    case QMetaType::QVariant    : return u"var"_qs;
    case QMetaType::QVariantList: return u"list"_qs;
    case QMetaType::QVariantMap : return u"object"_qs;
    case QMetaType::QDate       :
    case QMetaType::QTime       :
    case QMetaType::QDateTime   : return u"date"_qs;
    case QMetaType::QSize       :
    case QMetaType::QSizeF      : return u"size"_qs;
    case QMetaType::QColor      : return u"color"_qs;
    case QMetaType::Float       :
    case QMetaType::Double      : return u"real"_qs;
    default                     : return QLatin1String(mt.name());
    }
}

static QString stringify(const QVariant &value, int level = 0, bool indentFirstLine = false);

static QString stringifyQObject(const QObject *o, const QMetaObject *mo, int level, bool indentFirstLine)
{
    QString str;
    QString indent = QString(level * 2, QLatin1Char(' '));
    QString nextIndent = QString((level + 1) * 2, QLatin1Char(' '));

    bool isGadget = mo->metaType().flags().testFlag(QMetaType::IsGadget);

    if (indentFirstLine)
        str.append(indent);

    str = str % QLatin1String(mo->className()) % u" {\n";

    if (!isGadget && mo->superClass()) {
        str = str % nextIndent % u"superclass: "
                % stringifyQObject(o, mo->superClass(), level + 1, false) % u'\n';
    }

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        QVariant value = isGadget ? p.readOnGadget(o) : p.read(o);
        str = str % nextIndent % stringifyType(p.metaType()) % u' '
                % QLatin1String(p.name()) % u": " % stringify(value, level + 1, false) % u'\n';
    }
/*
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod m = mo->method(i);
        switch (m.methodType()) {
        case QMetaMethod::Slot:   break;
        case QMetaMethod::Method: break;
        default: continue;
        }
        const auto pnames = m.parameterNames();
        QStringList params;
        for (int pi = 0; pi < m.parameterCount(); ++pi)
            params.append(stringifyType(m.parameterMetaType(pi)) % u' ' % QLatin1String(pnames.at(pi)));

        str = str % nextIndent % stringifyType(m.returnMetaType()) % u' ' % QLatin1String(m.name())
                % u'(' % params.join(u", ") % u")\n";
    }
*/
    str = str % indent % u'}';
    return str;
}

static QString stringifyQGadget(const void *g, const QMetaObject *mo, int level, bool indentFirstLine)
{
    Q_ASSERT(g && mo && mo->metaType().flags().testFlag(QMetaType::IsGadget));
    return stringifyQObject(reinterpret_cast<const QObject *>(g), mo, level, indentFirstLine);
}

static QString stringify(const QVariant &value, int level, bool indentFirstLine)
{
    QString str;
    QString indent = QString(level * 2, QLatin1Char(' '));
    QString nextIndent = QString((level + 1) * 2, QLatin1Char(' '));

    if (indentFirstLine)
        str.append(indent);

    switch (int(value.typeId())) {
    case QMetaType::QVariantList: {
        QListIterator<QVariant> it(value.toList());
        if (!it.hasNext()) {
            str.append(u"[]");
        } else {
            str = str.append(u"[\n");

            while (it.hasNext()) {
                str = str % stringify(it.next(), level + 1, true);
                if (it.hasNext())
                    str.append(u',');
                str.append(u'\n');
            }
            str = str % indent % u']';
        }
        break;
    }
    case QMetaType::QVariantMap: {
        QMapIterator<QString, QVariant> it(value.toMap());
        if (!it.hasNext()) {
            str.append(u"{}");
        } else {
            str.append(u"{\n");

            while (it.hasNext()) {
                it.next();
                str = str % nextIndent % it.key() % u": " % stringify(it.value(), level + 1, false);
                if (it.hasNext())
                    str.append(u',');
                str.append(u'\n');
            }
            str = str % indent % u'}';
        }
        break;
    }
    case QMetaType::QString: {
        if (level > 0)
            str = str % u'"' % value.toString().remove(u"\0"_qs) % u'"';
        else
            str = value.toString().remove(u"\0"_qs);
        break;
    }
    case QMetaType::QSize:
    case QMetaType::QSizeF: {
        const auto s = value.toSizeF();
        str = QString::number(s.width()) % u" x " % QString::number(s.height());
        break;
    }
    case QMetaType::QDateTime: {
        const auto dt = value.toDateTime();
        str = dt.isValid() ? dt.toString() : u"<invalid date>"_qs;
        break;
    }
    case QMetaType::QObjectStar: {
        QObject *o = qvariant_cast<QObject *>(value);
        if (!o) {
            str.append(u"<invalid QObject>");
            break;
        }
        str.append(stringifyQObject(o, o->metaObject(), level, false));
        break;
    }
    case QMetaType::Nullptr:
        str = u"null"_qs;
        break;
    default: {
        QMetaType meta(value.typeId());
        if (meta.flags().testFlag(QMetaType::IsGadget)) {
            str.append(stringifyQGadget(value.data(), meta.metaObject(), level, false));
        } else if (meta.flags().testFlag(QMetaType::PointerToQObject)) {
            QObject *o = qvariant_cast<QObject *>(value);
            if (!o) {
                str.append(u"<invalid QObject>");
                break;
            }
            str.append(stringifyQObject(o, o->metaObject(), level, false));
        } else {
            str.append(value.toString());
        }
        break;
    }
    }
    return str;
}


ScriptManager::ScriptManager(QQmlEngine *engine)
    : m_engine(engine)
{ }

ScriptManager::~ScriptManager()
{
    clearScripts();
    s_inst = nullptr;
}

ScriptManager *ScriptManager::s_inst = nullptr;

ScriptManager *ScriptManager::inst()
{
    return s_inst;
}

ScriptManager *ScriptManager::create(QQmlEngine *engine)
{
    Q_ASSERT(!s_inst);
    s_inst = new ScriptManager(engine);
    s_inst->reload();
    return s_inst;
}

bool ScriptManager::reload()
{
    emit aboutToReload();

    clearScripts();

    QStringList spath = { u":/extensions"_qs };
    QString dataloc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataloc.isEmpty())
        spath.prepend(dataloc + u"/extensions"_qs);

    for (const QString &path : qAsConst(spath)) {
        QDir dir(path);
        if (!path.startsWith(u':')) {
            qCInfo(LogScript) << "Loading scripts from directory:" << path;
            if (!dir.exists())
                dir.mkpath(u"."_qs);
        }

        const QFileInfoList fis = dir.entryInfoList(QStringList(u"*.bs.qml"_qs), QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : fis) {
            QString filePath = fi.absoluteFilePath();
            if (filePath.startsWith(u':'))
                filePath = u"qrc://" % filePath.mid(1);

            try {
                loadScript(filePath);

                qCInfo(LogScript).noquote() << "  [ ok ]" << fi.fileName();

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
    Q_ASSERT(m_engine);

    auto comp = new QQmlComponent(m_engine, fileName, m_engine);
    auto ctx = new QQmlContext(m_engine, m_engine);
    std::unique_ptr<QObject> root { comp->create(ctx) };
    if (!root)
        throw QmlException(comp->errors(), "Could not load QML file %1").arg(fileName);
    if (root && !qobject_cast<Script *>(root.get()))
        throw Exception("The root element of the script %1 is not 'Script'").arg(fileName);

    auto script = static_cast<Script *>(root.release());
    script->m_fileName = fileName;
    script->m_context = ctx;
    script->m_component = comp;

    m_scripts.append(script);
}

QVector<Script *> ScriptManager::scripts() const
{
    return m_scripts;
}

std::tuple<QString, bool> ScriptManager::executeString(const QString &s)
{
    Q_ASSERT(m_engine);

    const char script[] =
            "import BrickStore\n"
            "import BrickLink\n"
            "import QtQml\n"
            "QtObject {\n"
            "    property var bl: BrickLink\n"
            "    property var bs: BrickStore\n"
            "    property string help: \"Use 'bl'/'bs' to access the BrickLink/BrickStore singletons\"\n"
            "}\n";

    QQmlComponent component(m_engine);
    component.setData(script, QUrl());
    if (component.status() == QQmlComponent::Error)
        return { u"JS compile error: "_qs % component.errorString(), false };
    m_rootObject = component.create();

    QQmlExpression e(m_engine->rootContext(), m_rootObject, s);
    bool isUndefined = false;
    auto result = e.evaluate(&isUndefined);

    if (e.hasError())
        return { e.error().toString(), false };
    else if (isUndefined)
        return { QString { }, true };
    else
        return { stringify(result, 0, false), true };
}

void ScriptManager::clearScripts()
{
    qDeleteAll(m_scripts);
    m_scripts.clear();
}

#include "moc_scriptmanager.cpp"
