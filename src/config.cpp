/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <QDesktopServices>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include <QCryptographicHash>
#include "config.h"
#include "currency.h"

#define XSTR(a) #a
#define STR(a) XSTR(a)

namespace {

static inline qint32 mkver(int a, int b, int c)
{
    return (qint32(a) << 24) | (qint32(b) << 16) | (qint32(c));
}

} // namespace


Config::Config()
        : QSettings()
{
    m_show_input_errors = value("/General/ShowInputErrors", true).toBool();
    m_measurement = (value("/General/MeasurementSystem").toString() == QLatin1String("imperial")) ? QLocale::ImperialSystem : QLocale::MetricSystem;
    m_translations_parsed = false;
}

Config::~Config()
{
    s_inst = 0;
}

Config *Config::s_inst = 0;

Config *Config::inst()
{
    if (!s_inst)
        s_inst = new Config();
    return s_inst;
}

QString Config::scramble(const QString &str)
{
#if defined(Q_OS_WIN)
    // win9x registries cannot store unicode values
    if (QSysInfo::WindowsVersion & QSysInfo::WV_DOS_based)
        return str;
#endif

    QString result;
    const QChar *unicode = str.unicode();
    for (int i = 0; i < str.length(); i++)
        result += (unicode [i].unicode() < 0x20) ? unicode [i] :
                  QChar(0x1001F - unicode [i].unicode());
    return result;
}

void Config::upgrade(int vmajor, int vminor, int vrev)
{
    QStringList sl;
    QString s;

    int cfgver = value("/General/ConfigVersion", 0).toInt();
    setValue("/General/ConfigVersion", mkver(vmajor, vminor, vrev));

    if (cfgver < mkver(1, 0, 125)) {
        // convert itemview column info (>= 1.0.25)
        beginGroup("/ItemView/List");

        sl = value("/ColumnWidths").toStringList();
        if (!sl.isEmpty())  setValue("/ColumnWidths", sl.join(QLatin1String(",")));

        sl = value("/ColumnWidthsHidden").toStringList();
        if (!sl.isEmpty())  setValue("/ColumnWidthsHidden", sl.join(QLatin1String(",")));

        sl = value("/ColumnOrder").toStringList();
        if (!sl.isEmpty())  setValue("/ColumnOrder", sl.join(QLatin1String(",")));

        if (contains("/SortAscending")) {
            setValue("/SortDirection", value("/SortAscending", true).toBool() ? QLatin1String("A") : QLatin1String("D"));
            remove("/SortAscending");
        }

        endGroup();

        // fix a typo (>= 1.0.125)
        s = value("/Default/AddItems/Condition").toString();
        if (!s.isEmpty())  setValue("/Defaults/AddItems/Condition", s);
        remove("/Default/AddItems/Condition");
    }
}

QDateTime Config::lastDatabaseUpdate() const
{
    QDateTime dt;

    time_t tt = value("/BrickLink/LastDBUpdate", 0).toInt();
    if (tt)
        dt.setTime_t (tt);
    return dt;
}

void Config::setLastDatabaseUpdate(const QDateTime &dt)
{
    time_t tt = dt.isValid() ? dt.toTime_t () : 0;
    setValue("/BrickLink/LastDBUpdate", int(tt));
}

bool Config::closeEmptyDocuments() const
{
    return value("/General/CloseEmptyDocs", false).toBool();
}

void Config::setCloseEmptyDocuments(bool b)
{
    setValue("/General/CloseEmptyDocs", b);
}

bool Config::showInputErrors() const
{
    return m_show_input_errors;
}

void Config::setShowInputErrors(bool b)
{
    if (b != m_show_input_errors) {
        m_show_input_errors = b;
        setValue("/General/ShowInputErrors", b);

        emit showInputErrorsChanged(b);
    }
}

void Config::setLDrawDir(const QString &dir)
{
    setValue("/General/LDrawDir", dir);
}

QString Config::lDrawDir() const
{
    QString dir = value("/General/LDrawDir").toString();

    if (dir.isEmpty())
        dir = QString::fromLocal8Bit(::getenv("LDRAWDIR"));

#if defined(Q_OS_WIN)
    if (dir.isEmpty()) {
        wchar_t inidir [MAX_PATH];

        DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
        if (l >= 0) {
            inidir [l] = 0;
            dir = QString::fromUtf16(reinterpret_cast<const ushort *>(inidir));
        }
    }

    if (dir.isEmpty()) {
        HKEY skey, lkey;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
            if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                wchar_t regdir [MAX_PATH + 1];
                DWORD regdirsize = MAX_PATH * sizeof(WCHAR);

                if (RegQueryValueExW(lkey, L"InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                    regdir [regdirsize / sizeof(WCHAR)] = 0;
                    dir = QString::fromUtf16(reinterpret_cast<const ushort *>(regdir));
                }
                RegCloseKey(lkey);
            }
            RegCloseKey(skey);
        }
    }
#endif
    return dir;
}

QString Config::documentDir() const
{
    QString dir = value("/General/DocDir", QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).toString();

    if (dir.isEmpty())
        dir = QDir::homePath();

    return dir;
}

void Config::setDocumentDir(const QString &dir)
{
    setValue("/General/DocDir", dir);
}


QNetworkProxy Config::proxy() const
{
    QNetworkProxy proxy;

    proxy.setType(value("/Internet/UseProxy", false).toBool() ? QNetworkProxy::HttpCachingProxy : QNetworkProxy::NoProxy);
    proxy.setHostName(value("/Internet/ProxyName").toString());
    proxy.setPort(value("/Internet/ProxyPort", 8080).toInt());

    return proxy;
}

void Config::setProxy(const QNetworkProxy &np)
{
    QNetworkProxy op = proxy();

    if ((op.type() != np.type()) || (op.hostName() != np.hostName()) || (op.port() != np.port())) {
        setValue("/Internet/UseProxy", (np.type() != QNetworkProxy::NoProxy));
        setValue("/Internet/ProxyName", np.hostName());
        setValue("/Internet/ProxyPort", np.port());

        emit proxyChanged(np);
    }
}


QString Config::dataDir() const
{
    return value("/BrickLink/DataDir", QDesktopServices::storageLocation(QDesktopServices::CacheLocation)).toString();
}

void Config::setDataDir(const QString &dir)
{
    setValue("/BrickLink/DataDir", dir);
}

QString Config::language() const
{
    return value("/General/Locale", QLocale::system().name()).toString();
}

void Config::setLanguage(const QString &lang)
{
    QString old = language();

    if (old != lang) {
        setValue("/General/Locale", lang);

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
        setValue("/General/MeasurementSystem", ms == QLocale::MetricSystem ? QLatin1String("metric") : QLatin1String("imperial"));

        emit measurementSystemChanged(ms);
    }
}


QMap<QByteArray, int> Config::updateIntervals() const
{
    QMap<QByteArray, int> uiv = updateIntervalsDefault();

    const char *lut[] = { "Picture", "PriceGuide", "Database", "LDraw", 0 };

    for (const char **ptr = lut; *ptr; ++ptr)
        uiv[*ptr] = value(QLatin1String("/BrickLink/UpdateInterval/") + QLatin1String(*ptr), uiv[*ptr]).toInt();
    return uiv;
}

QMap<QByteArray, int> Config::updateIntervalsDefault() const
{
    const int day2sec = 60*60*24;
    QMap<QByteArray, int> uiv;

    uiv.insert("Picture",   180 * day2sec);
    uiv.insert("PriceGuide", 14 * day2sec);
    uiv.insert("Database",    7 * day2sec);
    uiv.insert("LDraw",      31 * day2sec);

    return uiv;
}

void Config::setUpdateIntervals(const QMap<QByteArray, int> &uiv)
{
    bool modified = false;
    QMap<QByteArray, int> old_uiv = updateIntervals();

    for (QMapIterator<QByteArray, int> it(uiv); it.hasNext(); ) {
        it.next();

        if (it.value() != old_uiv.value(it.key())) {
            setValue(QLatin1String("/BrickLink/UpdateInterval/") + it.key(), it.value());
            modified = true;
        }
    }

    if (modified)
        emit updateIntervalsChanged(updateIntervals());
}


QPair<QString, QString> Config::loginForBrickLink() const
{
    return qMakePair(value("/BrickLink/Login/Username").toString(), scramble(value("/BrickLink/Login/Password").toString()));
}

void Config::setLoginForBrickLink(const QString &name, const QString &pass)
{
    setValue("/BrickLink/Login/Username", name);
    setValue("/BrickLink/Login/Password", scramble(pass));
}


bool Config::onlineStatus() const
{
    return value("/Internet/Online", true).toBool();
}

void Config::setOnlineStatus(bool b)
{
    bool ob = onlineStatus();

    if (b != ob) {
        setValue("/Internet/Online", b);

        emit onlineStatusChanged(b);
    }
}

QList<Config::Translation> Config::translations() const
{
    if (!m_translations_parsed)
        m_translations_parsed = parseTranslations();
    return m_translations;
}

bool Config::parseTranslations() const
{
    m_translations.clear();

    QDomDocument doc;
    QFile file(QLatin1String("translations/translations.xml"));

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
                        QDomNamedNodeMap map = name.attributes();

                        QString tr_id = map.namedItem(QLatin1String("lang")).toAttr().value();
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
    setValue("/Currency/Default", currencyCode);

    emit defaultCurrencyCodeChanged(currencyCode);
}

QString Config::defaultCurrencyCode() const
{
    return value("/Currency/Default", QLatin1String("USD")).toString();
}
