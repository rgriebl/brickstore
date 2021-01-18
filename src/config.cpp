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
#include <cstdlib>
#include <ctime>

#include <QGlobalStatic>
#include <QStringList>
#include <QDir>
#include <QDomDocument>
#include <QStandardPaths>
#include <QSize>
#include <QUuid>
#include <QDebug>
#include <QCryptographicHash>
#include <QStringBuilder>

#include "config.h"
#include "currency.h"
#include "version.h"

#define XSTR(a) #a
#define STR(a) XSTR(a)

namespace {

static inline qint32 mkver(int a, int b, int c)
{
    return (qint32(a) << 16) | (qint32(b) << 8) | (qint32(c));
}

} // namespace

static const char *organization_v12x = "patrickbrans.com";
static const char *application_v12x = "BrickStock";
#if defined(Q_OS_MACOS)
static const char *organization_v11x = "softforge.de";
static const char *organization = "brickstore.org";
#else
static const char *organization_v11x = "softforge";
static const char *organization = BRICKSTORE_NAME;
#endif
static const char *application_v11x = "BrickStore";
static const char *application = BRICKSTORE_NAME;


Config::Config()
    : QSettings(organization, application)
{
    m_show_input_errors = value("General/ShowInputErrors", true).toBool();
    m_measurement = (value("General/MeasurementSystem").toString() == QLatin1String("imperial"))
            ? QLocale::ImperialSystem : QLocale::MetricSystem;
    m_translations_parsed = false;
}

Config::~Config()
{
    s_inst = nullptr;
}

Config *Config::s_inst = nullptr;

Config *Config::inst()
{
    if (!s_inst)
        s_inst = new Config();
    return s_inst;
}

QString Config::scramble(const QString &str)
{
    QString result;
    const QChar *unicode = str.unicode();
    for (int i = 0; i < str.length(); i++)
        result += (unicode [i].unicode() < 0x20) ? unicode [i] :
                  QChar(0x1001F - unicode [i].unicode());
    return result;
}

void Config::upgrade(int vmajor, int vminor)
{
    QStringList sl;

    // no upgrades available, uncomment if needed later on
    //int cfgver = value("General/ConfigVersion", 0).toInt();
    setValue("General/ConfigVersion", mkver(vmajor, vminor, 0));

    auto copyOldConfig = [this](const char *org, const char *app) -> bool {
        static const std::vector<const char *> ignore = {
            "MainWindow/",
            "General/Registration/",
            "General/ConfigVersion",
            "General/SimpleMode",
            "General/lastApplicationUpdateCheck",
            "Internet/UseProxy",
            "Internet/Proxy"
        };

        QSettings old(org, app);
        if (old.value("General/ConfigVersion", 0).toInt()) {
            const auto allOldKeys = old.allKeys();
            for (const QString &key : allOldKeys) {
                bool skip = false;
                for (const char *ign : ignore) {
                    if (key.startsWith(ign, Qt::CaseInsensitive))
                        skip = true;
                }
                if (!skip)
                    setValue(key, old.value(key));
            }
            return true;
        } else {
            return false;
        }
    };

    // import old settings from BrickStore 1.1.x
    if (!value("General/ImportedV11xSettings", 0).toInt()) {
        if (copyOldConfig(organization_v11x, application_v11x))
            setValue("General/ImportedV11xSettings", 1);
    }
    // import old settings from BrickStock 1.2.x
    if (!value("General/ImportedV12xSettings", 0).toInt()) {
        if (copyOldConfig(organization_v12x, application_v12x))
            setValue("General/ImportedV12xSettings", 1);
    }

    // do config upgrades as needed
//    if (cfgver < mkver(2021, 1, 1)) {
//    }
}

QStringList Config::recentFiles() const
{
    return value("/Files/Recent").toStringList();
}

void Config::setRecentFiles(const QStringList &recent)
{
    if (recent != recentFiles()) {
        setValue("/Files/Recent", recent);
        emit recentFilesChanged(recent);
    }
}

void Config::addToRecentFiles(const QString &file)
{
    QString name = QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath());

    auto recent = recentFiles();
    recent.removeAll(name);
    recent.prepend(name);
    if (recent.size() > MaxRecentFiles)
        recent = recent.mid(0, MaxRecentFiles);

    setRecentFiles(recent);
}

bool Config::closeEmptyDocuments() const
{
    return value("General/CloseEmptyDocs", false).toBool();
}

void Config::setCloseEmptyDocuments(bool b)
{
    setValue("General/CloseEmptyDocs", b);
}

bool Config::showInputErrors() const
{
    return m_show_input_errors;
}

bool Config::simpleMode() const
{
    return m_simple_mode;
}

void Config::setShowInputErrors(bool b)
{
    if (b != m_show_input_errors) {
        m_show_input_errors = b;
        setValue("General/ShowInputErrors", b);

        emit showInputErrorsChanged(b);
    }
}

void Config::setSimpleMode(bool b)
{
    if (b != m_simple_mode) {
        m_simple_mode = b;
        setValue("General/SimpleMode", b);

        emit simpleModeChanged(b);
    }
}

void Config::setLDrawDir(const QString &dir)
{
    setValue("General/LDrawDir", dir);
}

QString Config::ldrawDir() const
{
    return value("General/LDrawDir").toString();
}

QString Config::documentDir() const
{
    QString dir = value("General/DocDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    if (dir.isEmpty())
        dir = QDir::homePath();

    return dir;
}

void Config::setDocumentDir(const QString &dir)
{
    setValue("General/DocDir", dir);
}


QString Config::dataDir() const
{
    static QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return value("BrickLink/DatDir", cacheDir).toString();
}

void Config::setDataDir(const QString &dir)
{
    setValue("BrickLink/DataDir", dir);
}

QString Config::language() const
{
    return value("General/Locale", QLocale::system().name()).toString();
}

void Config::setLanguage(const QString &lang)
{
    QString old = language();

    if (old != lang) {
        setValue("General/Locale", lang);

        emit languageChanged();
    }
}

QLocale::MeasurementSystem Config::measurementSystem() const
{
    return m_measurement;
}

void Config::setMeasurementSystem(QLocale::MeasurementSystem ms)
{
    if (ms != m_measurement) {
        m_measurement= ms;
        setValue("General/MeasurementSystem", ms == QLocale::MetricSystem ? QLatin1String("metric")
                                                                          : QLatin1String("imperial"));
        emit measurementSystemChanged(ms);
    }
}


QMap<QByteArray, int> Config::updateIntervals() const
{
    QMap<QByteArray, int> uiv = updateIntervalsDefault();

    static const char *lut[] = { "Picture", "PriceGuide", "Database" };

    for (const auto &iv : lut)
        uiv[iv] = value(QLatin1String("BrickLink/UpdateInterval/") + QLatin1String(iv), uiv[iv]).toInt();
    return uiv;
}

QMap<QByteArray, int> Config::updateIntervalsDefault() const
{
    const int day2sec = 60*60*24;
    QMap<QByteArray, int> uiv;

    uiv.insert("Picture",   180 * day2sec);
    uiv.insert("PriceGuide", 14 * day2sec);
    uiv.insert("Database",    7 * day2sec);

    return uiv;
}

void Config::setUpdateIntervals(const QMap<QByteArray, int> &uiv)
{
    bool modified = false;
    QMap<QByteArray, int> old_uiv = updateIntervals();

    for (QMapIterator<QByteArray, int> it(uiv); it.hasNext(); ) {
        it.next();

        if (it.value() != old_uiv.value(it.key())) {
            setValue(QLatin1String("BrickLink/UpdateInterval/") + it.key(), it.value());
            modified = true;
        }
    }

    if (modified)
        emit updateIntervalsChanged(updateIntervals());
}

void Config::setFontSizePercent(int p)
{
    auto oldp = fontSizePercent();

    if (oldp != p) {
        setValue("Interface/FontSizePercent", qBound(50, p, 200));
        emit fontSizePercentChanged(p);
    }
}

void Config::setIconSize(const QSize &iconSize)
{
    if (!iconSize.isValid() || (iconSize.width() != iconSize.height()))
        return;

    auto oldIconSize = this->iconSize();

    if (oldIconSize != iconSize) {
        setValue("Interface/IconSize", iconSize.width());
        emit iconSizeChanged(iconSize);
    }
}

void Config::setItemImageSizePercent(int p)
{
    auto oldp = itemImageSizePercent();

    if (oldp != p) {
        setValue("Interface/ItemImageSizePercent", qBound(50, p, 200));
        emit itemImageSizePercentChanged(p);
    }
}

QByteArray Config::columnLayout(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value(u"ColumnLayouts/" % id % u"/Layout").value<QByteArray>();
}

QString Config::columnLayoutName(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value(u"ColumnLayouts/" % id % u"/Name").toString();
}

int Config::columnLayoutOrder(const QString &id) const
{
    if (id.isEmpty())
        return -1;
    return value(u"ColumnLayouts/" % id % u"/Order", -1).toInt();
}

QStringList Config::columnLayoutIds() const
{
    const_cast<Config *>(this)->beginGroup("ColumnLayouts");
    auto sl = childGroups();
    sl.removeOne("user-default");
    const_cast<Config *>(this)->endGroup();
    return sl;
}

QString Config::setColumnLayout(const QString &id, const QByteArray &layout)
{
    bool isNew = id.isEmpty();

    if (isNew || (layout != columnLayout(id))) {
        QString nid = id;

        if (isNew) {
            nid = QUuid::createUuid().toString();
        } else {
            if (id != u"user-default") {
                beginGroup("ColumnLayouts");
                bool hasLayout = childGroups().contains(id);
                endGroup();
                if (!hasLayout)
                    return { };
            }
        }
        setValue(u"ColumnLayouts/" % nid % u"/Layout", layout);

        if (isNew) {
            auto newIds = columnLayoutIds();
            setValue(u"ColumnLayouts/" % nid % u"/Order", newIds.count() - 1);
            emit columnLayoutIdsChanged(newIds);
        }
        emit columnLayoutChanged(nid, layout);
        return nid;
    }
    return { };
}

bool Config::deleteColumnLayout(const QString &id)
{
    if (!id.isEmpty()) {
        beginGroup("ColumnLayouts");
        bool hasLayout = childGroups().contains(id);
        endGroup();
        if (hasLayout) {
            remove(u"ColumnLayouts/" % id);
            emit columnLayoutIdsChanged(columnLayoutIds());
            return true;
        }
    }
    return false;
}

bool Config::renameColumnLayout(const QString &id, const QString &name)
{
    if (!id.isEmpty()) {
        beginGroup("ColumnLayouts");
        bool hasLayout = childGroups().contains(id);
        endGroup();

        if (hasLayout) {
            QString oldname = value(u"ColumnLayouts/" % id % u"/Name").toString();
            if (oldname != name) {
                setValue(u"ColumnLayouts/" % id % u"/Name", name);
                emit columnLayoutNameChanged(id, name);
                return true;
            }
        }
    }
    return false;
}

bool Config::reorderColumnLayouts(const QStringList &ids)
{
    auto newIds = ids;
    auto oldIds = columnLayoutIds();
    newIds.sort();
    oldIds.sort();

    if (oldIds == newIds) {
        for (int i = 0; i < ids.count(); ++i)
            setValue(u"ColumnLayouts/" % ids.at(i) % u"/Order", i);
        emit columnLayoutIdsOrderChanged(ids);
        return true;
    }
    return false;
}


QPair<QString, QString> Config::loginForBrickLink() const
{
    return qMakePair(value("BrickLink/Login/Username").toString(),
                     scramble(value("BrickLink/Login/Password").toString()));
}

void Config::setLoginForBrickLink(const QString &name, const QString &pass)
{
    setValue("BrickLink/Login/Username", name);
    setValue("BrickLink/Login/Password", scramble(pass));
}


bool Config::onlineStatus() const
{
    return value("Internet/Online", true).toBool();
}

void Config::setOnlineStatus(bool b)
{
    bool ob = onlineStatus();

    if (b != ob) {
        setValue("Internet/Online", b);

        emit onlineStatusChanged(b);
    }
}

QVector<Config::Translation> Config::translations() const
{
    if (!m_translations_parsed)
        m_translations_parsed = parseTranslations();
    return m_translations;
}

int Config::fontSizePercent() const
{
    return value("Interface/FontSizePercent", 100).toInt();
}

int Config::itemImageSizePercent() const
{
    return value("Interface/ItemImageSizePercent", 100).toInt();
}

QSize Config::iconSize() const
{
    int s = value("Interface/IconSize", 0).toInt();
    return { s, s };
}

bool Config::parseTranslations() const
{
    m_translations.clear();

    QDomDocument doc;
    QFile file(":/i18n/translations.xml");

    if (file.open(QIODevice::ReadOnly)) {
        QString err_str;
        int err_line = 0, err_col = 0;

        if (doc.setContent(&file, &err_str, &err_line, &err_col)) {
            QDomElement root = doc.documentElement();

            if (root.isElement() && root.nodeName() == QLatin1String("translations")) {
                for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
                    if (!node.isElement() || (node.nodeName() != QLatin1String("translation")))
                        continue;
                    QDomNamedNodeMap map = node.attributes();
                    Translation trans;

                    trans.language = map.namedItem(QLatin1String("lang")).toAttr().value();
                    trans.author = map.namedItem(QLatin1String("author")).toAttr().value();
                    trans.authorEMail = map.namedItem(QLatin1String("authoremail")).toAttr().value();

                    if (trans.language.isEmpty())
                        goto error;

                    for (QDomNode name = node.firstChild(); !name.isNull(); name = name.nextSibling()) {
                        if (!name.isElement() || (name.nodeName() != QLatin1String("name")))
                            continue;
                        QDomNamedNodeMap nameAttribs = name.attributes();

                        QString tr_id = nameAttribs.namedItem(QLatin1String("lang")).toAttr().value();
                        QString tr_name = name.toElement().text();

                        if (!tr_name.isEmpty())
                            trans.languageName[tr_id.isEmpty() ? QLatin1String("en") : tr_id] = tr_name;
                    }

                    if (trans.languageName.isEmpty())
                        goto error;

                    m_translations << trans;
                }
                return true;
            }
        }
    }
error:
    return false;
}

void Config::setDefaultCurrencyCode(const QString &currencyCode)
{
    setValue("Currency/Default", currencyCode);

    emit defaultCurrencyCodeChanged(currencyCode);
}

QString Config::defaultCurrencyCode() const
{
    return value("Currency/Default", QLatin1String("USD")).toString();
}

#include "moc_config.cpp"
