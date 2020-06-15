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
#include <qapplication.h>
#include <qpainter.h>
#include <qimage.h>
//Added by qt3to4:
#include <QPixmap>
#include <qdesktopwidget.h>

#include "capplication.h"
#include "cutility.h"
#include "cresource.h"
#include "csplash.h"


CSplash *CSplash::s_inst = 0;
bool CSplash::s_dont_show = false;

CSplash *CSplash::inst ( )
{
	if ( !s_inst && !s_dont_show ) {
		s_inst = new CSplash ( );
		s_inst-> show ( );
	}
	return s_inst;
}

CSplash::CSplash ( )
	: QSplashScreen ( QPixmap ( 1, 1 ), Qt::WDestructiveClose )
{
	float d = qApp-> desktop ( )-> height ( ) / 1200.f;

	QFont f = QApplication::font ( );
	f. setPixelSizeFloat ( QMAX( 14.f * d, 10.f ));
	setFont ( f );
	QFontMetrics fm ( f );

	QSize s ( 20 + fm. width ( "x" ) * 50, 10 + fm. height ( ) * 8 );

	QImage gradient = CUtility::createGradient ( QSize ( s. width ( ), s. height ( ) / 2 ), Qt::Vertical, QColor ( 255, 255, 255 ), QColor ( 160, 160, 160 ),  150.0f );

    QPixmap pix ( s );
    QPainter p (&pix);
	p. drawImage ( 0, 0, gradient );
	p. drawImage ( 0, s. height ( ) / 2, gradient. mirror ( false, true ));

	p. setPen ( QColor ( 232, 232, 232 ));
	for ( int i = s. height ( ) / 5; i < ( s. height ( ) - s. height ( ) / 5 + 5 ); i += s. height ( ) / 20 )
		p. drawLine ( 0, i, s. width ( ) - 1, i );
	p. setPen ( QColor ( 0, 0, 0 ));

	QFont f2 = f;
	f2. setPixelSize ( f2.pixelSize ( ) * 2 );
	f2. setBold ( true );
	p. setFont ( f2 );

	QPixmap logo = CResource::inst ( )-> pixmap ( "icon" );
	QString logo_text = cApp-> appName ( );

	QSize ts = p. boundingRect ( geometry ( ), Qt::AlignCenter, logo_text ). size ( );
	QSize ps = logo. size ( );

	int dx = ( s. width ( ) - ( ps. width ( ) + 8 + ts. width ( ))) / 2;
	int dy = s. height ( ) / 2;

	p. drawPixmap ( dx, dy - ps. height ( ) / 2, logo );
	p. drawText ( dx + ps. width ( ) + 8, dy - ts. height ( ) / 2, ts. width ( ), ts. height ( ), Qt::AlignCenter, logo_text );
	p. end ( );

	setPixmap ( pix );
}

CSplash::~CSplash ( )
{
	s_inst = 0;
	s_dont_show = true;
}

void CSplash::message ( const QString &msg )
{
    showMessage ( msg, Qt::AlignHCenter | Qt::AlignBottom );
    qApp-> processEvents();
}
