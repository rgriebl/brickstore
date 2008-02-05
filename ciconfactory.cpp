/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#include <qimage.h>

#include "cresource.h"
#include "ciconfactory.h"


CIconFactory::CIconFactory ( )
{
	setAutoDelete ( true );
	installDefaultFactory ( this );
	
	m_has_alpha = CResource::inst ( )-> pixmapAlphaSupported ( );
}

QPixmap *CIconFactory::createPixmap ( const QIconSet & iconSet, QIconSet::Size size, QIconSet::Mode mode, QIconSet::State state )
{
	if ( !m_has_alpha )
		return QIconFactory::createPixmap ( iconSet, size, mode, state );

	if ( mode == QIconSet::Normal )
		return 0;

	int af = ( mode == QIconSet::Disabled ? 3 : 1 );
	int vf = ( mode == QIconSet::Active ? 9 : 8 );


	QPixmap *p = new QPixmap ( iconSet. pixmap ( size, QIconSet::Normal, state ));

	QImage img = p-> convertToImage ( );
	img. setAlphaBuffer ( true );

	if ( img. depth ( ) != 32 )
		img = img. convertDepth ( 32 );

	for ( int y = 0; y < img. height ( ); y++ ) {
		QRgb *line = (QRgb *) img. scanLine ( y );

		for ( int x = 0; x < img. width ( ); x++ ) {
			int red   = QMIN( 255, qRed( line [x] ) * vf / 8 );
			int green = QMIN( 255, qGreen ( line [x] ) * vf / 8 );
			int blue  = QMIN( 255, qBlue ( line [x] ) * vf / 8 );
			int alpha = qAlpha ( line [x] ) / af;

			line [x] = qRgba ( red, green, blue, alpha );
		}
	}


	p-> convertFromImage ( img );

	return p;
}
