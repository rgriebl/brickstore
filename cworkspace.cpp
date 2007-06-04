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
#include <stdlib.h>

#include <QLayout>
#include <QStackedLayout>
#include <QTabBar>
#include <QEvent>

//#include "cresource.h"

#include "cworkspace.h"


#if defined( Q_WS_WIN )
#include <QWindowsXPStyle>
#include <QWindowsStyle>
#endif


CWorkspace::CWorkspace ( QWidget *parent, Qt::WindowFlags f )
	: QWidget ( parent, f )
{
	m_tabmode = TopTabs;

	m_verticallayout = new QVBoxLayout ( this );
	m_verticallayout-> setMargin ( 0 );
	m_verticallayout-> setSpacing ( 0 );

	m_tabbar = new QTabBar ( this );
	m_tabbar-> setElideMode ( Qt::ElideMiddle );
	m_tabbar-> hide ( );

	m_tablayout = new QHBoxLayout ( );
	m_tablayout-> addSpacing ( 4 );
	m_tablayout-> addWidget ( m_tabbar );
	m_tablayout-> addSpacing ( 4 );
	m_verticallayout-> addLayout ( m_tablayout );
	
	m_stacklayout = new QStackedLayout ( m_verticallayout );

	relayout ( );

	connect ( m_tabbar, SIGNAL( currentChanged ( int )), m_stacklayout, SLOT( setCurrentIndex ( int )));
	connect ( m_stacklayout, SIGNAL( currentChanged ( int )), this, SLOT( currentChangedHelper ( int )));
	connect ( m_stacklayout, SIGNAL( widgetRemoved ( int )), this, SLOT( removeWindow ( int )));
}


void CWorkspace::relayout ( )
{
	switch ( m_tabmode ) {
	case BottomTabs:
		m_verticallayout-> removeItem ( m_tablayout );
		m_tabbar-> setShape ( QTabBar::TriangularSouth );
		m_verticallayout-> addLayout ( m_tablayout );
		break;
	
	default:	
	case TopTabs:
		m_verticallayout-> removeItem ( m_stacklayout );
		m_tabbar-> setShape ( QTabBar::RoundedNorth );
		m_verticallayout-> addLayout ( m_stacklayout );
		break;
	}	

	if ( m_stacklayout-> count ( ) > 1 )
		m_tabbar-> show ( );
}


void CWorkspace::addWindow ( QWidget *w )
{
	if ( !w )
		return;

	m_tabbar-> addTab ( w-> windowTitle ( ));
	m_stacklayout-> addWidget ( w );

	w-> installEventFilter ( this );

	m_tabbar-> setShown ( m_stacklayout-> count ( ) > 1 );
}

void CWorkspace::currentChangedHelper ( int id )
{
	emit currentChanged ( m_stacklayout-> widget ( id ));
}

void CWorkspace::setCurrentWidget ( QWidget *w )
{
	m_tabbar-> setCurrentIndex ( m_stacklayout-> indexOf ( w ));
}

void CWorkspace::removeWindow ( int id )
{
	m_tabbar-> removeTab ( id );
	m_tabbar-> setShown ( m_stacklayout-> count ( ) > 1 );
}

CWorkspace::TabMode CWorkspace::tabMode ( ) const
{
	return m_tabmode;
}

void CWorkspace::setTabMode ( TabMode tm )
{
	if ( tm != m_tabmode ) {
		m_tabmode = tm;
		relayout ( );
	}
}

bool CWorkspace::eventFilter( QObject *o, QEvent *e )
{
	QWidget *w = qobject_cast <QWidget *> ( o );

	if ( w && ( e-> type ( ) == QEvent::WindowTitleChange ))
		m_tabbar-> setTabText ( m_stacklayout-> indexOf ( w ), w-> windowTitle ( ));

	return QWidget::eventFilter ( o, e );
}

QList <QWidget *> CWorkspace::allWindows ( )
{
	QList <QWidget *> res;

	for ( int i = 0; i < m_stacklayout-> count ( ); i++ )
		res << m_stacklayout-> widget ( i );
	
	return res;
}

QWidget *CWorkspace::activeWindow ( )
{
	return m_stacklayout-> currentWidget ( );
}
