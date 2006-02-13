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
#include <qfile.h>
#include <qbuffer.h>
#include <qfileinfo.h>
#include <qtextstream.h>
#include <qregexp.h>
#include <qdatetime.h>

#include "cutility.h"
#include "bricklink.h"
#include "cmessagebox.h"
#include "cconfig.h"


BrickLink::Inventory *BrickLink::inventory ( const Item *item )
{
	if ( !item )
		return 0;

	QCString key;
	key. sprintf ( "%c@%s", item-> itemType ( )-> id ( ), item-> id ( ));

	Inventory *inv = m_inventories. cache [key];

	if ( !inv ) {
		inv = new Inventory ( item );
		m_inventories. cache. insert ( key, inv );
	}

	if ( inv && ( !inv-> valid ( ) || ( inv-> lastUpdate ( ) < item-> inventoryUpdated ( ))))
		updateInventory ( inv );

	return inv;
}

BrickLink::Inventory::Inventory ( const Item *item )
{
	m_item = item;

	m_valid = false;
	m_update_status = Ok;

	load_from_disk ( );
}

BrickLink::Inventory::~Inventory ( )
{
	qDeleteAll ( m_list );
}

void BrickLink::Inventory::load_from_disk ( )
{
	if ( !m_item ) // shouldn't happen anyway...
		return;

	QString path = BrickLink::inst ( )-> dataPath ( m_item );

	if ( path. isEmpty ( ))
		return;

	// cleanup - version before 1.0.100 used txt files
	QFile::remove ( path + "inventory.txt" );

	path += "inventory.xml";

	m_valid = false;
	m_list. clear ( );

	QFile f ( path );
	if ( f. open ( IO_ReadOnly )) {
//   		uint invalid_items = 0;
		InvItemList *items = 0;

		QString emsg;
		int eline = 0, ecol = 0;
		QDomDocument doc;

		if ( doc. setContent ( &f, &emsg, &eline, &ecol )) {
			QDomElement root = doc. documentElement ( );

			items = BrickLink::inst ( )-> parseItemListXML ( doc. documentElement ( ). toElement ( ), BrickLink::XMLHint_Inventory /*, &invalid_items */);

			if ( items ) {
				m_list += *items;
				m_valid = true;
				delete items;

//				if ( invalid_items )
//					CMessageBox::information ( 0, BrickLink::tr( "This inventory contains %1 invalid item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));
			}
		}
	}

	if ( m_valid ) {
		QFileInfo fi ( path );
		m_fetched = fi. lastModified ( );
	}
}

void BrickLink::Inventory::update ( )
{
	BrickLink::inst ( )-> updateInventory ( this );
}

void BrickLink::updateInventory ( BrickLink::Inventory *inv )
{
	if ( !inv || ( inv-> m_update_status == Updating ))
		return;

	if ( !m_online ) {
		inv-> m_update_status = UpdateFailed;
		emit inventoryUpdated ( inv );
		return;
	}

	inv-> m_update_status = Updating;
	inv-> addRef ( );

	QCString url;
	CKeyValueList query;

	url = "http://www.bricklink.com/catalogDownload.asp";

	query << CKeyValue ( "a",            "a" )
	      << CKeyValue ( "viewType",     "4" )
	      << CKeyValue ( "itemTypeInv",  QChar ( inv-> item ( )-> itemType ( )-> id ( )))
	      << CKeyValue ( "itemNo",       inv-> item ( )-> id ( ))
	      << CKeyValue ( "downloadType", "X" );

	//qDebug ( "INV request started for %s", (const char *) url );
	m_inventories. transfer-> get ( url, query, 0, inv );
}

void BrickLink::inventoryJobFinished ( CTransfer::Job *j )
{
	if ( !j || !j-> data ( ) || !j-> userObject ( ))
		return;

	Inventory *inv = static_cast <Inventory *> ( j-> userObject ( ));

	inv-> m_update_status = UpdateFailed;

	if ( !j-> failed ( )) {
		QString path = BrickLink::inst ( )-> dataPath ( inv-> m_item );

		bool ok = false;

		if ( !path. isEmpty ( )) {
			path += "inventory.xml";

			QFile f ( path );
			if ( j-> data ( )-> size ( ) && f. open ( IO_WriteOnly )) {
				ok = ( f. writeBlock ( *j-> data ( )) == Q_LONG( j-> data ( )-> size ( )));

				f. close ( );

				if ( ok )
					inv-> load_from_disk ( );
			}
		}
		inv-> m_fetched = QDateTime ( );
		inv-> m_update_status = Ok;
	}

	emit inventoryUpdated ( inv );
	inv-> release ( );
}

