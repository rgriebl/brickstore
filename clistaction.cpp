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
#include <qstringlist.h>
#include <qpopupmenu.h>

#include "clistaction.h"


CListAction::CListAction ( bool use_numbers, QObject *parent, const char *name )
	: QActionGroup ( parent, name )
{
	m_use_numbers = use_numbers;
	m_list = 0;
	m_provider = 0;
}

CListAction::~CListAction ( )
{
	if ( m_provider )
		delete m_provider;
}

void CListAction::setListProvider ( Provider *prov )
{
	m_provider = prov;
	m_list = 0;
}

CListAction::Provider *CListAction::listProvider ( ) const
{
	return m_provider;
}

void CListAction::setList ( const QStringList *list )
{
	m_list = list;
	m_provider = 0;
}

const QStringList *CListAction::list ( ) const
{
	return m_list;
}

bool CListAction::addTo ( QWidget *w )
{
	if ( w-> inherits ( "QPopupMenu" )) {
		QPopupMenu *sub;

		if ( usesDropDown ( )) {
			sub = new QPopupMenu ( w );
			static_cast <QPopupMenu *> ( w )-> insertItem ( menuText ( ), sub );
		}
		else {
			sub = static_cast <QPopupMenu *> ( w );
			m_id_map [sub]. clear ( );
		}

		connect ( sub, SIGNAL( aboutToShow ( )), this, SLOT( refreshMenu ( )));
		return true;
	}
	return false;
}

bool CListAction::removeFrom ( QWidget *w )
{
	if ( w-> inherits ( "QPopupMenu" ))
		m_id_map. erase ( static_cast <QPopupMenu *> ( w ));
	return true;
}

void CListAction::refreshMenu ( )
{
	const QObject *o = sender ( );

	if ( o-> inherits ( "QPopupMenu" )) {
		QPopupMenu *sub = static_cast <QPopupMenu *> ( const_cast <QObject *> ( o ));

		bool dd = usesDropDown ( );

		if ( dd )
			sub-> clear ( );
		else {
			QValueVector<int> &v = m_id_map [sub];

			for ( QValueVector<int>::iterator it = v. begin ( ); it != v. end ( ); ++it )
				sub-> removeItem ( *it );
			v. clear ( );
		}

		int active = -1;
		QValueList <int> custom_ids;
		QStringList sl = m_list ? *m_list : ( m_provider ? m_provider-> list ( active, custom_ids ) : QStringList ( ));

		if ( !sl. isEmpty ( )) {
			int i = 0;
			for ( QStringList::ConstIterator it = sl. begin ( ); it != sl. end ( ); ++it, i++ ) {
				QString s;

				if ( m_use_numbers )
					s = QString ( "&%1   " ). arg ( i+1 );
				s += *it;

				int id = i;
				if ( custom_ids. count ( ) == sl. count ( ))
					id = custom_ids [i];

				id = sub-> insertItem ( s, this, SIGNAL( activated ( int )), 0, id );

				if ( i == active )
					sub-> setItemChecked ( i, true );

				if ( !dd )
					m_id_map [sub]. push_back ( id );
			}
		}
	}
}
