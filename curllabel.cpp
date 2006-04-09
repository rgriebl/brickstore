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
#include <qsimplerichtext.h>
#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>

#include "cutility.h"
#include "curllabel.h"


CUrlLabel::CUrlLabel ( QWidget *parent, const char *name )
	: QTextBrowser ( parent, name )
{
	QPalette pal = palette ( );
	pal. setColor ( QColorGroup::Text, pal. color ( QPalette::Active, QColorGroup::Foreground ));
	pal. setBrush ( QColorGroup::Base, pal. brush ( QPalette::Active, QColorGroup::Background ));
	setPalette ( pal );

	setFocusPolicy( NoFocus );
	
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
		QTextBrowser::setSource ( src );
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
	
	QSimpleRichText rt ( text ( ), font ( ));
	rt. setWidth ( w - 2 * frameWidth ( ) - 10 );
	
	w = 10 + rt. widthUsed ( ) + 2 * frameWidth ( );
	if ( w < ms. width ( ))
		w = ms. width ( );
	
	int h = rt. height ( ) + 2 * frameWidth ( );
	if ( h < ms. height ( ))
		h = ms. height ( );
	
	return QSize ( w, h );
}
