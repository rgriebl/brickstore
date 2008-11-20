/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#if defined(Q_WS_WIN)
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
    m_measurement = (value("/General/MeasurementSystem", "metric").toString() == "metric") ? QLocale::MetricSystem : QLocale::ImperialSystem;
    m_simple_mode = value("/General/SimpleMode", false).toBool();

    m_registration = OpenSource;
    m_translations_parsed = false;
    setRegistration(registrationName(), registrationKey());

    if (isLocalCurrencySet()) {
        QPair<QString, QString> syms = localCurrencySymbols();
        Currency::setLocal(syms.first, syms.second, localCurrencyRate());
    }
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

QString Config::registrationName() const
{
    return value("/General/Registration/Name").toString();
}

QString Config::registrationKey() const
{
    return value("/General/Registration/Key").toString();
}

bool Config::checkRegistrationKey(const QString &name, const QString &key)
{
#if !defined( BS_REGKEY )
    Q_UNUSED(name)
    Q_UNUSED(key)

    return true;

#else
    if (name.isEmpty() || key.isEmpty())
        return false;

    QByteArray src;
    QDataStream ss(&src, QIODevice::WriteOnly);
    ss.setByteOrder(QDataStream::LittleEndian);
    ss << (QString(STR(BS_REGKEY)) + name);

    QCryptographicHash sha1_hash(QCryptographicHash::Sha1);
    sha1_hash.addData(src.data() + 4, src.size() - 4);
    QByteArray sha1 = sha1_hash.result();

    if (sha1.count() < 8)
        return false;

    QString result;
    quint64 serial = 0;
    QDataStream ds(&sha1, QIODevice::ReadOnly);
    ds >> serial;

    // 32bit without 0/O and 5/S
    const char *mapping = "12346789ABCDEFGHIJKLMNPQRTUVWXYZ";

    // get 12*5 = 60 bits
    for (int i = 12; i; i--) {
        result.append(mapping [serial & 0x1f]);
        serial >>= 5;
    }
    result.insert(8, '-');
    result.insert(4, '-');

    return (result == key);
#endif
}

Config::Registration Config::registration() const
{
    return m_registration;
}

Config::Registration Config::setRegistration(const QString &name, const QString &key)
{
#if defined( BS_REGKEY )
    Registration old = m_registration;

    if (name.isEmpty() && key.isEmpty())
        m_registration = None;
    else if (name == "FREE")
        m_registration = Personal;
    else if (name == "DEMO")
        m_registration = Demo;
    else
        m_registration = checkRegistrationKey(name, key) ? Full : Personal;

    setValue("/General/Registration/Name", name);
    setValue("/General/Registration/Key", key);

    if (old != m_registration)
        emit registrationChanged(m_registration);
#else
    Q_UNUSED(name)
    Q_UNUSED(key)
#endif
    return m_registration;
}

QString Config::scramble(const QString &str)
{
#if defined( Q_WS_WIN )
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
        if (!sl.isEmpty())  setValue("/ColumnWidths", sl.join(","));

        sl = value("/ColumnWidthsHidden").toStringList();
        if (!sl.isEmpty())  setValue("/ColumnWidthsHidden", sl.join(","));

        sl = value("/ColumnOrder").toStringList();
        if (!sl.isEmpty())  setValue("/ColumnOrder", sl.join(","));

        if (contains("/SortAscending")) {
            setValue("/SortDirection", value("/SortAscending", true).toBool() ? "A" : "D");
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
        dir = ::getenv("LDRAWDIR");

#if defined( Q_WS_WIN )
    if (dir.isEmpty()) {
        QT_WA({
            WCHAR inidir [MAX_PATH];

            DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
            if (l >= 0) {
                inidir [l] = 0;
                dir = QString::fromUtf16(inidir);
            }
        }, {
            char inidir [MAX_PATH];

            DWORD l = GetPrivateProfileStringA("LDraw", "BaseDirectory", "", inidir, MAX_PATH, "ldraw.ini");
            if (l >= 0) {
                inidir [l] = 0;
                dir = QString::fromLocal8Bit(inidir);
            }
        })
    }

    if (dir.isEmpty()) {
        HKEY skey, lkey;

        QT_WA({
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
                if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                    WCHAR regdir [MAX_PATH + 1];
                    DWORD regdirsize = MAX_PATH * sizeof(WCHAR);

                    if (RegQueryValueExW(lkey, L"InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                        regdir [regdirsize / sizeof(WCHAR)] = 0;
                        dir = QString::fromUtf16(regdir);
                    }
                    RegCloseKey(lkey);
                }
                RegCloseKey(skey);
            }
        }, {
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
                if (RegOpenKeyExA(skey, "LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                    char regdir [MAX_PATH + 1];
                    DWORD regdirsize = MAX_PATH;

                    if (RegQueryValueExA(lkey, "InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                        regdir [regdirsize] = 0;
                        dir = QString::fromLocal8Bit(regdir);
                    }
                    RegCloseKey(lkey);
                }
                RegCloseKey(skey);
            }
        })
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
#if QT_VERSION < 0x040500
    return value("/BrickLink/DataDir", QDesktopServices::storageLocation(QDesktopServices::DataLocation)).toString();
#else
    return value("/BrickLink/DataDir", QDesktopServices::storageLocation(QDesktopServices::CacheLocation)).toString();
#endif
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
        setValue("/General/MeasurementSystem", ms == QLocale::MetricSystem ? "metric" : "imperial");

        emit measurementSystemChanged(ms);
    }
}


bool Config::simpleMode() const
{
    return m_simple_mode;
}

void Config::setSimpleMode(bool sm)
{
    if (sm != m_simple_mode) {
        m_simple_mode = sm;
        setValue("/General/SimpleMode", sm);

        emit simpleModeChanged(sm);
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
	QFile file("translations/translations.xml");

	if (file.open(QIODevice::ReadOnly)) {
		QString err_str;
		int err_line = 0, err_col = 0;

		if (doc.setContent(&file, &err_str, &err_line, &err_col)) {
			QDomElement root = doc.documentElement();

			if (root.isElement() && root.nodeName() == "translations") {
				for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
					if (!node.isElement() || (node.nodeName() != "translation"))
						continue;
					QDomNamedNodeMap map = node.attributes();
					Translation trans;

					trans.language = map.namedItem("lang").toAttr().value();
                    trans.author = map.namedItem("author").toAttr().value();
                    trans.authorEMail = map.namedItem("authoremail").toAttr().value();

					if (trans.language.isEmpty())
						goto error;

					for (QDomNode name = node.firstChild(); !name.isNull(); name = name.nextSibling()) {
						if (!name.isElement() || (name.nodeName() != "name"))
							continue;
						QDomNamedNodeMap map = name.attributes();

						QString tr_id = map.namedItem("lang").toAttr().value();
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

void Config::setLocalCurrency(const QString &symint, const QString &sym, double rate)
{
    setValue("/General/Local/Active", true);
    setValue("/General/Local/IntSymbol", symint);
    setValue("/General/Local/Symbol", sym);
    setValue("/General/Local/Rate", rate);

    Currency::setLocal(symint, sym, rate);

    emit localCurrencyChanged();
}

void Config::unsetLocalCurrency()
{
    setValue("/General/Local/Active", false);
    Currency::setLocal(QLatin1String("USD"), QLatin1String("$"), 1.);

    emit localCurrencyChanged();
}

bool Config::isLocalCurrencySet() const
{
    return value("/General/Local/Active", false).toBool();
}

double Config::localCurrencyRate() const
{
    return value("/General/Local/Rate", 1.).toDouble();
}

QPair<QString, QString> Config::localCurrencySymbols() const
{
    return qMakePair(value("/General/Local/IntSymbol", QLatin1String("USD")).toString(),
                     value("/General/Local/Symbol", QLatin1String("$")).toString());
}
