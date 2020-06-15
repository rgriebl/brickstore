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

#include <qapplication.h>
#include <qicon.h>
#include <qfile.h>
#include <qdir.h>
#include <qlocale.h>
//Added by qt3to4:
#include <QPixmap>
#include <QTranslator>

#if defined( Q_OS_MACX )
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#endif

#include "cconfig.h"
#include "cresource.h"


CResource *CResource::s_inst = 0;

CResource *CResource::inst ( )
{
	if ( !s_inst )
		s_inst = new CResource ( );
	return s_inst;
}

CResource::~CResource ( )
{
	s_inst = 0;
}

QString CResource::toQString(CFStringRef str)
{
    if (!str)
        return QString();

    CFIndex length = CFStringGetLength(str);
    if (length == 0)
        return QString();

    QString string(length, Qt::Uninitialized);
    CFStringGetCharacters(str, CFRangeMake(0, length), reinterpret_cast<UniChar *>
        (const_cast<QChar *>(string.unicode())));
    return string;
}

CResource::CResource ( )
	: m_iconset_dict ( 23 ), m_pixmap_dict ( 23 )
{
	m_iconset_dict. setAutoDelete ( true );
	m_pixmap_dict. setAutoDelete ( true );

	m_has_alpha = ( QPixmap::defaultDepth ( ) >= 15 );

#if defined( Q_WS_MACX )
	CFURLRef bundlepath = CFBundleCopyBundleURL ( CFBundleGetMainBundle ( ));
	CFStringRef macpath = CFURLCopyFileSystemPath ( bundlepath, kCFURLPOSIXPathStyle );

	// default
    m_paths << toQString ( macpath ) + "/Contents/Resources/";
	// fallback
	m_paths << qApp-> applicationDirPath ( ) + "/../Resources/";

	CFRelease ( macpath );
	CFRelease ( bundlepath );

#elif defined( Q_WS_WIN )
	// default
	m_paths << qApp-> applicationDirPath ( ) + "/";
	// development (debug in developer-studio)
	m_paths << qApp-> applicationDirPath ( ) + "/../";
	// fallback
    m_paths << QString ( ::getenv ( "ProgramFiles" )) + "/BrickStock/";
		
	int wv = QApplication::winVersion ( );

	m_has_alpha &= (( wv != QSysInfo::WV_95 ) && ( wv != QSysInfo::WV_NT ));
	
    QPixmap::setDefaultOptimization ( QPixmap::MemoryOptim );

	// using the native dialog causes the app to crash under W2K (comdlg32.dll)
	// (both the release and the debug versions will crash ONLY when run
	//  without a debugger, destroying the stack at the same time...)
	
	extern bool qt_use_native_dialogs;
	qt_use_native_dialogs = !(( wv & QSysInfo::WV_DOS_based ) || (( wv & QSysInfo::WV_NT_based ) < QSysInfo::WV_XP ));

#elif defined( Q_WS_X11 )
	// default
	m_paths << "/usr/share/brickstock/";
	// alternative
	m_paths << "/opt/brickstock/";
	// fallbck / development
	m_paths << qApp-> applicationDirPath ( ) + "/";

	extern bool qt_use_xrender;
	m_has_alpha &= qt_use_xrender;

#endif
}

bool CResource::pixmapAlphaSupported ( ) const
{
	return m_has_alpha;
}

QString CResource::locate ( const QString &name, LocateType lt )
{
	for ( QStringList::const_iterator it = m_paths. begin ( ); it != m_paths. end ( ); ++it ) {
		QString s = *it + name;

		if ( lt == LocateFile ) {
			if ( QFile::exists ( s )) 
				return s;
		}
		else if ( lt == LocateDir ) {
			QDir d ( s );

			if ( d. exists ( ) && d. isReadable ( ))
				return s;
		}
	}
	return QString::null;
}

QTranslator *CResource::translation ( const char *name, const QLocale &locale )
{
	QTranslator *trans = new QTranslator ( qApp );

	if ( qSharedBuild ( ) && !qstrcmp ( name, "qt" )) {
		if ( trans-> load ( QString( qInstallPathTranslations ( )) + "/" + name + "_" + locale. name ( )))
			return trans;
	}
	
	for ( QStringList::const_iterator it = m_paths. begin ( ); it != m_paths. end ( ); ++it ) {
		QString s = *it + "/translations/" + name + "_" + locale. name ( );

		if ( trans-> load ( s ))
			return trans;
	}
	delete trans;
	return 0;
}

QIcon CResource::icon ( const char *name )
{
	if ( !name || !name [0] )
        return QIcon ( );

    QIcon *is = m_iconset_dict [name];

	if ( !is ) {
		QPixmap large ( locate ( QString( "images/22x22/" ) + name + ".png" ));
        //QPixmap small ( locate ( QString( "images/16x16/" ) + name + ".png" ));

        if ( !large. isNull ( )/* && !small. isNull ( )*/) {
            is = new QIcon ( large );
			m_iconset_dict. insert ( name, is );
		}
	}
    return is ? *is : QIcon ( );
}

QPixmap CResource::pixmap ( const char *name )
{
	if ( !name || !name [0] )
		return QPixmap ( );

	QPixmap *p = m_pixmap_dict [name];

	if ( !p ) {
		QPixmap pix;

		if ( pix. load ( locate ( QString( "images/" ) + name + ".png" )) && !pix. isNull ( ))
			p = new QPixmap ( pix );
		else if ( pix. load ( locate ( QString( "/images/" ) + name + ".jpg" )) && !pix. isNull ( ))
			p = new QPixmap ( pix );

		if ( p )
			m_pixmap_dict. insert ( name, p );
	}
	return p ? *p : QPixmap ( );
}

