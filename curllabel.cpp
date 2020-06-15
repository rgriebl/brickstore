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
#include <q3simplerichtext.h>
#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>

#include "cutility.h"
#include "curllabel.h"


CUrlLabel::CUrlLabel ( QWidget *parent, const char *name )
    : Q3TextBrowser ( parent, name )
{
	QPalette pal = palette ( );
	pal. setColor ( QPalette::Active,   QColorGroup::Text, pal. color ( QPalette::Active,   QColorGroup::Foreground ));
	pal. setBrush ( QPalette::Active,   QColorGroup::Base, pal. brush ( QPalette::Active,   QColorGroup::Background ));
	pal. setColor ( QPalette::Inactive, QColorGroup::Text, pal. color ( QPalette::Inactive, QColorGroup::Foreground ));
	pal. setBrush ( QPalette::Inactive, QColorGroup::Base, pal. brush ( QPalette::Inactive, QColorGroup::Background ));
	pal. setColor ( QPalette::Disabled, QColorGroup::Text, pal. color ( QPalette::Disabled, QColorGroup::Foreground ));
	pal. setBrush ( QPalette::Disabled, QColorGroup::Base, pal. brush ( QPalette::Disabled, QColorGroup::Background ));
	setPalette ( pal );

    setFocusPolicy( Qt::NoFocus );
	
	setTextFormat ( Qt::RichText );
	setFrameStyle ( NoFrame );
    setHScrollBarMode ( AlwaysOff );
    setVScrollBarMode ( AlwaysOff );

	if ( qApp-> mainWidget ( ) && qApp-> mainWidget ( )-> inherits ( "QMainWindow" ))
        connect ( this, SIGNAL( highlighted ( const QString & )), static_cast <QMainWindow *> ( qApp-> mainWidget ( ))-> statusBar ( ), SLOT( message ( const QString & )));
}

CUrlLabel::~CUrlLabel ( )
{
}

void CUrlLabel::setSource ( const QString &src )
{
	if ( src. find ( ':' ) > 0 )
		CUtility::openUrl ( src );
    else
        Q3TextBrowser::setSource ( src );
}

QSize CUrlLabel::sizeHint ( ) const
{
	return minimumSizeHint ( );
}

QSize CUrlLabel::minimumSizeHint ( ) const
{
	QSize ms = minimumSize ( );
	if (( ms. width ( ) > 0 ) && ( ms. height ( ) > 0 ))
		return ms;
	
	int w = 500;
	if ( ms. width ( ) > 0 )
		w = ms. width ( );
	
    Q3SimpleRichText rt ( text ( ), font ( ));
	rt. setWidth ( w - 2 * frameWidth ( ) - 10 );
	
	w = 10 + rt. widthUsed ( ) + 2 * frameWidth ( );
	if ( w < ms. width ( ))
		w = ms. width ( );
	
	int h = rt. height ( ) + 2 * frameWidth ( );
	if ( h < ms. height ( ))
		h = ms. height ( );
	
	return QSize ( w, h );
}
