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
#include <stdlib.h>
#include <time.h>


#include <QGlobalStatic>
#include <QStringList>
#include <QDir>
#include <QDomDocument>

#if defined( Q_WS_WIN )
#include <windows.h>
#include <tchar.h>
#include <shlobj.h>
#endif

#if defined ( Q_WS_MACX )
#include <Carbon/Carbon.h>
#endif

#include <QCryptographicHash>
#include "cconfig.h"

#define XSTR(a) #a
#define STR(a) XSTR(a)

namespace {

static inline qint32 mkver(int a, int b, int c)
{
    return (qint32(a) << 24) | (qint32(b) << 16) | (qint32(c));
}

} // namespace


CConfig::CConfig()
        : QSettings()
{
    m_show_input_errors = value("/General/ShowInputErrors", true).toBool();
    m_measurement = (value("/General/MeasurementSystem", "metric").toString() == "metric") ? QLocale::MetricSystem : QLocale::ImperialSystem;
    m_simple_mode = value("/General/SimpleMode", false).toBool();

    m_registration = OpenSource;
    m_translations_parsed = false;
    setRegistration(registrationName(), registrationKey());
}

CConfig::~CConfig()
{
    s_inst = 0;
}

CConfig *CConfig::s_inst = 0;

CConfig *CConfig::inst()
{
    if (!s_inst)
        s_inst = new CConfig();
    return s_inst;
}

QString CConfig::registrationName() const
{
    return value("/General/Registration/Name").toString();
}

QString CConfig::registrationKey() const
{
    return value("/General/Registration/Key").toString();
}

bool CConfig::checkRegistrationKey(const QString &name, const QString &key)
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

CConfig::Registration CConfig::registration() const
{
    return m_registration;
}

CConfig::Registration CConfig::setRegistration(const QString &name, const QString &key)
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

QString CConfig::scramble(const QString &str)
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

void CConfig::upgrade(int vmajor, int vminor, int vrev)
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

QDateTime CConfig::lastDatabaseUpdate() const
{
    QDateTime dt;

    time_t tt = value("/BrickLink/LastDBUpdate", 0).toInt();
    if (tt)
        dt.setTime_t (tt);
    return dt;
}

void CConfig::setLastDatabaseUpdate(const QDateTime &dt)
{
    time_t tt = dt.isValid() ? dt.toTime_t () : 0;
    setValue("/BrickLink/LastDBUpdate", int(tt));
}

bool CConfig::closeEmptyDocuments() const
{
    return value("/General/CloseEmptyDocs", false).toBool();
}

void CConfig::setCloseEmptyDocuments(bool b)
{
    setValue("/General/CloseEmptyDocs", b);
}

bool CConfig::showInputErrors() const
{
    return m_show_input_errors;
}

void CConfig::setShowInputErrors(bool b)
{
    if (b != m_show_input_errors) {
        m_show_input_errors = b;
        setValue("/General/ShowInputErrors", b);

        emit showInputErrorsChanged(b);
    }
}

void CConfig::setLDrawDir(const QString &dir)
{
    setValue("/General/LDrawDir", dir);
}

QString CConfig::lDrawDir() const
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

QString CConfig::documentDir() const
{
    QString dir = value("/General/DocDir").toString();

    if (dir.isEmpty()) {
        dir = QDir::homePath();

#if defined( Q_WS_WIN )
        QT_WA({
            WCHAR wpath [MAX_PATH];

            if (SHGetSpecialFolderPathW(NULL, wpath, CSIDL_PERSONAL, TRUE))
                dir = QString::fromUtf16(wpath);
        }, {
            char apath [MAX_PATH];

            if (SHGetSpecialFolderPathA(NULL, apath, CSIDL_PERSONAL, TRUE))
                dir = QString::fromLocal8Bit(apath);
        })

#elif defined( Q_WS_MACX )
        FSRef dref;

        if (FSFindFolder(kUserDomain, kDocumentsFolderType, kDontCreateFolder, &dref) == noErr) {
            UInt8 strbuffer [PATH_MAX];

            if (FSRefMakePath(&dref, strbuffer, sizeof(strbuffer)) == noErr)
                dir = QString::fromUtf8(reinterpret_cast <char *>(strbuffer));
        }

#endif
    }
    return dir;
}

void CConfig::setDocumentDir(const QString &dir)
{
    setValue("/General/DocDir", dir);
}


QNetworkProxy CConfig::proxy() const
{
    QNetworkProxy proxy;

    proxy.setType(value("/Internet/UseProxy", false).toBool() ? QNetworkProxy::HttpCachingProxy : QNetworkProxy::NoProxy);
    proxy.setHostName(value("/Internet/ProxyName").toString());
    proxy.setPort(value("/Internet/ProxyPort", 8080).toInt());

    return proxy;
}

void CConfig::setProxy(const QNetworkProxy &np)
{
    QNetworkProxy op = proxy();

    if ((op.type() != np.type()) || (op.hostName() != np.hostName()) || (op.port() != np.port())) {
        setValue("/Internet/UseProxy", (np.type() != QNetworkProxy::NoProxy));
        setValue("/Internet/ProxyName", np.hostName());
        setValue("/Internet/ProxyPort", np.port());

        emit proxyChanged(np);
    }
}


QString CConfig::dataDir() const
{
    return value("/BrickLink/DataDir").toString();
}

void CConfig::setDataDir(const QString &dir)
{
    setValue("/BrickLink/DataDir", dir);
}

QString CConfig::language() const
{
    return value("/General/Locale").toString();
}

void CConfig::setLanguage(const QString &lang)
{
    QString old = language();

    if (old != lang) {
        setValue("/General/Locale", lang);

        emit languageChanged();
    }
}

QLocale::MeasurementSystem CConfig::measurementSystem() const
{
    return m_measurement;
}

void CConfig::setMeasurementSystem(QLocale::MeasurementSystem ms)
{
    if (ms != m_measurement) {
        m_measurement= ms;
        setValue("/General/MeasurementSystem", ms == QLocale::MetricSystem ? "metric" : "imperial");

        emit measurementSystemChanged(ms);
    }
}


bool CConfig::simpleMode() const
{
    return m_simple_mode;
}

void CConfig::setSimpleMode(bool sm)
{
    if (sm != m_simple_mode) {
        m_simple_mode = sm;
        setValue("/General/SimpleMode", sm);

        emit simpleModeChanged(sm);
    }
}


QMap<QByteArray, int> CConfig::updateIntervals() const
{
    QMap<QByteArray, int> uiv = updateIntervalsDefault();
    
    const char *lut[] = { "Picture", "PriceGuide", "Database", "LDraw", 0 };

    for (const char **ptr = lut; *ptr; ++ptr)
        uiv[*ptr] = value(QLatin1String("/BrickLink/UpdateInterval/") + QLatin1String(*ptr), uiv[*ptr]).toInt();
    return uiv;
}

QMap<QByteArray, int> CConfig::updateIntervalsDefault() const
{
    const int day2sec = 60*60*24;
    QMap<QByteArray, int> uiv;

    uiv.insert("Picture",   180 * day2sec);
    uiv.insert("PriceGuide", 14 * day2sec);
    uiv.insert("Database",    7 * day2sec);
    uiv.insert("LDraw",      31 * day2sec);

    return uiv;
}

void CConfig::setUpdateIntervals(const QMap<QByteArray, int> &uiv)
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


QPair<QString, QString> CConfig::loginForBrickLink() const
{
    return qMakePair(value("/BrickLink/Login/Username").toString(), scramble(value("/BrickLink/Login/Password").toString()));
}

void CConfig::setLoginForBrickLink(const QString &name, const QString &pass)
{
    setValue("/BrickLink/Login/Username", name);
    setValue("/BrickLink/Login/Password", scramble(pass));
}


bool CConfig::onlineStatus() const
{
    return value("/Internet/Online", true).toBool();
}

void CConfig::setOnlineStatus(bool b)
{
    bool ob = onlineStatus();

    if (b != ob) {
        setValue("/Internet/Online", b);

        emit onlineStatusChanged(b);
    }
}

QList<CConfig::Translation> CConfig::translations() const
{
    if (!m_translations_parsed)
        m_translations_parsed = parseTranslations();
    return m_translations;
}

bool CConfig::parseTranslations() const
{
	m_translations.clear();

	QDomDocument doc;
	QFile file(":/translations/translations.xml");

	if (file.open(QIODevice::ReadOnly)) {
		QString err_str;
		int err_line = 0, err_col = 0;
	
		if (doc.setContent(&file, &err_str, &err_line, &err_col)) {
			QDomElement root = doc.documentElement();

			if (root.isElement() && root.nodeName() == "translations") {
				bool found_default = false;

				for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
					if (!node.isElement() || (node.nodeName() != "translation"))
						continue;
					QDomNamedNodeMap map = node.attributes();
					Translation trans;

					trans.m_langid = map.namedItem("lang").toAttr().value();
					trans.m_default = map.contains("default");

					if (trans.m_default) {
						if (found_default)
							goto error;
						found_default = true;
					}

					if (trans.m_langid.isEmpty())
						goto error;

					QString defname, trname;

					for (QDomNode name = node.firstChild(); !name.isNull(); name = name.nextSibling()) {
						if (!name.isElement() || (name.nodeName() != "name"))
							continue;
						QDomNamedNodeMap map = name.attributes();

						QString tr_id = map.namedItem("lang").toAttr().value();
                        QString tr_name = name.toElement().text();
                        
                        if (!tr_name.isEmpty())
                            trans.m_names[tr_id.isEmpty() ? trans.m_langid : tr_id] = tr_name;
					}

					if (trans.m_names.isEmpty())
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

