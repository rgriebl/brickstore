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
#include <qtimer.h>
#include <qevent.h>
#include <qdir.h>

#if defined( Q_OS_UNIX )
#include <sys/utsname.h>
#endif

#include "cconfig.h"
#include "cmoney.h"
#include "cresource.h"
#include "cframework.h"
#include "cmessagebox.h"
#include "creport.h"
#include "bricklink.h"
#include "version.h"
#include "dlgmessageimpl.h"
#include "dlgupdateimpl.h"
#include "crebuilddatabase.h"

#include "capplication.h"

#include "commands.h"

//#define BS_DEMO  30 // demo time in minutes


class COpenEvent : public QCustomEvent {
public:
	enum { Type = QEvent::User + 142 };

	COpenEvent ( const QString &file ) 
		: QCustomEvent ( Type ), m_file ( file ) 
	{ }
	
	QString fileName ( ) const 
	{ return m_file; }
	
private:
	QString m_file;
};


CApplication *cApp = 0;                

CApplication::CApplication ( int _argc, char **_argv ) 
	: QApplication ( _argc, _argv )
{
	cApp = this;

	m_enable_emit = false;

#if defined( Q_WS_MACX )
	AEInstallEventHandler( kCoreEventClass, kAEOpenDocuments, appleEventHandler, 0, false );
#endif

	if (( argc ( ) == 2 ) && ( !strcmp( argv ( ) [1], "-h" ) || !strcmp( argv ( ) [1], "--help" ))) {
#if defined( Q_OS_WIN32 )
		QMessageBox::information ( 0, "BrickStore", "<b>Usage:</b><br />brickstore.exe [&lt;files&gt;]<br /><br />brickstore.exe --rebuild-database &lt;dbname&gt;<br />", QMessageBox::Ok );
#else
		printf ( "Usage: %s [<files>]\n", _argv [0] );
		printf ( "       %s --rebuild-database <dbname>\n", _argv [0] );
#endif

		postEvent ( this, new QEvent ( QEvent::Quit ));
		return;
	}
	else if (( argc ( ) == 3 ) && ( !strcmp( argv ( ) [1], "--rebuild-database" ))) {
		m_rebuild_db_only = argv ( ) [2];
	}
	else {
		for ( int i = 1; i < argc ( ); i++ )
			m_files_to_open << argv ( ) [i];
	}

	// initialize config & resource
	(void) CConfig::inst ( )-> upgrade ( BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH );
	(void) CMoney::inst ( );
	(void) CResource::inst ( );
	(void) CReportManager::inst ( );

	QTranslator *trans;

	trans = CResource::inst ( )-> translation ( "qt" );
	if ( trans )
		installTranslator ( trans );

	trans = CResource::inst ( )-> translation ( "brickstore" );
	if ( trans )
		installTranslator ( trans );

	initStrings ( );

	CMessageBox::setDefaultTitle ( appName ( ));

	QPixmap pix;
	
	pix = CResource::inst ( )-> pixmap ( "icon" );
	QMimeSourceFactory::defaultFactory ( )-> setImage ( "brickstore-icon", pix. convertToImage ( ));
	pix = CResource::inst ( )-> pixmap ( "important" );
	QMimeSourceFactory::defaultFactory ( )-> setImage ( "brickstore-important", pix. convertToImage ( ));

	if ( !initBrickLink ( )) {
		// we cannot call quit directly, since there is
		// no event loop to quit from...
		postEvent ( this, new QEvent ( QEvent::Quit ));
		return;
	}
	else if ( !m_rebuild_db_only. isEmpty ( )) {	
		QTimer::singleShot ( 0, this, SLOT( rebuildDatabase ( )));
	}
	else {
		CFrameWork::inst ( )-> show ( );
	
		demoVersion ( );
	}
}
	
CApplication::~CApplication ( )
{
#if defined( Q_WS_MACX )
	AERemoveEventHandler ( kCoreEventClass, kAEOpenDocuments, appleEventHandler, false );
#endif
	exitBrickLink ( );

	QMimeSourceFactory::defaultFactory ( )-> setData ( "brickstore-icon", 0 );

	delete CReportManager::inst ( );
	delete CResource::inst ( );
	delete CMoney::inst ( );
	delete CConfig::inst ( );
}

void CApplication::exit ( int code )
{
	QApplication::exit ( code );
}

void CApplication::rebuildDatabase ( )
{
	CRebuildDatabase *rdb = new CRebuildDatabase ( m_rebuild_db_only );

	connect ( rdb, SIGNAL( finished ( int )), cApp, SLOT( exit ( int )));
	rdb-> exec ( );
}


void CApplication::demoVersion ( )
{
#if defined( BS_DEMO )
	static const char *layout_text =
		"<qt><center>"
			"<table border=\"0\"><tr>"
				"<td valign=\"middle\" align=\"right\"><img src=\"brickstore-icon\" /></td>"
				"<td align=\"left\"><big>"
					"<big><strong>%1</strong></big>"
					"<br />%2<br />"
					"<strong>%3</strong>"
				"</big></td>"
			"</tr></table>"
			"<br />%4"
		"</center</qt>";

	QString text = QString ( layout_text ). arg ( appName ( ), m_copyright, m_version, m_demo );

	DlgMessageImpl d ( appName ( ), text, mainWidget ( ));
	d. exec ( );

	QTimer::singleShot ( BS_DEMO * 60 * 1000, this, SLOT( demoVersion ( )));
#endif
}

QString CApplication::appName ( ) const
{
	return "BrickStore";
}

QString CApplication::appVersion ( ) const
{
	return BRICKSTORE_VERSION;
}

QString CApplication::appURL ( ) const
{
	return BRICKSTORE_URL;
}

QString CApplication::sysName ( ) const
{
	return m_sys_name;
}

QString CApplication::sysVersion ( ) const
{
	return m_sys_version;
}

void CApplication::enableEmitOpenDocument ( bool b )
{
	if ( b != m_enable_emit ) {
		m_enable_emit = b;
		
		if ( b && !m_files_to_open. isEmpty ( ))
			QTimer::singleShot ( 0, this, SLOT( doEmitOpenDocument ( )));
	}
}

void CApplication::doEmitOpenDocument ( )
{
	while ( m_enable_emit && !m_files_to_open. isEmpty ( )) {
		QString file = m_files_to_open. front ( );
		m_files_to_open. pop_front ( );
		
		emit openDocument ( file );
	}
}

void CApplication::customEvent ( QCustomEvent *e )
{
	if ( int( e-> type ( )) == int( COpenEvent::Type )) {
		m_files_to_open. append ( static_cast <COpenEvent *> ( e )-> fileName ( ));
		
		doEmitOpenDocument ( );
	}
}
   	
#if defined( Q_WS_MACX )
OSErr CApplication::appleEventHandler ( const AppleEvent *event, AppleEvent *, long )
{
	AEDescList docs;
	
	if ( AEGetParamDesc ( event, keyDirectObject, typeAEList, &docs ) == noErr ) {
		long n = 0;
		AECountItems ( &docs, &n );
		UInt8 strbuffer [PATH_MAX];
		for ( long i = 0; i < n; i++ ) {
			FSRef ref;
			if ( AEGetNthPtr ( &docs, i + 1, typeFSRef, 0, 0, &ref, sizeof( ref ), 0) != noErr )
				continue;
			if ( FSRefMakePath ( &ref, strbuffer, 256 ) == noErr ) {
				COpenEvent e ( QString::fromUtf8 ( reinterpret_cast <char *> ( strbuffer )));
				QApplication::sendEvent ( qApp, &e );
			}
		}	
	}
	return noErr;
}
#endif


bool CApplication::initBrickLink ( )
{
	QString errstring;
	QString defdatadir = QDir::homeDirPath ( );

#if defined( Q_OS_WIN32 )
	defdatadir += "/brickstore-cache/";
#else
	defdatadir += "/.brickstore-cache/";
#endif

	BrickLink *bl = BrickLink::inst ( CConfig::inst ( )-> readEntry ( "/BrickLink/DataDir", defdatadir ), &errstring );

	if ( !bl )
		CMessageBox::critical ( 0, tr( "Could not initialize the BrickLink kernel:<br /><br />%1" ). arg ( errstring ));

	return ( bl != 0 );
}


void CApplication::exitBrickLink ( )
{
	delete BrickLink::inst ( );
}


void CApplication::initStrings ( )
{
	static const char *legal_text = QT_TRANSLATE_NOOP( "CApplication",
		"<p>"
		"This program is free software; it may be distributed and/or modified "
		"under the terms of the GNU General Public License version 2 as published "
		"by the Free Software Foundation and appearing in the file LICENSE.GPL "
		"included in this software package."
		"<br />"
		"This program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE "
		"WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE."
		"<br />"
		"See <a href=\"http://fsf.org/licensing/licenses/gpl.html\">www.fsf.org/licensing/licenses/gpl.html</a> for GPL licensing information."
		"</p><p>"
		"All data from <a href=\"http://www.bricklink.com\">www.bricklink.com</a> is owned by BrickLink<sup>TM</sup>, "
		"which is a trademark of Dan Jezek."
		"</p><p>"
		"Peeron Inventories from <a href=\"http://www.peeron.com\">www.peeron.com</a> are owned by Dan and Jennifer Boger."
		"</p><p>"
		"LEGO<sup>&reg;</sup> is a trademark of the LEGO group of companies, "
		"which does not sponsor, authorize or endorse this software."
		"</p><p>"
		"All other trademarks recognised."
		"</p>"
	);

	static const char *demo_text = QT_TRANSLATE_NOOP( "CApplication",
		"This is an <b>unrestricted</b> demo version."
		"<br /><br />"
		"If you want to support the development of this program "
		"(or if you just want to get rid of this dialog...), "
		"check out %1 how to get the non-demo version." 
	);

	QString url  = "<a href=\"http://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>";
	QString mail = "<a href=\"mailto:" BRICKSTORE_MAIL "\">" BRICKSTORE_MAIL "</a>";

	m_copyright  = tr( "Copyright &copy; %1" ). arg ( BRICKSTORE_COPYRIGHT );
	m_version    = tr( "Version %1" ). arg ( BRICKSTORE_VERSION );
	m_support    = tr( "Visit %1, or send an email to %2" ). arg ( url ). arg ( mail );
	m_demo       = tr( demo_text ). arg ( url );
	m_legal      = tr( legal_text );

	m_sys_name    = "(unknown)";
	m_sys_version = "(unknown)";

#if defined( Q_OS_MACX )
	m_sys_name = "MacOS";

	switch ( QApplication::macVersion ( )) {
		case Qt::MV_10_DOT_0: m_sys_version = "10.0 (Cheetah)"; break;
		case Qt::MV_10_DOT_1: m_sys_version = "10.1 (Puma)";    break;
		case Qt::MV_10_DOT_2: m_sys_version = "10.2 (Jaguar)";  break;
		case Qt::MV_10_DOT_3: m_sys_version = "10.3 (Panther)"; break;
		case Qt::MV_10_DOT_4: m_sys_version = "10.4 (Tiger)";   break;
		default             : break;
	}

#elif defined( Q_OS_WIN )
	m_sys_name = "Windows";

	switch ( QApplication::winVersion ( )) {
		case Qt::WV_95  : m_sys_version = "95";   break;
		case Qt::WV_98  : m_sys_version = "98";   break;
		case Qt::WV_Me  : m_sys_version = "ME";   break;
		case Qt::WV_NT  : m_sys_version = "NT";   break;
		case Qt::WV_2000: m_sys_version = "2000"; break;
		case Qt::WV_XP  : m_sys_version = "XP";   break;
		case Qt::WV_2003: m_sys_version = "2003"; break;
		default         : break;
	}

#elif defined( Q_OS_UNIX )
	m_sys_name = "Unix";

	struct ::utsname utsinfo;
	if ( ::uname ( &utsinfo ) >= 0 )
		m_sys_version = utsinfo. sysname + " " + utsinfo. machine;

#endif
}

void CApplication::about ( )
{
	static const char *layout_text =
		"<qt><center>"
			"<table border=\"0\"><tr>"
				"<td valign=\"middle\" align=\"right\"><img src=\"brickstore-icon\" /></td>"
				"<td align=\"left\"><big>"
					"<big><strong>%1</strong></big>"
					"<br />%2<br />"
					"<strong>%3</strong>"
				"</big></td>"
			"</tr></table>"
			"<br />%4"
		"</center>%5</qt>";

	QString text = QString ( layout_text ). arg ( appName ( )). arg ( m_copyright, m_version, m_support, m_legal );

	DlgMessageImpl d ( appName ( ), text, mainWidget ( ));
	d. exec ( );
}

void CApplication::checkForUpdates ( )
{
	DlgUpdateImpl d ( mainWidget ( ));
	d. exec ( );
}
