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

//#define BS_DEMO  30 // demo time in minutes


#include "capplication.h"


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

	for ( int i = 1; i < argc ( ); i++ )
		m_files_to_open << argv ( ) [i];

	// initialize config & resource
	(void) CConfig::inst ( );
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

	CMessageBox::setDefaultTitle ( m_appname );

	QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
	QMimeSourceFactory::defaultFactory ( )-> setImage ( "brickstore-icon", pix. convertToImage ( ));

	if ( !initBrickLink ( )) {
		// we cannot call quit directly, since there is
		// no event loop to quit from...
		postEvent ( this, new QEvent ( QEvent::Quit ));
		return;
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

	QString text = QString ( layout_text ). arg ( m_appname, m_copyright, m_version, m_demo );

	DlgMessageImpl d ( m_appname, text, mainWidget ( ));
	d. exec ( );

	QTimer::singleShot ( BS_DEMO * 60 * 1000, this, SLOT( demoVersion ( )));
#endif
}

QString CApplication::appName ( ) const
{
	return m_appname;
}

QString CApplication::appVersion ( ) const
{
	return QString( BRICKSTORE_VERSION );
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
		"</p><p>"
		"French translation by Sylvain (<a href=\"mailto:bricklink@1001bricks.com\">1001bricks</a>)"
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

	m_appname   = tr( "BrickStore" );
	m_copyright = tr( "Copyright &copy; %1" ). arg ( BRICKSTORE_COPYRIGHT );
	m_version   = tr( "Version %1" ). arg ( BRICKSTORE_VERSION );
	m_support   = tr( "Visit %1, or send an email to %2" ). arg ( url ). arg ( mail );
	m_demo      = tr( demo_text ). arg ( url );
	m_legal     = tr( legal_text );
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

	QString text = QString ( layout_text ). arg ( m_appname ). arg ( m_copyright, m_version, m_support, m_legal );

	DlgMessageImpl d ( m_appname, text, mainWidget ( ));
	d. exec ( );
}

void CApplication::checkForUpdates ( )
{
	DlgUpdateImpl d ( mainWidget ( ));
	d. exec ( );
}
