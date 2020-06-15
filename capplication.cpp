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
#include <qtimer.h>
#include <qevent.h>
#include <qdir.h>
#include <qwindowsstyle.h>

//#include <qsaglobal.h>
#include <qevent.h>
#include <qpixmap.h>

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
#include "dlgregistrationimpl.h"
#include "cprogressdialog.h"
#include "ccheckforupdates.h"
#include "csplash.h"

#include "capplication.h"

class COpenEvent : public QEvent {
public:
	enum { Type = QEvent::User + 142 };

    COpenEvent ( const QString &file ) : QEvent ( (QEvent::Type)Type ), m_file ( file ) { }
	
    QString fileName ( ) const { return m_file; }
	
private:
	QString m_file;
};

CApplication *cApp = 0;                

CApplication::CApplication (int _argc, char **_argv )
    : QApplication ( _argc, _argv )
{
	cApp = this;

	m_enable_emit = false;

    // try not to look ugly
    if ( style()-> inherits ( "QMotifStyle" ) || style()-> inherits ( "QMotifPlusStyle" ))
        setStyle ( new QWindowsStyle ( ) );
    CSplash::inst ( );

    // initialize config & resource
    (void) CConfig::inst ( )-> upgrade ( BRICKSTOCK_MAJOR, BRICKSTOCK_MINOR, BRICKSTOCK_PATCH );
	(void) CMoney::inst ( );
	(void) CResource::inst ( );
    (void) CReportManager::inst ( );

	m_trans_qt = 0;
    m_trans_brickstock = 0;

	if ( !initBrickLink ( )) {
		// we cannot call quit directly, since there is
		// no event loop to quit from...
		postEvent ( this, new QEvent ( QEvent::Quit ));
		return;
	}
	else {
		updateTranslations ( );
		connect ( CConfig::inst ( ), SIGNAL( languageChanged ( )), this, SLOT( updateTranslations ( )));
		
		CSplash::inst ( )-> message ( qApp-> translate ( "CSplash", "Initializing..." ));

		CMessageBox::setDefaultTitle ( appName ( ));

		QPixmap pix;
	
		pix = CResource::inst ( )-> pixmap ( "icon" );
        Q3MimeSourceFactory::defaultFactory ( )-> setImage ( "brickstock-icon", pix. convertToImage ( ));
		pix = CResource::inst ( )-> pixmap ( "important" );
        Q3MimeSourceFactory::defaultFactory ( )-> setImage ( "brickstock-important", pix. convertToImage ( ));

		for ( int i = 1; i < argc ( ); i++ )
			m_files_to_open << argv ( ) [i];
	
        CSplash::inst ( )-> finish ( CFrameWork::inst ( ));
		CFrameWork::inst ( )-> show ( );

		while ( CConfig::inst ( )-> registration ( ) == CConfig::None ) {
			DlgRegistrationImpl d ( true, mainWidget ( ));
			d. exec ( );			
		}
		demoVersion ( );
		connect ( CConfig::inst ( ), SIGNAL( registrationChanged ( CConfig::Registration )), this, SLOT( demoVersion ( )));
	}
}
	
CApplication::~CApplication ( )
{
	exitBrickLink ( );

    Q3MimeSourceFactory::defaultFactory ( )-> setData ( "brickstock-icon", 0 );

    delete CReportManager::inst ( );
	delete CResource::inst ( );
	delete CMoney::inst ( );
	delete CConfig::inst ( );
}

void CApplication::updateTranslations ( )
{
	QString locale = CConfig::inst ( )-> language ( );
	if ( locale. isEmpty ( ))
		locale = QLocale::system ( ). name ( );
	QLocale::setDefault ( QLocale ( locale ));

	if ( m_trans_qt )
		removeTranslator ( m_trans_qt );
    if ( m_trans_brickstock )
        removeTranslator ( m_trans_brickstock );

	m_trans_qt = CResource::inst ( )-> translation ( "qt", locale );
	if ( m_trans_qt )
		installTranslator ( m_trans_qt );

    m_trans_brickstock = CResource::inst ( )-> translation ( "brickstock", locale );
    if ( m_trans_brickstock )
        installTranslator ( m_trans_brickstock );
}

QString CApplication::appName ( ) const
{
    return "BrickStock";
}

QString CApplication::appVersion ( ) const
{
    return BRICKSTOCK_VERSION;
}

QString CApplication::appURL ( ) const
{
    return BRICKSTOCK_URL;
}

QString CApplication::sysName ( ) const
{
	QString sys_name = "(unknown)";

#if defined( Q_OS_MACX )
	sys_name = "MacOS";
#elif defined( Q_OS_WIN )
	sys_name = "Windows";
#elif defined( Q_OS_UNIX )
	sys_name = "Unix";

	struct ::utsname utsinfo;
	if ( ::uname ( &utsinfo ) >= 0 )
		sys_name = utsinfo. sysname;
#endif

	return sys_name;
}

QString CApplication::sysVersion ( ) const
{
	QString sys_version = "(unknown)";

#if defined( Q_OS_MACX )
	switch ( QApplication::macVersion ( )) {
        case QSysInfo::MV_10_0: sys_version = "10.0 (Cheetah)";         break;
        case QSysInfo::MV_10_1: sys_version = "10.1 (Puma)";            break;
        case QSysInfo::MV_10_2: sys_version = "10.2 (Jaguar)";          break;
        case QSysInfo::MV_10_3: sys_version = "10.3 (Panther)";         break;
        case QSysInfo::MV_10_4: sys_version = "10.4 (Tiger)";           break;
        case QSysInfo::MV_10_5: sys_version = "10.5 (Leopard)";         break;
        case QSysInfo::MV_10_6: sys_version = "10.6 (Snow Leopard)";    break;
        case QSysInfo::MV_10_7: sys_version = "10.7 (Lion)";            break;
        case QSysInfo::MV_10_8: sys_version = "10.8 (Mountain Lion)";   break;
        case 0x000B: sys_version = "10.9 (Mavericks)";       break;
        default             : break;
	}
#elif defined( Q_OS_WIN )
	switch ( QApplication::winVersion ( )) {
        case QSysInfo::WV_95   : sys_version = "95";    break;
        case QSysInfo::WV_98   : sys_version = "98";    break;
        case QSysInfo::WV_Me   : sys_version = "ME";    break;
        case QSysInfo::WV_NT   : sys_version = "NT";    break;
        case QSysInfo::WV_2000 : sys_version = "2000";  break;
        case QSysInfo::WV_XP   : sys_version = "XP";    break;
        case QSysInfo::WV_2003 : sys_version = "2003";  break;
        case QSysInfo::WV_VISTA: sys_version = "VISTA"; break;
        case QSysInfo::WV_WINDOWS7: sys_version = "7";  break;
        case QSysInfo::WV_WINDOWS8: sys_version = "8";  break;
        default          : break;
	}
#elif defined( Q_OS_UNIX )
	struct ::utsname utsinfo;
	if ( ::uname ( &utsinfo ) >= 0 )
		sys_version = QString( "%2 (%3)" ). arg ( utsinfo. release, utsinfo. machine );
#endif

	return sys_version;
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

bool CApplication::event(QEvent *e)
{
    switch ( e -> type ( ) ) {
        case QEvent::FileOpen:
            m_files_to_open. append ( static_cast <QFileOpenEvent *> ( e )-> file ( ) );
            doEmitOpenDocument ( );

            return true;
        default:
            return QApplication::event ( e );
    }
}

bool CApplication::initBrickLink ( )
{
	QString errstring;
	QString defdatadir = QDir::homeDirPath ( );

#if defined( Q_OS_WIN32 )
    defdatadir += "/brickstock-cache/";
#else
    defdatadir += "/.brickstock-cache/";
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

void CApplication::about ( )
{
	static const char *layout =
		"<qt>"
			"<center>"
				"<table border=\"0\"><tr>"
                    "<td valign=\"middle\" align=\"right\"><img src=\"brickstock-icon\" /></td>"
					"<td align=\"left\"><big>"
						"<big><strong>%1</strong></big>"
                        "<br />%2<br />%3<br />"
                        "<strong>%4</strong>"
					"</big></td>"
				"</tr></table>"
                "<br />%5<br /><br />%6"
            "</center>%7<p>%8</p>"
		"</qt>";


    QString page1_link = QString( "<strong>%1</strong> | <a href=\"brickstock-info-page2\">%2</a>" ). arg( tr( "Legal Info" ), tr( "System Info" ));
    QString page2_link = QString( "<a href=\"brickstock-info-page1\">%1</a> | <strong>%2</strong>" ). arg( tr( "Legal Info" ), tr( "System Info" ));

    QString copyright       = tr( "Copyright &copy; %1" ). arg ( BRICKSTOCK_COPYRIGHT );
    QString baseCopyright   = tr( "Based on BrickStore. %1" ).arg( tr( "Copyright &copy; %1" ). arg ( BRICKSTOCK_BASE_COPYRIGHT ) );
    QString version         = tr( "Version %1" ). arg ( BRICKSTOCK_VERSION );
    QString support         = "";//tr( "Visit %1, or send an email to %2" ). arg ( "<a href=\"http://" BRICKSTOCK_URL "\">" BRICKSTOCK_URL "</a>", "<a href=\"mailto:" BRICKSTOCK_MAIL "\">" BRICKSTOCK_MAIL "</a>" );

	::curl_version_info_data *curlver = curl_version_info ( CURLVERSION_FIRST );
	QString curl = curlver-> version;
	QString z    = curlver-> libz_version;
	QString qt   = qVersion ( );
//  QString qsa  = "-1"; //QSA_VERSION_STRING;

    QString translators = "<b>" + tr( "Translators" ) + "</b><table border=\"0\">";
   	QDomDocument doc;
	QFile file ( CResource::inst ( )-> locate ( "translations/translations.xml" ));

//  qWarning(QLocale::languageToString(QLocale().language()).ascii());
	if ( file. open ( QIODevice::ReadOnly )) {
		QString err_str;
		int err_line = 0, err_col = 0;
	
		if ( doc. setContent ( &file, &err_str, &err_line, &err_col )) {
			QDomElement root = doc. documentElement ( );

			if ( root. isElement ( ) && root. nodeName ( ) == "translations" ) {
				for ( QDomNode trans = root. firstChild ( ); !trans. isNull ( ); trans = trans. nextSibling ( )) {
					if ( !trans. isElement ( ) || ( trans. nodeName ( ) != "translation" ))
						continue;
					QDomNamedNodeMap map = trans. attributes ( );

                    if ( !map.contains ( "default" )) {
    					QString lang_id = map. namedItem ( "lang" ). toAttr ( ). value ( );
	    				QString author = map. namedItem ( "author" ). toAttr ( ). value ( );
		    			QString email = map. namedItem ( "authoremail" ). toAttr ( ). value ( );
                        QString langname;

                        for ( QDomNode name = trans. firstChild ( ); !name. isNull ( ); name = name. nextSibling ( )) {
						    if ( !name. isElement ( ) || ( name. nodeName ( ) != "name" ))
							    continue;
						    QDomNamedNodeMap map = name. attributes ( );

                            if ( QLocale().name().startsWith ( map. namedItem ( "lang" ). toAttr ( ). value ( )))
                                langname = name. toElement ( ). text ( );
                        }
                        if ( !langname. isEmpty ( ))
                            translators += QString("<tr><td>%1</td><td>%2 &lt;<a href=\"mailto:%3\">%4</a>&gt;</td></tr>").arg(langname, author, email, email);
                    }
                }
            }
        }
    }
    translators += "</table>";

	static const char *legal_src = QT_TR_NOOP(
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

	static const char *technical_src = 
		"<p>"
			"<strong>Runtime Info</strong><br /><br />"
			"<table>"	
                "<tr><td>OS     </td><td>%1</td></tr>"
                "<tr><td>libqt  </td><td>%2</td></tr>"
                "<tr><td>libcurl</td><td>%3</td></tr>"
                "<tr><td>libz   </td><td>%4</td></tr>"
			"</table>"
		"</p>";

    QString technical = QString ( technical_src ). arg ( sysName ( ) + " " + sysVersion ( )). arg ( qt, curl, z );

	QString legal = tr( legal_src );

    QString page1 = QString ( layout ). arg ( appName ( ), copyright, baseCopyright, version, support ). arg ( page1_link, legal, translators );
    QString page2 = QString ( layout ). arg ( appName ( ), copyright, baseCopyright, version, support ). arg ( page2_link, technical, QString());

    Q3MimeSourceFactory::defaultFactory ( )-> setText ( "brickstock-info-page1", page1 );
    Q3MimeSourceFactory::defaultFactory ( )-> setText ( "brickstock-info-page2", page2 );

	DlgMessageImpl d ( appName ( ), page1, false, mainWidget ( ));
	d. exec ( );

    Q3MimeSourceFactory::defaultFactory ( )-> setData ( "brickstock-info-page1", 0 );
    Q3MimeSourceFactory::defaultFactory ( )-> setData ( "brickstock-info-page2", 0 );
}

void CApplication::demoVersion ( )
{
	if ( CConfig::inst ( )-> registration ( ) != CConfig::Demo )
		return;

	static const char *layout =
        "<qt><center>"
			"<table border=\"0\"><tr>"
                "<td valign=\"middle\" align=\"right\"><img src=\"brickstock-icon\" /></td>"
				"<td align=\"left\"><big>"
					"<big><strong>%1</strong></big>"
                    "<br />%2<br />%3<br />"
                    "<strong>%4</strong>"
				"</big></td>"
			"</tr></table>"
            "<br />%5"
        "</center</qt>";

	static const char *demo_src = QT_TR_NOOP(
        "BrickStock is currently running in <b>Demo</b> mode.<br /><br />"
		"The complete functionality is accessible, but this reminder will pop up every 20 minutes."
		"<br /><br />"
		"You can change the mode of operation at anytime via <b>Help > Registration...</b>"
	);

    QString copyright       = tr( "Copyright &copy; %1" ). arg ( BRICKSTOCK_COPYRIGHT );
    QString baseCopyright   = tr( "Based on BrickStore. %1" ).arg( tr( "Copyright &copy; %1" ). arg ( BRICKSTOCK_BASE_COPYRIGHT ) );
    QString version         = tr( "Version %1" ). arg ( BRICKSTOCK_VERSION );
    QString demo            = tr( demo_src );
    QString text            = QString ( layout ). arg ( appName ( ), copyright, baseCopyright, version, demo );

    DlgMessageImpl d ( appName ( ), text, true, mainWidget ( ));
	d. exec ( );

	QTimer::singleShot ( 20*60*1000, this, SLOT( demoVersion ( )));
}

void CApplication::checkForUpdates ( )
{
	CProgressDialog d ( mainWidget ( ));
	CCheckForUpdates cfu ( &d );
	d. exec ( );
}

void CApplication::registration ( )
{
	CConfig::Registration oldreg = CConfig::inst ( )-> registration ( );

	DlgRegistrationImpl d ( false, mainWidget ( ));
	if (( d. exec ( ) == QDialog::Accepted ) &&
	    ( CConfig::inst ( )-> registration ( ) == CConfig::Demo ) &&
		( CConfig::inst ( )-> registration ( ) != oldreg )) 
	{
		CMessageBox::information ( mainWidget ( ), tr( "The program has to be restarted to activate the Demo mode." ));
		mainWidget ( )-> close ( );
		quit ( );
	}
}
