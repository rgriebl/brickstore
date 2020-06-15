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
#include <qstringlist.h>
#include <q3popupmenu.h>
//Added by qt3to4:
#include <Q3ActionGroup>
#include <Q3ValueList>

#include "clistaction.h"


CListAction::CListAction ( bool use_numbers, QWidget *parent, const char *name )
    : QMenu ( name, parent )
{
    setObjectName( name );
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

void CListAction::refreshMenu ( )
{
	const QObject *o = sender ( );

    if ( o-> inherits ( "QMenu" )) {
        QMenu *sub = static_cast <QMenu *> ( const_cast <QObject *> ( o ));
        sub-> clear ( );

		int active = -1;
		Q3ValueList <int> custom_ids;
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
			}
		}
	}
}
