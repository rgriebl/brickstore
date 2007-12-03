/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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

#define XSTR(a)	#a
#define STR(a)	XSTR(a)

namespace {

static inline qint32 mkver ( int a, int b, int c )
{ 
	return ( qint32( a ) << 24 ) | ( qint32( b ) << 16 ) | ( qint32( c ));
}

} // namespace


CConfig::CConfig ( )
	: QSettings ( )
{
	m_show_input_errors = value ( "/General/ShowInputErrors", true ). toBool ( );
	m_weight_system = ( value ( "/General/WeightSystem", "metric" ). toString ( ) == "metric" ) ? WeightMetric : WeightImperial;
	m_simple_mode = value ( "/General/SimpleMode", false ). toBool ( );

	m_registration = OpenSource;
	setRegistration ( registrationName ( ), registrationKey ( ));
}

CConfig::~CConfig ( )
{
	s_inst = 0;
}

CConfig *CConfig::s_inst = 0;

CConfig *CConfig::inst ( )
{
	if ( !s_inst )
		s_inst = new CConfig ( );
	return s_inst;
}

QString CConfig::registrationName ( ) const
{
	return value ( "/General/Registration/Name" ). toString ( );
}

QString CConfig::registrationKey ( ) const
{
	return value ( "/General/Registration/Key" ). toString ( );
}

bool CConfig::checkRegistrationKey ( const QString &name, const QString &key )
{
#if !defined( BS_REGKEY )
	Q_UNUSED( name )
	Q_UNUSED( key )

	return true;

#else
	if ( name. isEmpty ( ) || key. isEmpty ( ))
		return false;

	QByteArray src;
	QDataStream ss ( &src, QIODevice::WriteOnly );
	ss. setByteOrder ( QDataStream::LittleEndian );
	ss << ( QString( STR( BS_REGKEY )) + name );

	QCryptographicHash sha1_hash ( QCryptographicHash::Sha1 );
	sha1_hash. addData ( src. data ( ) + 4, src. size ( ) - 4 );
	QByteArray sha1 = sha1_hash. result ( );

	if ( sha1. count ( ) < 8 )
		return false;
	
	QString result;
	quint64 serial = 0;
	QDataStream ds ( &sha1, QIODevice::ReadOnly );
	ds >> serial;
	
	// 32bit without 0/O and 5/S
	const char *mapping = "12346789ABCDEFGHIJKLMNPQRTUVWXYZ";
	
	// get 12*5 = 60 bits
	for ( int i = 12; i; i-- ) {
		result. append ( mapping [serial & 0x1f] );
		serial >>= 5;
	}
	result. insert ( 8, '-' );
	result. insert ( 4, '-' );
	
	return ( result == key );
#endif	
}

CConfig::Registration CConfig::registration ( ) const
{
	return m_registration;
}

CConfig::Registration CConfig::setRegistration ( const QString &name, const QString &key )
{
#if defined( BS_REGKEY )
	Registration old = m_registration;

	if ( name.isEmpty ( ) && key. isEmpty ( ))
		m_registration = None;
	else if ( name == "FREE" )
		m_registration = Personal;
	else if ( name == "DEMO" )
		m_registration = Demo;
	else
		m_registration = checkRegistrationKey ( name, key ) ? Full : Personal;

	setValue ( "/General/Registration/Name", name );
	setValue ( "/General/Registration/Key", key );

	if ( old != m_registration )
		emit registrationChanged ( m_registration );
#else
	Q_UNUSED( name )
	Q_UNUSED( key )
#endif
	return m_registration;
}

QString CConfig::obscure ( const QString &str )
{
#if defined( Q_WS_WIN )
	// win9x registries cannot store unicode values
	if ( QSysInfo::WindowsVersion & QSysInfo::WV_DOS_based )
		return str;
#endif

	QString result;
	const QChar *unicode = str. unicode ( );
	for ( int i = 0; i < str. length ( ); i++ )
		result += ( unicode [i]. unicode ( ) < 0x20 ) ? unicode [i] :
	QChar ( 0x1001F - unicode [i]. unicode ( ));
	return result;
}

QString CConfig::readPasswordEntry ( const QString &key ) const
{
	return obscure ( value ( key ). toString ( ));
}

void CConfig::writePasswordEntry ( const QString &key, const QString &password )
{
	setValue ( key, obscure ( password ));
}


void CConfig::upgrade ( int vmajor, int vminor, int vrev )
{
	QStringList sl;
	QString s;

	int cfgver = value ( "/General/ConfigVersion", 0 ). toInt ( );
	setValue ( "/General/ConfigVersion", mkver ( vmajor, vminor, vrev ));

	if ( cfgver < mkver ( 1, 0, 125 )) {
		// convert itemview column info (>= 1.0.25)
		beginGroup ( "/ItemView/List" );

		sl = value ( "/ColumnWidths" ). toStringList ( );
		if ( !sl. isEmpty ( ))  setValue ( "/ColumnWidths", sl. join ( "," ));

		sl = value ( "/ColumnWidthsHidden" ). toStringList ( );
		if ( !sl. isEmpty ( ))  setValue ( "/ColumnWidthsHidden", sl. join ( "," ));

		sl = value ( "/ColumnOrder" ). toStringList ( );
		if ( !sl. isEmpty ( ))  setValue ( "/ColumnOrder", sl. join ( "," ));

		if ( contains ( "/SortAscending" )) {
			setValue ( "/SortDirection", value ( "/SortAscending", true ). toBool ( ) ? "A" : "D" );
			remove ( "/SortAscending" );
		}

		endGroup ( );
	
		// fix a typo (>= 1.0.125)
		s = value ( "/Default/AddItems/Condition" ). toString ( );
		if ( !s. isEmpty ( ))  setValue ( "/Defaults/AddItems/Condition", s );
		remove ( "/Default/AddItems/Condition" );
	}
}

QDateTime CConfig::lastDatabaseUpdate ( ) const
{
	QDateTime dt;

	time_t tt = value ( "/BrickLink/LastDBUpdate", 0 ). toInt ( );
	if ( tt )
		dt. setTime_t ( tt );
	return dt;
}

void CConfig::setLastDatabaseUpdate ( const QDateTime &dt )
{
	time_t tt = dt. isValid ( ) ? dt. toTime_t ( ) : 0;
	setValue ( "/BrickLink/LastDBUpdate", int( tt ));
}

bool CConfig::closeEmptyDocuments ( ) const
{
	return value ( "/General/CloseEmptyDocs", false ). toBool ( );
}

void CConfig::setCloseEmptyDocuments ( bool b )
{
	setValue ( "/General/CloseEmptyDocs", b  );
}

bool CConfig::showInputErrors ( ) const
{
	return m_show_input_errors;
}

void CConfig::setShowInputErrors ( bool b )
{
	if ( b != m_show_input_errors ) {
		m_show_input_errors = b;
		setValue ( "/General/ShowInputErrors", b );

		emit showInputErrorsChanged ( b );
	}
}

void CConfig::setLDrawDir ( const QString &dir )
{
	setValue ( "/General/LDrawDir", dir );
}

QString CConfig::lDrawDir ( ) const
{
	QString dir = value ( "/General/LDrawDir" ). toString ( );

	if ( dir. isEmpty ( ))
		dir = ::getenv ( "LDRAWDIR" );

#if defined( Q_WS_WIN )
	if ( dir. isEmpty ( )) {
		QT_WA( {
			WCHAR inidir [MAX_PATH];

			DWORD l = GetPrivateProfileStringW ( L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini" );
			if ( l >= 0 ) {
				inidir [l] = 0;
				dir = QString::fromUtf16 ( inidir );
			}
		}, {
			char inidir [MAX_PATH];

			DWORD l = GetPrivateProfileStringA ( "LDraw", "BaseDirectory", "", inidir, MAX_PATH, "ldraw.ini" );
			if ( l >= 0 ) {
				inidir [l] = 0;
				dir = QString::fromLocal8Bit ( inidir );
			}
		})
	}

	if ( dir. isEmpty ( )) {
		HKEY skey, lkey;

		QT_WA( {
			if ( RegOpenKeyExW ( HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey ) == ERROR_SUCCESS ) {
				if ( RegOpenKeyExW ( skey, L"LDraw", 0, KEY_READ, &lkey ) == ERROR_SUCCESS ) {
					WCHAR regdir [MAX_PATH + 1];
					DWORD regdirsize = MAX_PATH * sizeof( WCHAR );

					if ( RegQueryValueExW ( lkey, L"InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize ) == ERROR_SUCCESS ) {
						regdir [regdirsize / sizeof( WCHAR )] = 0;
						dir = QString::fromUtf16 ( regdir );
					}
					RegCloseKey ( lkey );
				}
				RegCloseKey ( skey );
			}
		}, {
			if ( RegOpenKeyExA ( HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &skey ) == ERROR_SUCCESS ) {
				if ( RegOpenKeyExA ( skey, "LDraw", 0, KEY_READ, &lkey ) == ERROR_SUCCESS ) {
					char regdir [MAX_PATH + 1];
					DWORD regdirsize = MAX_PATH;

					if ( RegQueryValueExA ( lkey, "InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize ) == ERROR_SUCCESS ) {
						regdir [regdirsize] = 0;
						dir = QString::fromLocal8Bit ( regdir );
					}
					RegCloseKey ( lkey );
				}
				RegCloseKey ( skey );
			}
		})
	}
#endif
	return dir;
}

QString CConfig::documentDir ( ) const
{
	QString dir = value ( "/General/DocDir" ). toString ( );

	if ( dir. isEmpty ( )) {
		dir = QDir::homePath ( );

#if defined( Q_WS_WIN )	
		QT_WA( {
			WCHAR wpath [MAX_PATH];

			if ( SHGetSpecialFolderPathW ( NULL, wpath, CSIDL_PERSONAL, TRUE )) 
				dir = QString::fromUtf16 ( wpath );
		}, {
			char apath [MAX_PATH];
			
			if ( SHGetSpecialFolderPathA ( NULL, apath, CSIDL_PERSONAL, TRUE ))
				dir = QString::fromLocal8Bit ( apath );
		} )
		
#elif defined( Q_WS_MACX )
		FSRef dref;
		
		if ( FSFindFolder( kUserDomain, kDocumentsFolderType, kDontCreateFolder, &dref ) == noErr ) {
			UInt8 strbuffer [PATH_MAX];
		
			if ( FSRefMakePath ( &dref, strbuffer, sizeof( strbuffer )) == noErr )
				dir = QString::fromUtf8 ( reinterpret_cast <char *> ( strbuffer ));
 		}
		
#endif
	}
	return dir;
}

void CConfig::setDocumentDir ( const QString &dir )
{
	setValue ( "/General/DocDir", dir );
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


QString CConfig::dataDir ( ) const
{
	return value ( "/BrickLink/DataDir" ). toString ( );
}

void CConfig::setDataDir ( const QString &dir )
{
	setValue ( "/BrickLink/DataDir", dir );
}

QString CConfig::language ( ) const
{
	return value ( "/General/Locale" ). toString ( );
}

void CConfig::setLanguage ( const QString &lang )
{
	QString old = language ( );

	if ( old != lang ) {
		setValue ( "/General/Locale", lang );

		emit languageChanged ( );
	}
}

CConfig::WeightSystem CConfig::weightSystem ( ) const
{
	return m_weight_system;
}

void CConfig::setWeightSystem ( WeightSystem ws )
{
	if ( ws != m_weight_system ) {
		m_weight_system = ws;
		setValue ( "/General/WeightSystem", ws == WeightMetric ? "metric" : "imperial" );

		emit weightSystemChanged ( ws );
	}
}


bool CConfig::simpleMode ( ) const
{
	return m_simple_mode;
}

void CConfig::setSimpleMode ( bool sm  )
{
	if ( sm != m_simple_mode ) {
		m_simple_mode = sm;
		setValue ( "/General/SimpleMode", sm );

		emit simpleModeChanged ( sm );
	}
}


void CConfig::blUpdateIntervals ( int &pic, int &pg ) const
{
	int picd, pgd;
	
	blUpdateIntervalsDefaults ( picd, pgd );

	pic = CConfig::inst ( )-> value ( "/BrickLink/UpdateInterval/Pictures",    picd ). toInt ( );
	pg  = CConfig::inst ( )-> value ( "/BrickLink/UpdateInterval/PriceGuides", pgd  ). toInt ( );
}

void CConfig::blUpdateIntervalsDefaults ( int &picd, int &pgd ) const
{
	int day2sec = 60*60*24;

	picd = 180 * day2sec;
	pgd  =  14 * day2sec;
}

void CConfig::setBlUpdateIntervals ( int pic, int pg )
{
	int opic, opg;
	
	blUpdateIntervals ( opic, opg );
	
	if (( opic != pic ) || ( opg != pg )) {
		setValue ( "/BrickLink/UpdateInterval/Pictures",    pic );
		setValue ( "/BrickLink/UpdateInterval/PriceGuides", pg  );		

		emit blUpdateIntervalsChanged ( pic, pg );
	}
}


QString CConfig::blLoginUsername ( ) const
{
	return value ( "/BrickLink/Login/Username" ). toString ( );
}

void CConfig::setBlLoginUsername ( const QString &name )
{
	setValue ( "/BrickLink/Login/Username", name );
}


QString CConfig::blLoginPassword ( ) const
{
	return readPasswordEntry ( "/BrickLink/Login/Password" );
}

void CConfig::setBlLoginPassword ( const QString &pass )
{
	writePasswordEntry ( "/BrickLink/Login/Password", pass );
}


bool CConfig::onlineStatus ( ) const
{
	return value ( "/Internet/Online", true ). toBool ( );
}

void CConfig::setOnlineStatus ( bool b )
{
	bool ob = onlineStatus ( );
	
	if ( b != ob ) {
		setValue ( "/Internet/Online", b );
	
		emit onlineStatusChanged ( b );
	}
}

