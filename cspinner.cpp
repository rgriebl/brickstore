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
#include <qpixmap.h>
#include <qpainter.h>

#include "cspinner.h"


CSpinner::CSpinner ( QWidget *parent, const char *name, WFlags fl )
	: QWidget ( parent, name, fl )
{
	m_step = 0;
	m_width = 0;
	m_count = 0;
	m_pixmap = new QPixmap ( );
	m_timer_id = 0;
}

CSpinner::~CSpinner ( )
{
	stop ( );
	delete m_pixmap;
}

void CSpinner::setPixmap ( const QPixmap &pix )
{
	if ( pix. isNull ( ) || ( pix. height ( ) % pix. width ( ) != 0 ))
		return;

	bool act = isActive ( );

	stop ( );

	*m_pixmap = pix;
	m_width = pix. width ( );
	m_count = pix. height ( ) / m_width;
	m_step = 0;

	setFixedSize ( minimumSizeHint ( ));

	if ( act )
		start ( );
}

void CSpinner::setActive ( bool b )
{
	if ( b != isActive ( )) {
		if ( b ) {
			m_timer_id = startTimer ( 150 );
		}
		else {
			killTimer ( m_timer_id );
			m_timer_id = 0;
		}
		m_step = 0;
		repaint ( );
	}
}

void CSpinner::start ( )
{
	setActive ( true );
}

void CSpinner::stop ( )
{
	setActive ( false );
}

bool CSpinner::isActive ( ) const
{
	return ( m_timer_id != 0 );
}

QSize CSpinner::minimumSizeHint ( ) const
{
	return QSize ( m_width, m_width );
}

void CSpinner::timerEvent ( QTimerEvent *te )
{
	if ( te-> timerId ( ) == m_timer_id ) {
		m_step++;
		if ( m_step >= m_count )
			m_step = 0;

		repaint ( );
	}
}

void CSpinner::paintEvent ( QPaintEvent * )
{
	if ( !m_pixmap-> isNull ( )) {
		QPainter p ( this );

		p. drawPixmap ( 0, 0, *m_pixmap, 0, m_step * m_width, m_width, m_width );
	}
}


