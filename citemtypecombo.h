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
#ifndef __CITEMTYPECOMBO_H__
#define __CITEMTYPECOMBO_H__

#include <qcombobox.h>
#include <qlistbox.h>

#include "bricklink.h"

class CItemTypeCombo : public QObject {
	Q_OBJECT

public:
	CItemTypeCombo ( QComboBox *cb, bool onlywithinv = false )
		: QObject ( cb ), m_cb ( cb )
	{
		connect ( cb, SIGNAL( activated ( int )), this, SLOT( cbActivated ( int )));
		reset ( onlywithinv );
	}

	void reset ( bool onlywithinv = false )
	{
		m_cb-> listBox ( )-> clear ( );

		for ( QIntDictIterator<BrickLink::ItemType> it ( BrickLink::inst ( )-> itemTypes ( )); it. current ( ); ++it ) {
			if ( !onlywithinv || it. current ( )-> hasInventories ( ))
				(void) new MyListItem ( m_cb, it. current ( ));
		}
		m_cb-> listBox ( )-> sort ( );
	}

	void setCurrentItemType ( const BrickLink::ItemType *it )
	{
		for ( uint i = 0; i < m_cb-> listBox ( )-> count ( ); i++ ) {
			if ( index2type ( i ) == it ) {
				m_cb-> setCurrentItem ( i );
				break;
			}
		}
	}

	const BrickLink::ItemType *currentItemType ( ) const
	{
		return index2type ( m_cb-> currentItem ( ));
	}

signals:
	void itemTypeActivated ( const BrickLink::ItemType * );

private slots:
	void cbActivated ( int i )
	{
		emit itemTypeActivated ( index2type ( i ));
	}

private:
	const BrickLink::ItemType *index2type ( int i ) const
	{
		if (( i >= 0 ) && ( i < m_cb-> count ( )))
			return ( static_cast <MyListItem *> ( m_cb-> listBox ( )-> item ( i ))-> itemType ( ));
		else
			return 0;
	}


	class MyListItem : public QListBoxText {
	public:
		MyListItem ( QComboBox *combo, const BrickLink::ItemType *it )
			: QListBoxText ( combo-> listBox ( ), it-> name ( )), m_item_type ( it ) { }

		const BrickLink::ItemType *itemType ( ) { return m_item_type; }

	private:
		const BrickLink::ItemType *m_item_type;
	};

	QComboBox *m_cb;
};

#endif
