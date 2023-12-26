// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
            m_errorString = m_errorString + u'\n' + error.toString();
    }
};

static QString stringifyType(QMetaType mt)
{
    if (mt.metaObject()) {
        int qei = mt.metaObject()->indexOfClassInfo("QML.Element");
        if (qei >= 0) {
            auto name = QString::fromLatin1(mt.metaObject()->classInfo(qei).value());
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
    default                     : return QString::fromLatin1(mt.name());
    }
}

static QString stringify(const QVariant &value, int level = 0, bool indentFirstLine = false);

static QString stringifyQObject(const QObject *o, const QMetaObject *mo, int level, bool indentFirstLine)
{
    QString str;
    QString indent = QString(level * 2, u' ');
    QString nextIndent = QString((level + 1) * 2, u' ');

    bool isGadget = mo->metaType().flags().testFlag(QMetaType::IsGadget);

    if (indentFirstLine)
        str.append(indent);

    str = str + QLatin1String(mo->className()) + u" {\n";

    if (!isGadget && mo->superClass()
            && ((mo->superClass() != &QObject::staticMetaObject) || !o->objectName().isEmpty())) {
        str = str + nextIndent + u"superclass: "
                + stringifyQObject(o, mo->superClass(), level + 1, false) + u'\n';
    }

    QByteArrayList noStringify;
    if (int nostr = mo->indexOfClassInfo("bsNoStringify"); nostr >= 0)
        noStringify = QByteArray(mo->classInfo(nostr).value()).split(',');

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        QString valueStr;
        if (noStringify.contains(p.name()))
            valueStr = u"..."_qs;
        else
            valueStr = stringify(isGadget ? p.readOnGadget(o) : p.read(o), level + 1, false);
        str = str + nextIndent + stringifyType(p.metaType()) + u' '
              + QString::fromLatin1(p.name()) + u": " + valueStr + u'\n';
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
            params.append(stringifyType(m.parameterMetaType(pi)) + u' ' + QLatin1String(pnames.at(pi)));

        str = str + nextIndent + stringifyType(m.returnMetaType()) + u' ' + QLatin1String(m.name())
                + u'(' + params.join(u", ") + u")\n";
    }
*/
    str = str + indent + u'}';
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
    QString indent = QString(level * 2, u' ');
    QString nextIndent = QString((level + 1) * 2, u' ');

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
                str = str + stringify(it.next(), level + 1, true);
                if (it.hasNext())
                    str.append(u',');
                str.append(u'\n');
            }
            str = str + indent + u']';
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
                str = str + nextIndent + it.key() + u": " + stringify(it.value(), level + 1, false);
                if (it.hasNext())
                    str.append(u',');
                str.append(u'\n');
            }
            str = str + indent + u'}';
        }
        break;
    }
    case QMetaType::QString: {
        if (level > 0)
            str = str + u'"' + value.toString().remove(u"\0"_qs) + u'"';
        else
            str = value.toString().remove(u"\0"_qs);
        break;
    }
    case QMetaType::QSize:
    case QMetaType::QSizeF: {
        const auto s = value.toSizeF();
        str = QString::number(s.width()) + u" x " + QString::number(s.height());
        break;
    }
    case QMetaType::QDateTime: {
        const auto dt = value.toDateTime();
        str = dt.isValid() ? dt.toString() : u"<invalid date>"_qs;
        break;
    }
    case QMetaType::QObjectStar: {
        auto *o = qvariant_cast<QObject *>(value);
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
            auto *o = qvariant_cast<QObject *>(value);
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

    for (const QString &path : std::as_const(spath)) {
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
                filePath = u"qrc://" + filePath.mid(1);

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

    // the Script cannot own these, as they have to be deleted, AFTER the Script object
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

    // Evaluating 's' via QQmlExpression would be straight forward, but this is problematic due
    // to QTBUG-33514 (singletons are not available inside QQmlExpressions) and BrickStore relying
    // heavily on singletons.

    const char *help =
            "  This is a JavaScript shell with full access to BrickStore's internals.\n"
            "  bl and bs are shortcuts for the BrickLink and BrickStore singletons.\n"
            "  The available JS API is documented here:\n"
            "    https://www.brickstore.dev/extensions\n";

    const char *script =
            "import BrickStore\n"
            "import BrickLink\n"
            "import QtQml\n"
            "QtObject {\n"
            "    property var bl: BrickLink\n"
            "    property var bs: BrickStore\n"
            "    property string help: \"${HELP}\"\n"
            "    property var __result\n"
            "    property var __error\n"
            "    Component.onCompleted: {\n"
            "        try { __result = function() { return ${SCRIPT} }() }\n"
            "        catch (error) { __error = error }\n"
            "    }\n"
            "}\n";

    QQmlComponent component(m_engine);
    component.setData(QByteArray(script).replace("${HELP}", help)
                          .replace("${SCRIPT}", s.toUtf8()), QUrl());
    if (component.status() == QQmlComponent::Error) {
        QStringList errorStrings;
        const auto errors = component.errors();
        errorStrings.reserve(errors.size());
        for (const auto &e : errors)
            errorStrings << e.description();
        return { u"JS compile error: "_qs + errorStrings.join(u", "_qs), false };
    }
    m_rootObject = component.create();

    QQmlExpression e(m_engine->rootContext(), m_rootObject, u"if (__error) throw __error; __result"_qs);
    bool isUndefined = false;
    auto result = e.evaluate(&isUndefined);

    if (e.hasError())
        return { e.error().description(), false };
    else if (isUndefined)
        return { QString { }, true };
    else
        return { stringify(result, 0, false), true };
}

void ScriptManager::clearScripts()
{
    for (auto *script : std::as_const(m_scripts)) {
        auto ctx = script->qmlContext();
        auto comp = script->qmlComponent();
        delete script;
        delete ctx;
        delete comp;
    }
    m_scripts.clear();
    m_engine->clearComponentCache();
}

#include "moc_scriptmanager.cpp"
