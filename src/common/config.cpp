// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdlib>
#include <ctime>
#include <array>

#include <QGlobalStatic>
#include <QStringList>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSize>
#include <QUuid>
#include <QDebug>
#include <QKeySequence>
#include <QFileInfo>
#include <QMetaType>

#include "config.h"


namespace {

static inline qint32 mkver(int a, int b, int c)
{
    return (qint32(a) << 16) | (qint32(b) << 8) | (qint32(c));
}

} // namespace

#if defined(Q_OS_MACOS)
static const char *organization = "brickstore.org"; // this is wrong, but it can't be fixed easily
#elif defined(Q_OS_IOS)
static const char *organization = "brickforge.de";
#else
static const char *organization = "BrickStore";
#endif
static const char *application = "BrickStore";


Config::Config()
    : QSettings(QString::fromLatin1(organization), QString::fromLatin1(application))
{
    m_show_input_errors = value(u"General/ShowInputErrors"_qs, true).toBool();
    m_show_difference_indicators = value(u"General/ShowDifferenceIndicators"_qs, false).toBool();
    m_columnSpacing = std::clamp(value(u"Interface/ColumnSpacing"_qs, 1).toInt(), 0, 2);
    m_liveEditRowHeight = value(u"Interface/LiveEditRowHeight"_qs, true).toBool();
    m_rowHeightPercent = std::clamp(value(u"Interface/ItemImageSizePercent"_qs, 100).toInt(), 50, 200);

    m_measurement = (value(u"General/MeasurementSystem"_qs).toString() == u"imperial")
            ? QLocale::ImperialSystem : QLocale::MetricSystem;
    m_translations_parsed = false;

    m_bricklinkUsername = value(u"BrickLink/Login/Username"_qs).toString();
    m_bricklinkPassword = scramble(value(u"BrickLink/Login/Password"_qs).toString());

    qRegisterMetaType<QSet<uint>>();
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

void Config::upgrade(int vmajor, int vminor, int vpatch)
{
    int cfgver = value(u"General/ConfigVersion"_qs, 0).toInt();
    setValue(u"General/ConfigVersion"_qs, mkver(vmajor, vminor, vpatch));

#if defined(BS_DESKTOP)
    // import old settings from BrickStock 1.2.x
    if (!value(u"General/ImportedV12xSettings"_qs, 0).toInt()) {
        static const std::vector<const char *> ignore = {
            "MainWindow/",
            "General/Registration/",
            "General/ConfigVersion",
            "General/Money/",
            "General/lastApplicationUpdateCheck",
            "Internet/UseProxy",
            "Internet/Proxy"
        };

        QSettings old(u"patrickbrans.com"_qs, u"BrickStock"_qs);
        if (old.value(u"General/ConfigVersion"_qs, 0).toInt()) {
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
            if (old.value(u"General/Money/Localized"_qs, false).toBool())
                setValue(u"Rates/Legacy"_qs, old.value(u"General/Money/Factor"_qs, 1).toDouble());
        }
        setValue(u"General/ImportedV12xSettings"_qs, 1);
    }
#endif

    // do config upgrades as needed
    if (!cfgver)
        return;

    if (cfgver < mkver(2021, 10, 1)) {
        // update IconSize to IconSizeEnum
        int oldSize = value(u"Interface/IconSize"_qs, 0).toInt();
        if (oldSize)
            oldSize = std::clamp((oldSize - 12) / 10, 0, 2); // 22->1, 32->2
        setValue(u"Interface/IconSizeEnum"_qs, oldSize);
    }
    if (cfgver < mkver(2022, 2, 3)) {
#if defined(BS_DESKTOP)
        // transition to new ldrawDir settings
        setValue(u"General/LDrawTransition"_qs, true);
#endif
        // reset the sentry consent flag on platforms that didn't have sentry builds before
#if !defined(Q_OS_WINDOWS)
        setSentryConsent(SentryConsent::Unknown);
#endif
    }
    if (cfgver < mkver(2022, 6, 3)) {
        auto vl = value(u"Announcements/ReadIds"_qs).toList();
        for (QVariant &v : vl) {
            if (v.typeId() == QMetaType::ULongLong) {
                quint64 q = v.toULongLong();
                // 32b days | 32b crc16 -> 16b days | 16b crc16
                q = ((q >> 16) & ~0xffffULL) | (q & 0xffffULL);
                v.setValue<quint32>(quint32(q));
            }
        }
        setValue(u"Announcements/ReadIds"_qs, vl);
    }
    if (cfgver < mkver(2022, 12, 1)) {
#if defined(BS_DESKTOP)
        const auto recentFiles = value(u"Files/Recent"_qs).toStringList();
        if (recentFiles.size() > 0) {
            beginWriteArray(u"RecentFiles"_qs);
            for (int i = 0; i < recentFiles.size(); ++i) {
                QFileInfo fi(recentFiles.at(i));
                setArrayIndex(i);
                setValue(u"Path"_qs, fi.absoluteFilePath());
                setValue(u"Name"_qs, fi.fileName());
            }
            endArray();
        }
#endif
        remove(u"Files"_qs);
    }
}

QVariantList Config::availableLanguages() const
{
    const auto trs = translations();
    QVariantList al;
    al.reserve(trs.size());
    for (const Translation &tr : trs) {
        al.append(QVariantMap {
                      { u"language"_qs, tr.language },
                      { u"name"_qs, tr.name },
                      { u"localName"_qs, tr.localName },
                      { u"flagPath"_qs, QString(u"qrc"_qs + tr.flagPath) },
                      { u"author"_qs, tr.author },
                      { u"authorEMail"_qs, tr.authorEmail },
                  });
    }
    return al;
}

bool Config::showInputErrors() const
{
    return m_show_input_errors;
}

void Config::setShowInputErrors(bool b)
{
    if (b != m_show_input_errors) {
        m_show_input_errors = b;
        setValue(u"General/ShowInputErrors"_qs, b);

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
        setValue(u"General/ShowDifferenceIndicators"_qs, b);

        emit showDifferenceIndicatorsChanged(b);
    }
}

void Config::setLDrawDir(const QString &dir)
{
    if (ldrawDir() != dir) {
        setValue(u"General/LDrawDir"_qs, dir);
        emit ldrawDirChanged(dir);
    }
}

QString Config::ldrawDir() const
{
    return value(u"General/LDrawDir"_qs).toString();
}

QString Config::documentDir() const
{
    QString dir = value(u"General/DocDir"_qs, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    if (dir.isEmpty())
        dir = QDir::homePath();

    return dir;
}

void Config::setDocumentDir(const QString &dir)
{
    if (documentDir() != dir) {
        setValue(u"General/DocDir"_qs, dir);
        emit documentDirChanged(dir);
    }
}


QString Config::cacheDir() const
{
    static QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return value(u"BrickLink/CacheDir"_qs, cacheDir).toString();
}

QString Config::language() const
{
    return value(u"General/Locale"_qs, QLocale::system().name().left(2)).toString();
}

void Config::setLanguage(const QString &lang)
{
    if (language() != lang) {
        setValue(u"General/Locale"_qs, lang);
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
        setValue(u"General/MeasurementSystem"_qs, (ms == QLocale::MetricSystem) ? u"metric"_qs
                                                                               : u"imperial"_qs);
        emit measurementSystemChanged(ms);
    }
}

QMap<QByteArray, int> Config::updateIntervals() const
{
    QMap<QByteArray, int> uiv = updateIntervalsDefault();

    static const std::array lut = { "Picture", "PriceGuide", "Database" };

    for (const auto &iv : lut)
        uiv[iv] = value(u"BrickLink/UpdateInterval/"_qs + QLatin1String(iv), uiv[iv]).toInt();
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
            setValue(u"BrickLink/UpdateInterval/"_qs + QLatin1String(it.key()), it.value());
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
        setValue(u"Interface/FontSizePercent"_qs, std::clamp(p, 50, 200));
        emit fontSizePercentChanged(p);
    }
}

void Config::setRowHeightPercent(int p)
{
    p = std::clamp(p, 50, 200);

    if (m_rowHeightPercent != p) {
        m_rowHeightPercent = p;
        setValue(u"Interface/ItemImageSizePercent"_qs, p);
        emit rowHeightPercentChanged(p);
    }
}

QByteArray Config::columnLayout(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value(u"ColumnLayouts/"_qs + id + u"/Layout").toByteArray();
}

QString Config::columnLayoutName(const QString &id) const
{
    if (id.isEmpty())
        return { };
    return value(u"ColumnLayouts/"_qs + id + u"/Name").toString();
}

int Config::columnLayoutOrder(const QString &id) const
{
    if (id.isEmpty())
        return -1;
    return value(u"ColumnLayouts/"_qs + id + u"/Order", -1).toInt();
}

QStringList Config::columnLayoutIds() const
{
    const_cast<Config *>(this)->beginGroup(u"ColumnLayouts"_qs);
    auto sl = childGroups();
    sl.removeOne(u"user-default"_qs);
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
                beginGroup(u"ColumnLayouts"_qs);
                bool hasLayout = childGroups().contains(id);
                endGroup();
                if (!hasLayout)
                    return { };
            }
        }
        setValue(u"ColumnLayouts/"_qs + nid + u"/Layout", layout);

        if (isNew) {
            auto newIds = columnLayoutIds();
            setValue(u"ColumnLayouts/"_qs + nid + u"/Order", newIds.count() - 1);
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
        beginGroup(u"ColumnLayouts"_qs);
        bool hasLayout = childGroups().contains(id);
        endGroup();
        if (hasLayout) {
            remove(u"ColumnLayouts/"_qs + id);
            emit columnLayoutIdsChanged(columnLayoutIds());
            return true;
        }
    }
    return false;
}

bool Config::renameColumnLayout(const QString &id, const QString &name)
{
    if (!id.isEmpty()) {
        beginGroup(u"ColumnLayouts"_qs);
        bool hasLayout = childGroups().contains(id);
        endGroup();

        if (hasLayout) {
            QString oldname = value(u"ColumnLayouts/"_qs + id + u"/Name").toString();
            if (oldname != name) {
                setValue(u"ColumnLayouts/"_qs + id + u"/Name", name);
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
            setValue(u"ColumnLayouts/"_qs + ids.at(i) + u"/Order", i);
        emit columnLayoutIdsOrderChanged(ids);
        return true;
    }
    return false;
}

QVariantMap Config::shortcuts() const
{
    return value(u"General/Shortcuts"_qs).toMap();
}

void Config::setShortcuts(const QVariantMap &list)
{
    if (shortcuts() != list) {
        setValue(u"General/Shortcuts"_qs, list);
        emit shortcutsChanged(list);
    }
}

QStringList Config::toolBarActions() const
{
    auto s = value(u"General/ToolBarActions"_qs).toString();
    return s.isEmpty() ? QStringList { } : s.split(u","_qs);
}

void Config::setToolBarActions(const QStringList &actions)
{
    if (toolBarActions() != actions) {
        setValue(u"General/ToolBarActions"_qs, actions.join(u','));
        emit toolBarActionsChanged(actions);
    }
}

QString Config::lastDirectory() const
{
    return m_lastDirectory.isEmpty() ? documentDir() : m_lastDirectory;
}

void Config::setLastDirectory(const QString &dir)
{
    if (!dir.isEmpty())
        m_lastDirectory = dir;
}

Config::SentryConsent Config::sentryConsent() const
{
    int v = value(u"General/SentryConsent"_qs).toInt();
    if ((v < int(SentryConsent::Unknown)) || (v > int(SentryConsent::Revoked)))
         v = int(SentryConsent::Unknown);
    return SentryConsent(v);
}

void Config::setSentryConsent(SentryConsent consent)
{
    if (sentryConsent() != consent) {
        setValue(u"General/SentryConsent"_qs, int(consent));
        emit sentryConsentChanged(consent);
    }
}

Config::UITheme Config::uiTheme() const
{
    int v = value(u"Interface/Theme"_qs).toInt();
    if ((v < int(UITheme::SystemDefault)) || (v > int(UITheme::Dark)))
         v = int(UITheme::SystemDefault);
    return UITheme(v);
}

void Config::setUITheme(UITheme theme)
{
    if (uiTheme() != theme) {
        setValue(u"Interface/Theme"_qs, int(theme));
        emit uiThemeChanged(theme);
    }
}

Config::UISize Config::mobileUISize() const
{
    int s = value(u"Interface/MobileUISize"_qs, 0).toInt();
    return static_cast<UISize>(std::clamp(s, 0, 2));
}

void Config::setMobileUISize(UISize size)
{
    if (mobileUISize() != size) {
        setValue(u"Interface/MobileUISize"_qs, int(size));
        emit mobileUISizeChanged(size);
    }
}

QSet<uint> Config::pinnedColorIds() const
{
    return value(u"Pinning/ColorIds"_qs).value<QSet<uint>>();
}

void Config::setPinnedColorIds(const QSet<uint> &colors)
{
    if (colors != pinnedColorIds()) {
        setValue(u"Pinning/ColorIds"_qs, QVariant::fromValue(colors));
        emit pinnedColorIdsChanged();
    }
}

QSet<uint> Config::pinnedCategoryIds() const
{
    return value(u"Pinning/CategoryIds"_qs).value<QSet<uint>>();
}

void Config::setPinnedCategoryIds(const QSet<uint> &categories)
{
    if (categories != pinnedCategoryIds()) {
        setValue(u"Pinning/CategoryIds"_qs, QVariant::fromValue(categories));
        emit pinnedCategoryIdsChanged();
    }
}

QString Config::brickLinkUsername() const
{
    return m_bricklinkUsername;
}

QString Config::brickLinkPassword() const
{
    return m_bricklinkPassword;
}

void Config::setBrickLinkUsername(const QString &user)
{
    if (m_bricklinkUsername != user) {
        m_bricklinkUsername = user;
        setValue(u"BrickLink/Login/Username"_qs, user);
        emit brickLinkCredentialsChanged();
    }
}

void Config::setBrickLinkPassword(const QString &pass, bool doNotSave)
{
    if (m_bricklinkPassword != pass) {
        m_bricklinkPassword = pass;
        setValue(u"BrickLink/Login/Password"_qs, doNotSave ? QString { } : scramble(pass));
        emit brickLinkCredentialsChanged();
    }
}

bool Config::onlineStatus() const
{
    return value(u"Internet/Online"_qs, true).toBool();
}

void Config::setOnlineStatus(bool b)
{
    bool ob = onlineStatus();

    if (b != ob) {
        setValue(u"Internet/Online"_qs, b);

        emit onlineStatusChanged(b);
    }
}

Config::PartOutMode Config::partOutMode() const
{
    int v = value(u"General/PartOutMode"_qs).toInt();
    if ((v < int(PartOutMode::Ask)) || (v > int(PartOutMode::NewDocument)))
         v = int(PartOutMode::Ask);
    return PartOutMode(v);
}

void Config::setPartOutMode(Config::PartOutMode pom)
{
    if (partOutMode() != pom) {
        setValue(u"General/PartOutMode"_qs, int(pom));
        emit partOutModeChanged(pom);
    }
}

bool Config::visualChangesMarkModified() const
{
    return value(u"General/VisualChangesMarkModified"_qs, false).toBool();
}

void Config::setVisualChangesMarkModified(bool b)
{
    if (visualChangesMarkModified() != b) {
        setValue(u"General/VisualChangesMarkModified"_qs, b);
        emit visualChangesMarkModifiedChanged(b);
    }
}

bool Config::restoreLastSession() const
{
    return value(u"General/RestoreLastSession"_qs, true).toBool();
}

void Config::setRestoreLastSession(bool b)
{
    if (restoreLastSession() != b) {
        setValue(u"General/RestoreLastSession"_qs, b);
        emit restoreLastSessionChanged(b);
    }
}

bool Config::openBrowserOnExport() const
{
    return value(u"General/Export/OpenBrowser"_qs, true).toBool();
}

void Config::setOpenBrowserOnExport(bool b)
{
    if (openBrowserOnExport() != b) {
        setValue(u"General/Export/OpenBrowser"_qs, b);
        emit openBrowserOnExportChanged(b);
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
    return value(u"Interface/FontSizePercent"_qs, 100).toInt();
}

int Config::rowHeightPercent() const
{
    return m_rowHeightPercent;
}

Config::UISize Config::iconSize() const
{
    int s = value(u"Interface/IconSizeEnum"_qs, 0).toInt();
    return static_cast<UISize>(std::clamp(s, 0, 2));
}

void Config::setIconSize(UISize iconSize)
{
    if (this->iconSize() != iconSize) {
        setValue(u"Interface/IconSizeEnum"_qs, int(iconSize));
        emit iconSizeChanged(iconSize);
    }
}

int Config::columnSpacing() const
{
    return m_columnSpacing;
}

void Config::setColumnSpacing(int newColumnSpacing)
{
    newColumnSpacing = std::clamp(newColumnSpacing, 0, 2);

    if (m_columnSpacing != newColumnSpacing) {
        m_columnSpacing = newColumnSpacing;
        setValue(u"Interface/ColumnSpacing"_qs, newColumnSpacing);
        emit columnSpacingChanged(newColumnSpacing);
    }
}

bool Config::liveEditRowHeight() const
{
    return m_liveEditRowHeight;
}

void Config::setLiveEditRowHeight(bool liveEdit)
{
    if (m_liveEditRowHeight != liveEdit) {
        m_liveEditRowHeight = liveEdit;
        setValue(u"Interface/LiveEditRowHeight"_qs, liveEdit);
        emit liveEditRowHeightChanged(liveEdit);
    }
}

bool Config::parseTranslations() const
{
    m_translations.clear();

    QFile file(u":/translations/translations.json"_qs);

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull()) {
        qWarning() << "Failed to parse translations.json at offset" << parseError.offset << ":"
                   << parseError.errorString();
        return false;
    }

    const auto root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject value = it.value().toObject();

        auto lang = QLocale::codeToLanguage(it.key());
        Q_ASSERT(lang != QLocale::AnyLanguage);

        Translation trans;
        trans.language = it.key();
        trans.name = QLocale::languageToString(lang);
        trans.localName = value[u"localName"].toString(); // QLocale::nativeLanguageName is weird

        if (!trans.localName.isEmpty() && (trans.name != trans.localName))
            trans.localName = trans.localName + u" (" + trans.name + u')';
        else
            trans.localName = trans.name;

        trans.author = value[u"author"].toString();
        trans.authorEmail = value[u"authorEmail"].toString();
        trans.flagPath = u":/assets/flags/" + QLocale::territoryToCode(QLocale(lang).territory()).toUpper();

        if (trans.language.isEmpty())
            continue;

        m_translations << trans;
    }
    std::sort(m_translations.begin(), m_translations.end(), [](const auto &tr1, const auto &tr2) {
        if (tr1.language == u"en")
            return true;
        else if (tr2.language == u"en")
            return false;
        else
            return tr1.localName.localeAwareCompare(tr2.localName) < 0;
    });

    return true;
}

void Config::setDefaultCurrencyCode(const QString &currencyCode)
{
    setValue(u"Currency/Default"_qs, currencyCode);

    emit defaultCurrencyCodeChanged(currencyCode);
}

QString Config::defaultCurrencyCode() const
{
    return value(u"Currency/Default"_qs, u"USD"_qs).toString();
}

#include "moc_config.cpp"
