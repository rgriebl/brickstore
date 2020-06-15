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
#ifndef __CITEMTYPECOMBO_H__
#define __CITEMTYPECOMBO_H__

#include <qcombobox.h>
#include <qmap.h>

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
        m_map. clear ( );
        m_cb-> clear ( );

        for ( QHashIterator<int, BrickLink::ItemType *> it ( BrickLink::inst ( )-> itemTypes ( )); it. hasNext ( ); ) {
            it.next ( );
            if ( !onlywithinv || it. value ( )-> hasInventories ( ))
                m_map.insert(it.value ( )-> name ( ), it.value ( ));
		}

        foreach (QString name, m_map.keys()) {
            m_cb->addItem(name);
        }
	}

	void setCurrentItemType ( const BrickLink::ItemType *it )
	{
        m_cb->setCurrentItem(m_cb->findText(it->name()));
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
            return ( m_map[m_cb->itemText(i)]);
		else
			return 0;
    }

    QComboBox *m_cb;
    QMap<QString, BrickLink::ItemType*> m_map;
};

#endif
