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

#include <math.h>

#include <QColor>
#include <QDesktopWidget>
//qfontmetrics.h>
#include <QApplication>
//#include <qcursor.h>
//#include <qimage.h>
#include <QDir>
#include <QDateTime>
#include <QLocale>
#include <QWidget>

#if defined ( Q_WS_X11 ) || defined ( Q_WS_MACX )
#include <stdlib.h>
#endif

#if defined( Q_WS_WIN )
#include <windows.h>
#include <tchar.h>
#include <shlobj.h>
#endif

#if defined ( Q_WS_MACX )
#include <Carbon/Carbon.h>
#endif

#include "cutility.h"


QString CUtility::ellipsisText ( const QString &org, const QFontMetrics &fm, int width, int align )
{
	int ellWidth = fm. width ( "..." );
	QString text = QString::fromLatin1 ( "" );
	int i = 0;
	int len = org. length ( );
	int offset = ( align & Qt::AlignRight ) ? ( len - 1 ) - i : i;

	while ( i < len && fm. width ( text + org [offset] ) + ellWidth < width ) {
		if ( align & Qt::AlignRight )
			text. prepend ( org [offset] );
		else
			text += org [offset];
		offset = ( align & Qt::AlignRight ) ? ( len - 1 ) - ++i : ++i;
	}

	if ( text. isEmpty ( ))
		text = ( align & Qt::AlignRight ) ? org. right ( 1 ) : text = org. left ( 1 );

	if ( align & Qt::AlignRight )
		text. prepend ( "..." );
	else
		text += "...";
	return text;
}


QColor CUtility::gradientColor ( const QColor &c1, const QColor &c2, qreal f )
{
	qreal r1, g1, b1, a1, r2, g2, b2, a2;
	c1. getRgbF ( &r1, &g1, &b1, &a1 );
	c2. getRgbF ( &r2, &g2, &b2, &a2 );

	f = qMin( qMax ( f, 0.0 ), 1.0 );
	qreal e = 1.0 - f;

	return QColor::fromRgbF ( r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f );
}

QColor CUtility::contrastColor ( const QColor &c, qreal f )
{
	qreal h, s, v, a;
	c. getHsvF ( &h, &s, &v, &a );

	f = qMin( qMax ( f, 0.0 ), 1.0 );

	v += f * (( v < 0.5 ) ? 1.0 : -1.0 );
	v = qMin( qMax( v, 0.0 ), 1.0 );

	return QColor::fromHsvF ( h, s, v, a );
}

#if 0
QImage CUtility::createGradient ( const QSize &size, Qt::Orientation orient, const QColor &c1, const QColor &c2, float f )
{
	if ( size. isEmpty ( ))
		return QImage ( );

	QImage img ( size, 32 );
	int w = size. width ( );
	int h = size. height ( );

	int c1r, c1g, c1b, c2r, c2g, c2b, dr, dg, db;

	c1. getRgb ( &c1r, &c1g, &c1b ); 
	c2. getRgb ( &c2r, &c2g, &c2b ); 

	dr = c2r - c1r; 
	dg = c2g - c1g;
	db = c2b - c1b;

	bool invert = ( f < 0. );

	f = qMin( qMax( QABS( f ), 1. ), 200. );

	if ( orient == Qt::Horizontal ) {
		f /= ( w * -30. );
		
		uint *line = (uint *) img. scanLine ( 0 );

		for ( int x = 0; x < w; x++ ) {
			float e = 1 - exp ( f * x );

			line [invert ? w - x - 1 : x] = qRgb( c2r - int( e * dr ),
			                                      c2g - int( e * dg ),
			                                      c2b - int( e * db ));
		}

		for ( int y = 1; y < h; y++ )
			memcpy ( img. scanLine ( y ), line, w * sizeof( uint ));
	}
	else {
		f /= ( h * -30. );
		
		for ( int y = 0; y < h; y++ ) {
			uint *line = (uint *) img. scanLine ( invert ? h - y - 1 : y );
			float e = 1 - exp ( f * y );

			QRgb c = qRgb( c2r - int( e * dr ),
			               c2g - int( e * dg ),
			               c2b - int( e * db ));

			for ( int x = 0; x < w; x++ )
				*line++ = c;
		}
	}
	return img;
}
#endif

#if 0
QImage CUtility::shadeImage ( const QImage &oimg, const QColor &col )
{
	if ( oimg. isNull ( ))
		return oimg;

	QImage img = oimg;
	if ( img. depth ( ) != 32 )
		img. convertDepth ( 32 );

	int rc = col. red ( );
	int gc = col. green ( );
	int bc = col. blue ( );

	for ( int y = 0; y < img. height ( ); y++ ) {
		uint *line = (uint *) img. scanLine ( y );

		for ( int x = 0; x < img. width ( ); x++ ) {
			int r = qRed   ( *line ) * rc / 255;
			int g = qGreen ( *line ) * gc / 255;
			int b = qBlue  ( *line ) * bc / 255;

			*line = qRgba ( r, g, b, qAlpha ( *line ));
			line++;
		}
	}
	return img;
}
#endif

#if 0
bool CUtility::openUrl ( const QString &url )
{
	bool retval = false;
	QApplication::setOverrideCursor ( QCursor( Qt::BusyCursor ));

#if defined( Q_WS_X11 )
	const char *trylist [] = {
		"kfmclient exec \"%s\"",
		// "gnome ???",
		"mozilla-firefox \"%s\" &",
		"mozilla \"%s\" &",
		"opera \"%s\" &",
		0
	};

	QCString buffer;
	for ( const char **tryp = trylist; *tryp; tryp++ ) {
		buffer. sprintf ( *tryp, url. local8Bit ( ). data ( ));

		int rc = ::system ( buffer. data ( ));
		 
		if ( rc != 127 ) {
			retval = ( rc == 0 );
			break;
		}
	}

#elif defined( Q_WS_WIN )
	if ( qApp-> mainWidget ( )) {
		QT_WA( {
			retval = (((DWORD_PTR) ::ShellExecuteW ( qApp-> mainWidget ( )-> winId ( ), L"open", (LPCWSTR) url. ucs2 ( ), 0, 0, SW_SHOW )) > 32 );
		}, {
			retval = (((DWORD_PTR) ::ShellExecuteA ( qApp-> mainWidget ( )-> winId ( ), "open", url. local8Bit ( ), 0, 0, SW_SHOW )) > 32 );
		} )
	}

#elif defined( Q_WS_MACX )
	QCString buffer;
	buffer. sprintf ( "open \"%s\"", url. local8Bit ( ). data ( ));

	retval = ( ::system ( buffer. data ( )) == 0 );

#endif

	QApplication::restoreOverrideCursor ( );
	return retval;
}
#endif


void CUtility::setPopupPos ( QWidget *w, const QRect &pos )
{
	QSize sh = w-> sizeHint ( );
	QSize desktop = qApp-> desktop ( )-> size ( );

	int x = pos. x ( ) + ( pos. width ( ) - sh. width ( )) / 2;
	int y = pos. y ( ) + pos. height ( );

	if (( y + sh. height ( )) > desktop. height ( )) {
		int d = w-> frameSize ( ). height ( ) - w-> size ( ). height ( );
#if defined( Q_WS_X11 )
		if (( d <= 0 ) && qApp-> mainWidget ( ))
			d = qApp-> mainWidget ( )-> frameSize ( ). height ( ) - qApp-> mainWidget ( )-> size ( ). height ( );
#endif
		y = pos. y ( ) - sh. height ( ) - d;
	}
	if ( y < 0 )
		y = 0;

	if (( x + sh. width ( )) > desktop. width ( ))
		x = desktop. width ( ) - sh. width ( );
	if ( x < 0 )
		x = 0;

	w-> move ( x, y );
	w-> resize ( sh );
}



#if 0
QString CUtility::safeOpen ( const QString &basepath )
{
	QDir cwd;
	if ( cwd. exists ( basepath ))
		return basepath;
	else if ( cwd. exists ( basepath + ".old" ))
		return basepath + ".old";
	else
		return basepath;
}
#endif

QString CUtility::safeRename ( const QString &basepath )
{
	QString error;
	QString newpath = basepath + ".new";
	QString oldpath = basepath + ".old";
	
	QDir cwd;

	if ( cwd. exists ( newpath )) {
		if ( !cwd. exists ( oldpath ) || cwd. remove ( oldpath )) {
			if ( !cwd. exists ( basepath ) || cwd. rename ( basepath, oldpath )) {
				if ( cwd. rename ( newpath, basepath ))
					error = QString ( );
				else
					error = qApp-> translate ( "CUtility", "Could not rename %1 to %2." ). arg( newpath ). arg( basepath );
			}
			else
				error = qApp-> translate ( "CUtility", "Could not backup %1 to %2." ). arg( basepath ). arg( oldpath );
		}
		else
			error = qApp-> translate ( "CUtility", "Could not delete %1." ). arg( oldpath );
	}
	else
		error = qApp-> translate ( "CUtility", "Could not find %1." ). arg( newpath );

	return error;
}

namespace {

void set_tz ( const char *tz )
{
	char pebuf [256] = "TZ=";
	if ( tz )
		strcat ( pebuf, tz );

#if defined( Q_OS_WIN32 )
	_putenv ( pebuf );
	_tzset ( );
#else
	putenv ( pebuf );
	tzset ( );
#endif
}

}

time_t CUtility::toUTC ( const QDateTime &dt, const char *settz )
{
    QByteArray oldtz;
	
	if ( settz ) {
		oldtz = getenv ( "TZ" );
		set_tz ( settz );
	}

	// get a tm structure from the system to get the correct tz_name
	time_t t = time ( 0 );
	struct tm *lt = localtime ( &t );

	lt-> tm_sec   = dt. time ( ). second ( );
	lt-> tm_min   = dt. time ( ). minute ( );
	lt-> tm_hour  = dt. time ( ). hour ( );
	lt-> tm_mday  = dt. date ( ). day ( );
	lt-> tm_mon   = dt. date ( ). month ( ) - 1;   // 0-11 instead of 1-12
	lt-> tm_year  = dt. date ( ). year ( ) - 1900; // year - 1900
	lt-> tm_wday  = -1;
	lt-> tm_yday  = -1;
	lt-> tm_isdst = -1;	// tm_isdst negative -> mktime will find out about DST

	// keep tm_zone and tm_gmtoff
	t = mktime ( lt );

	if ( settz )
		set_tz ( oldtz. data ( ));

	//time_t t2 = dt.toTime_t();

	return t;
}
