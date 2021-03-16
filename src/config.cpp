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
#include <QKeySequence>
#include <QStringBuilder>

#include "utility.h"
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
    : QSettings(QLatin1String(organization), QLatin1String(application))
{
    m_show_input_errors = value("General/ShowInputErrors"_l1, true).toBool();
    m_show_difference_indicators = value("General/ShowDifferenceIndicators"_l1, false).toBool();
    m_measurement = (value("General/MeasurementSystem"_l1).toString() == "imperial"_l1)
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

QPair<QString, double> Config::legacyCurrencyCodeAndRate() const
{
    const QString localCCode = QLocale::system().currencySymbol(QLocale::CurrencyIsoCode);

    QSettings old_v12x(QLatin1String { organization_v12x }, QLatin1String { application_v12x });
    bool localized = old_v12x.value("/General/Money/Localized"_l1, false).toBool();
    if (localized) {
        return qMakePair(localCCode, old_v12x.value("/General/Money/Factor"_l1, 1).toDouble());
    } else {
        QSettings old_v11x(QLatin1String { organization_v11x }, QLatin1String { application_v11x });
        localized = old_v11x.value("/General/Money/Localized"_l1, false).toBool();
        if (localized)
            return qMakePair(localCCode, old_v11x.value("/General/Money/Factor"_l1, 1).toDouble());
    }
    return qMakePair(QString(), 0);
}

void Config::upgrade(int vmajor, int vminor)
{
    QStringList sl;

    // no upgrades available, uncomment if needed later on
    //int cfgver = value("General/ConfigVersion", 0).toInt();
    setValue("General/ConfigVersion"_l1, mkver(vmajor, vminor, 0));

    auto copyOldConfig = [this](const char *org, const char *app) -> bool {
        static const std::vector<const char *> ignore = {
            "MainWindow/",
            "General/Registration/",
            "General/ConfigVersion",
            "General/lastApplicationUpdateCheck",
            "Internet/UseProxy",
            "Internet/Proxy"
        };

        QSettings old(QLatin1String { org }, QLatin1String { app });
        if (old.value("General/ConfigVersion"_l1, 0).toInt()) {
            const auto allOldKeys = old.allKeys();
            for (const QString &key : allOldKeys) {
                bool skip = false;
                for (const char *ign : ignore) {
                    if (key.startsWith(QLatin1String(ign), Qt::CaseInsensitive))
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
    if (!value("General/ImportedV11xSettings"_l1, 0).toInt()) {
        if (copyOldConfig(organization_v11x, application_v11x))
            setValue("General/ImportedV11xSettings"_l1, 1);
    }
    // import old settings from BrickStock 1.2.x
    if (!value("General/ImportedV12xSettings"_l1, 0).toInt()) {
        if (copyOldConfig(organization_v12x, application_v12x))
            setValue("General/ImportedV12xSettings"_l1, 1);
    }

    // do config upgrades as needed
//    if (cfgver < mkver(2021, 1, 1)) {
//    }
}

QStringList Config::recentFiles() const
{
    return value("/Files/Recent"_l1).toStringList();
}

void Config::setRecentFiles(const QStringList &recent)
{
    if (recent != recentFiles()) {
        setValue("/Files/Recent"_l1, recent);
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

bool Config::showInputErrors() const
{
    return m_show_input_errors;
}

void Config::setShowInputErrors(bool b)
{
    if (b != m_show_input_errors) {
        m_show_input_errors = b;
        setValue("General/ShowInputErrors"_l1, b);

        emit showInputErrorsChanged(b);
    }
}

bool Config::showDifferenceIndicators() const
{
    return m_show_difference_indicators;
}

void Config::setShowDifferenceIndicators(bool b)
{
    if (b != m_show_difference_indicators) {
        m_show_difference_indicators = b;
        setValue("General/ShowDifferenceIndicators"_l1, b);

        emit showDifferenceIndicatorsChanged(b);
    }
}

void Config::setLDrawDir(const QString &dir)
{
    setValue("General/LDrawDir"_l1, dir);
}

QString Config::ldrawDir() const
{
    return value("General/LDrawDir"_l1).toString();
}

QString Config::documentDir() const
{
    QString dir = value("General/DocDir"_l1, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    if (dir.isEmpty())
        dir = QDir::homePath();

    return dir;
}

void Config::setDocumentDir(const QString &dir)
{
    setValue("General/DocDir"_l1, dir);
}


QString Config::dataDir() const
{
    static QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return value("BrickLink/DatDir"_l1, cacheDir).toString();
}

void Config::setDataDir(const QString &dir)
{
    setValue("BrickLink/DataDir"_l1, dir);
}

QString Config::language() const
{
    return value("General/Locale"_l1, QLocale::system().name().left(2)).toString();
}

void Config::setLanguage(const QString &lang)
{
    QString old = language();

    if (old != lang) {
        setValue("General/Locale"_l1, lang);

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
        setValue("General/MeasurementSystem"_l1, (ms == QLocale::MetricSystem) ? "metric"_l1
                                                                               : "imperial"_l1);
        emit measurementSystemChanged(ms);
    }
}

bool Config::areFiltersInFavoritesMode() const
{
    return true;
    //return value("General/FilterMode", "history").toString() == "favorites";
}

void Config::setFiltersInFavoritesMode(bool /*b*/)
{
//    if (areFiltersInFavoritesMode() != b) {
//        setValue("General/FilterMode", b ? "favorites" : "history");
//        emit filtersInFavoritesModeChanged(b);
//    }
}

QMap<QByteArray, int> Config::updateIntervals() const
{
    QMap<QByteArray, int> uiv = updateIntervalsDefault();

    static const char *lut[] = { "Picture", "PriceGuide", "Database" };

    for (const auto &iv : lut)
        uiv[iv] = value("BrickLink/UpdateInterval/"_l1 + QLatin1String(iv), uiv[iv]).toInt();
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
            setValue("BrickLink/UpdateInterval/"_l1 % QLatin1String(it.key()), it.value());
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
        setValue("Interface/FontSizePercent"_l1, qBound(50, p, 200));
        emit fontSizePercentChanged(p);
    }
}

void Config::setIconSize(const QSize &iconSize)
{
    if (!iconSize.isValid() || (iconSize.width() != iconSize.height()))
        return;

    auto oldIconSize = this->iconSize();

    if (oldIconSize != iconSize) {
        setValue("Interface/IconSize"_l1, iconSize.width());
        emit iconSizeChanged(iconSize);
    }
}

void Config::setItemImageSizePercent(int p)
{
    auto oldp = itemImageSizePercent();

    if (oldp != p) {
        setValue("Interface/ItemImageSizePercent"_l1, qBound(50, p, 200));
        emit itemImageSizePercentChanged(p);
    }
}

QByteArray Config::columnLayout(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value("ColumnLayouts/"_l1 % id % "/Layout"_l1).value<QByteArray>();
}

QString Config::columnLayoutName(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value("ColumnLayouts/"_l1 % id % "/Name"_l1).toString();
}

int Config::columnLayoutOrder(const QString &id) const
{
    if (id.isEmpty())
        return -1;
    return value("ColumnLayouts/"_l1 % id % "/Order"_l1, -1).toInt();
}

QStringList Config::columnLayoutIds() const
{
    const_cast<Config *>(this)->beginGroup("ColumnLayouts"_l1);
    auto sl = childGroups();
    sl.removeOne("user-default"_l1);
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
            if (id != "user-default"_l1) {
                beginGroup("ColumnLayouts"_l1);
                bool hasLayout = childGroups().contains(id);
                endGroup();
                if (!hasLayout)
                    return { };
            }
        }
        setValue("ColumnLayouts/"_l1 % nid % "/Layout"_l1, layout);

        if (isNew) {
            auto newIds = columnLayoutIds();
            setValue("ColumnLayouts/"_l1 % nid % "/Order"_l1, newIds.count() - 1);
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
        beginGroup("ColumnLayouts"_l1);
        bool hasLayout = childGroups().contains(id);
        endGroup();
        if (hasLayout) {
            remove("ColumnLayouts/"_l1 % id);
            emit columnLayoutIdsChanged(columnLayoutIds());
            return true;
        }
    }
    return false;
}

bool Config::renameColumnLayout(const QString &id, const QString &name)
{
    if (!id.isEmpty()) {
        beginGroup("ColumnLayouts"_l1);
        bool hasLayout = childGroups().contains(id);
        endGroup();

        if (hasLayout) {
            QString oldname = value("ColumnLayouts/"_l1 % id % "/Name"_l1).toString();
            if (oldname != name) {
                setValue("ColumnLayouts/"_l1 % id % "/Name"_l1, name);
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
            setValue("ColumnLayouts/"_l1 % ids.at(i) % "/Order"_l1, i);
        emit columnLayoutIdsOrderChanged(ids);
        return true;
    }
    return false;
}

QVariantMap Config::shortcuts() const
{
    return value("General/Shortcuts"_l1).toMap();
}

void Config::setShortcuts(const QVariantMap &list)
{
    if (shortcuts() != list) {
        setValue("General/Shortcuts"_l1, list);
        emit shortcutsChanged(list);
    }
}

QPair<QString, QString> Config::loginForBrickLink() const
{
    return qMakePair(value("BrickLink/Login/Username"_l1).toString(),
                     scramble(value("BrickLink/Login/Password"_l1).toString()));
}

void Config::setLoginForBrickLink(const QString &name, const QString &pass)
{
    setValue("BrickLink/Login/Username"_l1, name);
    setValue("BrickLink/Login/Password"_l1, scramble(pass));
}


bool Config::onlineStatus() const
{
    return value("Internet/Online"_l1, true).toBool();
}

void Config::setOnlineStatus(bool b)
{
    bool ob = onlineStatus();

    if (b != ob) {
        setValue("Internet/Online"_l1, b);

        emit onlineStatusChanged(b);
    }
}

Config::PartOutMode Config::partOutMode() const
{
    int v = value("General/PartOutMode"_l1).toInt();
    if ((v < int(PartOutMode::Ask)) || (v > int(PartOutMode::NewDocument)))
         v = int(PartOutMode::Ask);
    return PartOutMode(v);
}

void Config::setPartOutMode(Config::PartOutMode pom)
{
    setValue("General/PartOutMode"_l1, int(pom));
}

bool Config::visualChangesMarkModified() const
{
    return value("General/VisualChangesMarkModified"_l1, false).toBool();
}

void Config::setVisualChangesMarkModified(bool b)
{
    if (visualChangesMarkModified() != b) {
        setValue("General/VisualChangesMarkModified"_l1, b);
        emit visualChangesMarkModifiedChanged(b);
    }
}

bool Config::restoreLastSession() const
{
    return value("General/RestoreLastSession"_l1, true).toBool();
}

void Config::setRestoreLastSession(bool b)
{
    setValue("General/RestoreLastSession"_l1, b);
}

bool Config::openBrowserOnExport() const
{
    return value("/General/Export/OpenBrowser"_l1, true).toBool();
}

void Config::setOpenBrowserOnExport(bool b)
{
    setValue("/General/Export/OpenBrowser"_l1, b);
}

QVector<Config::Translation> Config::translations() const
{
    if (!m_translations_parsed)
        m_translations_parsed = parseTranslations();
    return m_translations;
}

int Config::fontSizePercent() const
{
    return value("Interface/FontSizePercent"_l1, 100).toInt();
}

int Config::itemImageSizePercent() const
{
    return value("Interface/ItemImageSizePercent"_l1, 100).toInt();
}

QSize Config::iconSize() const
{
    int s = value("Interface/IconSize"_l1, 0).toInt();
    return { s, s };
}

bool Config::parseTranslations() const
{
    m_translations.clear();

    QDomDocument doc;
    QFile file(":/i18n/translations.xml"_l1);

    if (file.open(QIODevice::ReadOnly)) {
        QString err_str;
        int err_line = 0, err_col = 0;

        if (doc.setContent(&file, &err_str, &err_line, &err_col)) {
            QDomElement root = doc.documentElement();

            if (root.isElement() && root.nodeName() == "translations"_l1) {
                for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
                    if (!node.isElement() || (node.nodeName() != "translation"_l1))
                        continue;
                    QDomNamedNodeMap map = node.attributes();
                    Translation trans;

                    trans.language = map.namedItem("lang"_l1).toAttr().value();
                    trans.author = map.namedItem("author"_l1).toAttr().value();
                    trans.authorEMail = map.namedItem("authoremail"_l1).toAttr().value();

                    if (trans.language.isEmpty())
                        goto error;

                    for (QDomNode name = node.firstChild(); !name.isNull(); name = name.nextSibling()) {
                        if (!name.isElement() || (name.nodeName() != "name"_l1))
                            continue;
                        QDomNamedNodeMap nameAttribs = name.attributes();

                        QString tr_id = nameAttribs.namedItem("lang"_l1).toAttr().value();
                        QString tr_name = name.toElement().text();

                        if (!tr_name.isEmpty())
                            trans.languageName[tr_id.isEmpty() ? "en"_l1 : tr_id] = tr_name;
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
    setValue("Currency/Default"_l1, currencyCode);

    emit defaultCurrencyCodeChanged(currencyCode);
}

QString Config::defaultCurrencyCode() const
{
    return value("Currency/Default"_l1, "USD"_l1).toString();
}

#include "moc_config.cpp"
