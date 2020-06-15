/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qglobal.h>
#include <qdir.h>
#include <qdatetime.h>

#if defined( Q_WS_WIN )
#include <windows.h>
#include <tchar.h>
#include <shlobj.h>

#include <qapplication.h>
#endif

#if defined ( Q_WS_MACX )
#include <Carbon/Carbon.h>
#endif

#include "sha1.h"
#include "cconfig.h"


namespace {

static inline Q_INT32 mkver ( int a, int b, int c )
{ 
	return ( Q_INT32( a ) << 24 ) | ( Q_INT32( b ) << 16 ) | ( Q_INT32( c ));
}

} // namespace


CConfig::CConfig ( )
    : QSettings ( )
{
    setPath ( "patrickbrans.com", "BrickStock", QSettings::UserScope );

	m_show_input_errors = readBoolEntry ( "/General/ShowInputErrors", true );
	m_weight_system = ( readEntry ( "/General/WeightSystem", "metric" ) == "metric" ) ? WeightMetric : WeightImperial;
	m_simple_mode = readBoolEntry ( "/General/SimpleMode", false );

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
	if ( !s_inst ) {
#if defined( Q_OS_UNIX ) && !defined( Q_OS_MAC )
		// config dirs have to exist, otherwise Qt falls back to the default location (~/.qt/...)
		QDir d = QDir::homeDirPath ( );
        if ( !d. cd ( ".patrickbrans" )) {
			d = QDir::homeDirPath ( );
            if ( !d. mkdir ( ".patrickbrans" ))
                qWarning ( "Could not create config directory: ~/.patrickbrans" );
		}
#endif
		s_inst = new CConfig ( );
	}
	return s_inst;
}

QString CConfig::registrationName ( )
{
    return readEntry ( "/General/Registration/Name" );
}

QString CConfig::registrationKey ( )
{
    return readEntry ( "/General/Registration/Key" );
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
    ss << ( QString( BS_REGKEY ) + name );

	QByteArray sha1 = sha1::calc ( src. data ( ) + 4, src. size ( ) - 4 );

	if ( sha1. count ( ) < 8 )
		return false;
	
	QString result;
	Q_UINT64 serial = 0;
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
	
    if ( name == "BrickStockTest" && QDateTime::currentDateTime().secsTo(QDateTime(QDate(2014, 02, 15), QTime(0, 0))) < 0)
        return false;

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

    writeEntry ( "/General/Registration/Name", name );
    writeEntry ( "/General/Registration/Key", key );

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
	if ( QApplication::winVersion ( ) & QSysInfo::WV_DOS_based )
		return str;
#endif

	QString result;
	const QChar *unicode = str. unicode ( );
    for ( int i = 0; i < str. length ( ); i++ )
		result += ( unicode [i]. unicode ( ) < 0x20 ) ? unicode [i] :
	QChar ( 0x1001F - unicode [i]. unicode ( ));
	return result;
}

QString CConfig::readPasswordEntry ( const QString &key )
{
	return obscure ( readEntry ( key, QString ( "" )));
}

bool CConfig::writePasswordEntry ( const QString &key, const QString &password )
{
	return writeEntry ( key, obscure ( password ));
}


void CConfig::upgrade ( int vmajor, int vminor, int vrev )
{
	QStringList sl;
	QString s;
	int i;
	bool b;
	bool ok;

	int cfgver = readNumEntry ( "/General/ConfigVersion", 0 );
	writeEntry ( "/General/ConfigVersion", mkver ( vmajor, vminor, vrev ));

	if ( cfgver < mkver ( 1, 0, 125 )) {
		// convert itemview column info (>= 1.0.25)
		beginGroup ( "/ItemView/List" );

		sl = readListEntry ( "/ColumnWidths", &ok );
		if ( ok )  writeEntry ( "/ColumnWidths", sl. join ( "," ));

		sl = readListEntry ( "/ColumnWidthsHidden", &ok );
		if ( ok )  writeEntry ( "/ColumnWidthsHidden", sl. join ( "," ));

		sl = readListEntry ( "/ColumnOrder", &ok );
		if ( ok )  writeEntry ( "/ColumnOrder", sl. join ( "," ));

		i = readNumEntry ( "/SortColumn", -1, &ok );
		if ( ok )  writeEntry ( "/SortColumn", QString::number ( i ));

		b = readBoolEntry ( "/SortAscending", true, &ok );
		if ( ok )  writeEntry ( "/SortDirection", b ? "A" : "D" );
		removeEntry ( "/SortAscending" );

		endGroup ( );
	
		// fix a typo (>= 1.0.125)
		s = readEntry ( "/Default/AddItems/Condition", QString::null, &ok );
		if ( ok )  writeEntry ( "/Defaults/AddItems/Condition", s );
		removeEntry ( "/Default/AddItems/Condition" );
	}
}

QDateTime CConfig::lastDatabaseUpdate ( )
{
	QDateTime dt;

	time_t tt = readNumEntry ( "/BrickLink/LastDBUpdate", 0 );
	if ( tt )
		dt. setTime_t ( tt );
	return dt;
}

void CConfig::setLastDatabaseUpdate ( QDateTime dt )
{
	time_t tt = dt. isValid ( ) ? dt. toTime_t ( ) : 0;
	writeEntry ( "/BrickLink/LastDBUpdate", int( tt ));
}

QDateTime CConfig::lastApplicationUpdateCheck ( )
{
    QDateTime dt;

    time_t tt = readNumEntry ( "/General/lastApplicationUpdateCheck", 0 );
    if ( tt )
        dt. setTime_t ( tt );
    return dt;
}

void CConfig::setLastApplicationUpdateCheck ( QDateTime dt )
{
    time_t tt = dt. isValid ( ) ? dt. toTime_t ( ) : 0;
    writeEntry ( "/General/lastApplicationUpdateCheck", int( tt ));
}

bool CConfig::closeEmptyDocuments ( )
{
	return readBoolEntry ( "/General/CloseEmptyDocs", false );
}

void CConfig::setCloseEmptyDocuments ( bool b )
{
	writeEntry ( "/General/CloseEmptyDocs", b  );
}

bool CConfig::showInputErrors ( ) const
{
	return m_show_input_errors;
}

void CConfig::setShowInputErrors ( bool b )
{
	if ( b != m_show_input_errors ) {
		m_show_input_errors = b;
		writeEntry ( "/General/ShowInputErrors", b );

		emit showInputErrorsChanged ( b );
	}
}

void CConfig::setLDrawDir ( const QString &dir )
{
	writeEntry ( "/General/LDrawDir", dir );
}

QString CConfig::lDrawDir ( )
{
	QString dir = readEntry ( "/General/LDrawDir" );

	if ( dir. isEmpty ( ))
		dir = ::getenv ( "LDRAWDIR" );

#if defined( Q_WS_WIN )
	if ( dir. isEmpty ( )) {
		QT_WA( {
			WCHAR inidir [MAX_PATH];

			DWORD l = GetPrivateProfileStringW ( L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini" );
			if ( l >= 0 ) {
				inidir [l] = 0;
				dir = QString::fromUcs2 ((ushort *) inidir );
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
						dir = QString::fromUcs2 ((ushort *) regdir );
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

QString CConfig::documentDir ( )
{
	QString dir = readEntry ( "/General/DocDir" );

	if ( dir. isEmpty ( )) {
		dir = QDir::homeDirPath ( );

#if defined( Q_WS_WIN )	
		QT_WA( {
			WCHAR wpath [MAX_PATH];

			if ( SHGetSpecialFolderPathW ( NULL, wpath, CSIDL_PERSONAL, TRUE )) 
				dir = QString::fromUcs2 ((ushort *) wpath );
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
	writeEntry ( "/General/DocDir", dir );
}


bool CConfig::useProxy ( )
{
	return readBoolEntry ( "/Internet/UseProxy", false );
}

QString CConfig::proxyName ( )
{
	return readEntry ( "/Internet/ProxyName" );
}

int CConfig::proxyPort ( )
{
	return readNumEntry ( "/Internet/ProxyPort", 8080 );
}

void CConfig::setProxy ( bool b, const QString &name, int port )
{
	bool ob = useProxy ( );
	QString oname = proxyName ( );
	int oport = proxyPort ( );
	
	if (( b != ob ) || ( oname != name ) || ( oport != port )) {
		writeEntry ( "/Internet/UseProxy", b );
		writeEntry ( "/Internet/ProxyName", name );
		writeEntry ( "/Internet/ProxyPort", port );
		
		emit proxyChanged ( b, name, port );
	}
}


QString CConfig::dataDir ( )
{
	return readEntry ( "/BrickLink/DataDir" );
}

void CConfig::setDataDir ( const QString &dir )
{
	writeEntry ( "/BrickLink/DataDir", dir );
}

QString CConfig::language ( )
{
	return readEntry ( "/General/Locale" );
}

void CConfig::setLanguage ( const QString &lang )
{
	QString old = language ( );

	if ( old != lang ) {
		writeEntry ( "/General/Locale", lang );

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
		writeEntry ( "/General/WeightSystem", ws == WeightMetric ? "metric" : "imperial" );

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
		writeEntry ( "/General/SimpleMode", sm );

		emit simpleModeChanged ( sm );
	}
}


void CConfig::blUpdateIntervals ( int &pic, int &pg ) const
{
	int picd, pgd;
	
	blUpdateIntervalsDefaults ( picd, pgd );

	pic = CConfig::inst ( )-> readNumEntry ( "/BrickLink/UpdateInterval/Pictures",    picd );
	pg  = CConfig::inst ( )-> readNumEntry ( "/BrickLink/UpdateInterval/PriceGuides", pgd  );
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
		writeEntry ( "/BrickLink/UpdateInterval/Pictures",    pic );
		writeEntry ( "/BrickLink/UpdateInterval/PriceGuides", pg  );		

		emit blUpdateIntervalsChanged ( pic, pg );
	}
}


QString CConfig::blLoginUsername ( )
{
	return readEntry ( "/BrickLink/Login/Username" );
}

void CConfig::setBlLoginUsername ( const QString &name )
{
	writeEntry ( "/BrickLink/Login/Username", name );
}


QString CConfig::blLoginPassword ( )
{
	return readPasswordEntry ( "/BrickLink/Login/Password" );
}

void CConfig::setBlLoginPassword ( const QString &pass )
{
	writePasswordEntry ( "/BrickLink/Login/Password", pass );
}


bool CConfig::onlineStatus ( )
{
	return readBoolEntry ( "/Internet/Online", true );
}

void CConfig::setOnlineStatus ( bool b )
{
	bool ob = onlineStatus ( );
	
	if ( b != ob ) {
		writeEntry ( "/Internet/Online", b );
	
		emit onlineStatusChanged ( b );
	}
}

