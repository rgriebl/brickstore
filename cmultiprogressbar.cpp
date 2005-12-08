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

#include <qprogressbar.h>
#include <qlabel.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qgrid.h>
#include <qlayout.h>
#include <qimage.h>

#include "cmultiprogressbar.h"


CMultiProgressBar::CMultiProgressBar ( QWidget *parent, const char *name )
	: QWidget ( parent, name )
{
	m_align = Qt::AlignRight | Qt::AlignTop;
	m_autoid = -1;

	QBoxLayout *lay = new QHBoxLayout ( this, 0, 0 );

	m_arrow = new QToolButton ( this );
	QToolTip::add ( m_arrow, tr( "View detailed progress information" ));
	m_arrow-> setAutoRaise ( true );
	m_arrow-> setToggleButton ( true );
	m_arrow-> hide ( );
	lay-> addWidget ( m_arrow );

	m_progress = new QProgressBar ( this );
	m_progress-> setFrameStyle ( QFrame::NoFrame );
    m_progress-> setIndicatorFollowsStyle ( false );
    m_progress-> setCenterIndicator ( true );
	m_progress-> setPercentageVisible ( false );
	lay-> addWidget ( m_progress );

	m_stop = new QToolButton ( this );
	QToolTip::add ( m_stop, tr( "Cancel all active transfers" ));
	m_stop-> setAutoRaise ( true );
	m_stop-> setAutoRepeat ( false );
	m_stop-> hide ( );
	lay-> addWidget ( m_stop );


	m_popup = new QGrid ( 2, 0, "CMultiProgressBar-Popup", WType_Popup );
	m_popup-> setFrameStyle ( QFrame::PopupPanel | QFrame::Raised );
	m_popup-> setMargin ( 4 );
	m_popup-> setSpacing ( 6 );

	connect ( m_arrow, SIGNAL( toggled ( bool )), this, SLOT( togglePopup ( bool )));
	connect ( m_stop, SIGNAL( clicked ( )), this, SIGNAL( stop ( )));

	m_popup-> installEventFilter ( this );
	recalc ( );
}

CMultiProgressBar::~CMultiProgressBar ( )
{
	m_items. setAutoDelete ( true );
	m_items. clear ( );

	delete m_popup;
}

void CMultiProgressBar::setPopupPixmap ( const QPixmap &pix )
{
	m_arrow_pix = pix;
	m_arrow-> setHidden ( pix. isNull ( ));

	recalcPixmap ( m_arrow, m_arrow_pix );
}

void CMultiProgressBar::setStopPixmap ( const QPixmap &pix )
{
	m_stop_pix = pix;
	m_stop-> setHidden ( pix. isNull ( ));

	recalcPixmap ( m_stop, m_stop_pix );
}

void CMultiProgressBar::resizeEvent ( QResizeEvent * )
{
	m_arrow-> setFixedSize ( height ( ), height ( ));
	m_stop-> setFixedSize ( height ( ), height ( ));

	recalcPixmap ( m_arrow, m_arrow_pix );
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

	for ( QIntDictIterator <ItemData> it ( m_items ); it. current ( ); ++it ) {
		ItemData *p = it. current ( );

		int tp = p-> m_progress-> totalSteps ( );
		int pp = p-> m_progress-> progress ( );

		p-> m_label-> setShown ( tp > 0 );
		p-> m_progress-> setShown ( tp > 0 );

		ta += tp;
		pa += pp;
	}
	if ( ta > 0 ) {
		m_progress-> setProgress ( pa, ta );
	}
	else {
		m_progress-> reset ( );
		m_popup-> hide ( );
		m_arrow-> setOn ( false );
	}
	m_arrow-> setEnabled ( ta > 0 );
	m_stop-> setEnabled ( ta > 0 );
}

void CMultiProgressBar::togglePopup ( bool b )
{
	if ( !b ) {
		m_popup-> hide ( );
	}
	else /*if ( m_progress-> totalSteps ( ) > 0 )*/ {
		QSize ps = m_popup-> sizeHint ( );
		if ( ps. width ( ) < width ( ))
			ps. setWidth ( width ( ));

		m_popup-> setFixedSize ( ps );

		QPoint p = mapToGlobal ( QPoint ( 0, 0 ));

		switch ( m_align & Qt::AlignHorizontal_Mask ) {
			case Qt::AlignLeft   : break;
			case Qt::AlignHCenter: p. rx ( ) += (( width ( ) - ps. width ( )) / 2 ); break;
			case Qt::AlignRight  : p. rx ( ) += ( width ( ) - ps. width ( )); break;
		}

		switch ( m_align & Qt::AlignVertical_Mask ) {
			case Qt::AlignTop    : p. ry ( ) -= m_popup-> height ( ); break;
			case Qt::AlignVCenter: p. ry ( ) += (( height ( ) - ps. height ( )) / 2 ); break;
			case Qt::AlignBottom : p. ry ( ) = ( height ( )); break;
		}

		m_popup-> move ( p );
		m_popup-> show ( );
	}
	if ( m_arrow-> isOn ( ) != b )
 	m_arrow-> setOn ( b );
}

bool CMultiProgressBar::eventFilter ( QObject *o, QEvent *e )
{
	if (( o == m_popup ) && ( e-> type ( ) == QEvent::MouseButtonPress ))
		togglePopup ( false );

	return QWidget::eventFilter ( o, e );
}

void CMultiProgressBar::setPopupAlignment ( int align )
{
	m_align = align;
}

int CMultiProgressBar::popupAlignment ( ) const
{
	return m_align;
}

CMultiProgressBar::ItemData::ItemData ( QWidget *parent, const QString &label )
{
	m_label = new QLabel ( label, parent );
	m_progress = new QProgressBar ( parent );
	m_progress-> setProgress ( 0, 0 );
}

CMultiProgressBar::ItemData::~ItemData ( )
{
	delete m_label;
	delete m_progress;
}

int CMultiProgressBar::addItem ( const QString &label, int id )
{
	if ( id < 0 )
		id = --m_autoid;

	ItemData *p = new ItemData ( m_popup, label );
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

	return p ? p-> m_label-> text ( ) : QString::null;
}


int CMultiProgressBar::itemProgress ( int id ) const
{
	ItemData *p = m_items [id];

	return p ? p-> m_progress-> progress ( ) : -1;
}

int CMultiProgressBar::itemTotalSteps ( int id ) const
{
	ItemData *p = m_items [id];

	return p ? p-> m_progress-> totalSteps ( ) : -1;
}


void CMultiProgressBar::setItemProgress ( int id, int progress )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_progress-> setProgress ( progress );
		recalc ( );
	}
}

void CMultiProgressBar::setItemProgress ( int id, int progress, int total )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_progress-> setProgress ( progress, total );
		recalc ( );
	}
}

void CMultiProgressBar::setItemTotalSteps ( int id, int total )
{
	ItemData *p = m_items [id];

	if ( p ) {
		p-> m_progress-> setTotalSteps ( total );
		recalc ( );
	}
}


void CMultiProgressBar::setItemLabel ( int id, const QString &label )
{
	ItemData *p = m_items [id];

	if ( p )
		p-> m_label-> setText ( label );
}

