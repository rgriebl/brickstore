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

#include <q3progressbar.h>
#include <qlabel.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <q3grid.h>
#include <qlayout.h>
#include <qimage.h>
//Added by qt3to4:
#include <QPixmap>
#include <QResizeEvent>
#include <Q3BoxLayout>
#include <Q3HBoxLayout>
#include <Q3Frame>

#include "cmultiprogressbar.h"


CMultiProgressBar::CMultiProgressBar ( QWidget *parent, const char *name )
	: QWidget ( parent, name )
{
	m_autoid = -1;

	Q3BoxLayout *lay = new Q3HBoxLayout ( this, 0, 0 );

	m_progress = new Q3ProgressBar ( this );
	m_progress-> setFrameStyle ( Q3Frame::NoFrame );
    //m_progress-> setIndicatorFollowsStyle ( false );
	m_progress-> setCenterIndicator ( true );
	m_progress-> setPercentageVisible ( false );
	lay-> addWidget ( m_progress, 1 );
	lay-> addStretch ( 0 );

	m_stop = new QToolButton ( this );
	m_stop-> setAutoRaise ( true );
	m_stop-> setAutoRepeat ( false );
	m_stop-> hide ( );
	lay-> addWidget ( m_stop, 0 );

	connect ( m_stop, SIGNAL( clicked ( )), this, SIGNAL( stop ( )));
	recalc ( );

	languageChange ( );
}

void CMultiProgressBar::languageChange ( )
{
	QToolTip::add ( m_stop, tr( "Cancel all active transfers" ));
}

CMultiProgressBar::~CMultiProgressBar ( )
{
	m_items. setAutoDelete ( true );
	m_items. clear ( );
}

void CMultiProgressBar::setStopPixmap ( const QPixmap &pix )
{
	m_stop_pix = pix;
	m_stop-> setHidden ( pix. isNull ( ));

	recalcPixmap ( m_stop, m_stop_pix );
}

void CMultiProgressBar::resizeEvent ( QResizeEvent * )
{
	m_stop-> setFixedSize ( height ( ), height ( ));

	recalcPixmap ( m_stop, m_stop_pix );
}

void CMultiProgressBar::recalcPixmap ( QToolButton *but, const QPixmap &pix )
{
	int h = but-> height ( ) - 2;

	QPixmap pnew;
	pnew. convertFromImage ( pix. convertToImage ( ). smoothScale ( pix. size ( ). boundedTo ( QSize ( h, h ))));

	but-> setPixmap ( pnew );
}


void CMultiProgressBar::recalc ( )
{
	int ta = 0, pa = 0;
	QStringList sl;
	QString str;

	for ( Q3IntDictIterator <ItemData> it ( m_items ); it. current ( ); ++it ) {
		ItemData *p = it. current ( );

		ta += p-> m_total;
		pa += p-> m_progress;

		if ( p-> m_total > 0 ) {
			sl << QString( "<tr><td><b>%1:</b></td><td align=\"right\">%2%</td><td>(%3/%4)</td></tr>" ). 
				arg( p-> m_label ). arg( int( 100.f * float( p-> m_progress ) / float( p-> m_total ))). 
			    arg( p-> m_progress ). arg( p-> m_total );
		}
	}
	if ( ta > 0 ) {
		m_progress-> setProgress ( pa, ta );
		str = "<table>" + sl. join ( "" ) + "</table>";

		if ( !m_progress-> isVisible ( ))
			m_progress-> show ( );

		QToolTip::add ( m_progress, str );
	}
	else {
		m_progress-> hide ( );
		m_progress-> reset ( );
	}

	m_stop-> setEnabled ( ta > 0 );
	emit statusChange ( ta > 0 );
}

CMultiProgressBar::ItemData::ItemData ( const QString &label )
{
	m_label = label;
	m_progress = 0;
	m_total = 0;
}

int CMultiProgressBar::addItem ( const QString &label, int id )
{
	if ( id < 0 )
		id = --m_autoid;

	ItemData *p = new ItemData ( label );
	m_items. insert ( id, p );

	recalc ( );
	return id;
}

void CMultiProgressBar::removeItem ( int id )
{
	ItemData *p = m_items [id];

	if ( p ) {
		m_items. remove ( id );
		delete p;
		recalc ( );
	}
}

QString CMultiProgressBar::itemLabel ( int id ) const
{
	ItemData *p = m_items [id];

	return p ? p-> m_label : QString::null;
}


int CMultiProgressBar::itemProgress ( int id ) const
{
	ItemData *p = m_items [id];

	return p ? p-> m_progress : -1;
}

int CMultiProgressBar::itemTotalSteps ( int id ) const
{
	ItemData *p = m_items [id];

	return p ? p-> m_total : -1;
}


void CMultiProgressBar::setItemProgress ( int id, int progress )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_progress = progress;
		recalc ( );
	}
}

void CMultiProgressBar::setItemProgress ( int id, int progress, int total )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_progress = progress;
		p-> m_total = total;
		recalc ( );
	}
}

void CMultiProgressBar::setItemTotalSteps ( int id, int total )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_total = total;
		recalc ( );
	}
}


void CMultiProgressBar::setItemLabel ( int id, const QString &label )
{
	ItemData *p = m_items [id];

	if ( p )
		p-> m_label = label;
}

