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

namespace {

static bool convert_inv_peeron_to_bricklink ( QIODevice *peeron, QFile *bl )
{
	if ( !peeron || !bl )
		return false;

	QTextStream in ( peeron );

	QDomDocument doc ( QString::null );
	QDomElement root = BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_Inventory, 0 );
	doc. appendChild ( root );

	QString line;
	bool next_is_item = false;
	int count = 0;

	QRegExp itempattern ( "<a href=[^>]+>(.+)</a>" );

	while (( line = in. readLine ( ))) {
		if ( next_is_item && line. startsWith ( "<td>" ) && line. endsWith ( "</td>" )) {
			QString tmp = line. mid ( 4, line. length ( ) - 9 );
			QStringList sl = QStringList::split ( "</td><td>", tmp, true );

			bool line_ok = false;

			if ( sl. count ( ) >= 3 ) {
				int qty, colorid;
				QString itemid, itemname, colorname;
				
				qty = sl [0]. toInt ( );

				if ( itempattern. exactMatch ( sl [1] ))
					itemid = itempattern. cap ( 1 );

				colorname = sl [2];
				const BrickLink::Color *color = BrickLink::inst ( )-> colorFromPeeronName ( colorname );
				colorid = color ? int( color-> id ( )) : -1;

				itemname = sl [3];

				int pos = itemname. find ( " <" );
				if ( pos > 0 )
					itemname. truncate ( pos );

				if ( qty > 0 ) {
					QDomElement item = doc. createElement ( "ITEM" );
					root. appendChild ( item );

					// <ITEMNAME> and <COLORNAME> are inofficial extension 
					// to help with incomplete items in Peeron inventories.

					item. appendChild ( doc. createElement ( "ITEMTYPE"  ). appendChild ( doc. createTextNode ( "P" )). parentNode ( ));
					item. appendChild ( doc. createElement ( "ITEMID"    ). appendChild ( doc. createTextNode ( itemid )). parentNode ( ));
					item. appendChild ( doc. createElement ( "ITEMNAME"  ). appendChild ( doc. createTextNode ( itemname )). parentNode ( ));
					item. appendChild ( doc. createElement ( "COLOR"     ). appendChild ( doc. createTextNode ( QString::number ( colorid ))). parentNode ( ));
					item. appendChild ( doc. createElement ( "COLORNAME" ). appendChild ( doc. createTextNode ( colorname )). parentNode ( ));
					item. appendChild ( doc. createElement ( "QTY"       ). appendChild ( doc. createTextNode ( QString::number ( qty ))). parentNode ( ));

					line_ok = true;
					count++;
				}
			}
			if ( !line_ok )
				qWarning ( "Failed to parse item line: %s", line. latin1 ( ));


			next_is_item = false;
		}

		if (( line == "<tr bgcolor=\"#dddddd\">" ) || ( line == "<tr bgcolor=\"#eeeeee\">" ))
			next_is_item = true;
		else
			next_is_item = false;
	}

	if ( count ) {
		QCString output = doc. toCString ( );
		return ( bl-> writeBlock ( output. data ( ), output. size ( ) - 1 ) ==  int( output. size ( ) - 1 )); // no 0-byte
	}
	else
		return false;
}

} // namespace


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

BrickLink::Inventory *BrickLink::inventoryFromPeeron ( const Item *item )
{
	if ( !item )
		return 0;

	QCString key;
	key. sprintf ( "%c@%s", item-> itemType ( )-> id ( ), item-> id ( ));

	Inventory *inv = m_inventories. cache [key];

	if ( !inv ) {
		inv = new Inventory ( item, true );
		m_inventories. cache. insert ( key, inv );
	}

	updateInventory ( inv );

	return inv;
}

BrickLink::Order *BrickLink::order ( const QString &id, Order::Type type )
{
	if (( id. length ( ) != 6 ) || ( type != Order::Received && type != Order::Placed ))
		return 0;

	// no caching !
	Order *ord = new Order ( id, type );

	if ( ord )
		updateInventory ( ord );

	return ord;
}

BrickLink::Inventory *BrickLink::storeInventory ( )
{
	// no caching !
	Inventory *inv = new Inventory ( );

	if ( inv )
		updateInventory ( inv );

	return inv;
}

BrickLink::Inventory::Inventory ( const Item *item, bool from_peeron )
{
	m_item = item;

	m_valid = false;
	m_update_status = Ok;
	m_from_peeron = from_peeron;

	load_from_disk ( );
}

BrickLink::Order::Order ( const QString &orderid, Type ordertype )
	: Inventory ( )
{
	m_id = orderid;
	m_type   = ordertype;
}

BrickLink::Inventory::Inventory ( )
{
	m_item = 0;

	m_valid = false;
	m_update_status = Ok;
	m_from_peeron = false;
}

BrickLink::Inventory::~Inventory ( )
{
	qDeleteAll ( m_list );
}

BrickLink::Order::~Order ( )
{
}

void BrickLink::Inventory::load_from_disk ( )
{
	if ( isOrder ( ) || !m_item ) // shouldn't happen anyway...
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

	if ( inv-> m_item && !inv-> isOrder ( )) {
		if ( inv-> m_from_peeron ) {
			url. sprintf ( "http://www.peeron.com/inv/sets/%s", inv-> item ( )-> id ( ));
		}
		else {
			url = "http://www.bricklink.com/catalogDownload.asp";

			query << CKeyValue ( "a",            "a" )
			      << CKeyValue ( "viewType",     "4" )
			      << CKeyValue ( "itemTypeInv",  QChar ( inv-> item ( )-> itemType ( )-> id ( )))
			      << CKeyValue ( "itemNo",       inv-> item ( )-> id ( ))
			      << CKeyValue ( "downloadType", "X" );
		}
		//qDebug ( "INV request started for %s", (const char *) url );
		m_inventories. transfer-> get ( url, query, 0, inv );
	}
	else if ( inv-> isOrder ( )) {
		Order *ord = static_cast <Order *> ( inv );

		url = "http://www.bricklink.com/orderExcelFinal.asp";

		query << CKeyValue ( "action",        "save" )
		      << CKeyValue ( "orderID",       ord-> id ( ))
			  << CKeyValue ( "orderType",     ord-> type ( ) == Order::Placed ? "placed" : "received" )
		      << CKeyValue ( "viewType",      "X" )    // XML
		      << CKeyValue ( "getDateFormat", "1" )    // YYYY/MM/DD
		      << CKeyValue ( "getOrders",     "" )     // regardless of date
		      << CKeyValue ( "fDD",           "1" )
		      << CKeyValue ( "fMM",           "1" )
		      << CKeyValue ( "fYY",           "2005" )
		      << CKeyValue ( "tDD",           "2" )
		      << CKeyValue ( "tMM",           "1" )
		      << CKeyValue ( "tYY",           "2005" )
		      << CKeyValue ( "getDetail",     "y" )    // get items (that's why we do this in the first place...)
		      << CKeyValue ( "getFiled",      "Y" )    // regardless of filed state
		      << CKeyValue ( "getStatus",     "" )     // regardless of status
		      << CKeyValue ( "statusID",      "" )
		      << CKeyValue ( "frmUsername",   CConfig::inst ( )-> blLoginUsername ( ))
		      << CKeyValue ( "frmPassword",   CConfig::inst ( )-> blLoginPassword ( ));

		//qDebug ( "ORD request started for %s", (const char *) url );
		m_inventories. transfer-> post ( url, query, 0, inv );
	}
	else {
		url = "http://www.bricklink.com/invExcelFinal.asp"; 

		query << CKeyValue ( "itemType",      "" )
		      << CKeyValue ( "catID",         "" )
		      << CKeyValue ( "colorID",       "" )
		      << CKeyValue ( "invNew",        "" )
		      << CKeyValue ( "itemYear",      "" )
		      << CKeyValue ( "viewType",      "x" )    // XML 
		      << CKeyValue ( "invStock",      "Y" )   
		      << CKeyValue ( "invStockOnly",  "" )
			  << CKeyValue ( "invQty",        "" )
		      << CKeyValue ( "invQtyMin",     "0" )
		      << CKeyValue ( "invQtyMax",     "0" )
		      << CKeyValue ( "invBrikTrak",   "" )
		      << CKeyValue ( "invDesc",       "" )
		      << CKeyValue ( "frmUsername",   CConfig::inst ( )-> blLoginUsername ( ))
		      << CKeyValue ( "frmPassword",   CConfig::inst ( )-> blLoginPassword ( ));

		//qDebug ( "STR request started for %s", (const char *) url );
		m_inventories. transfer-> post ( url, query, 0, inv );
	}
}

namespace {

static QDate ymd2date ( const QString &ymd )
{
	QDate d;
	QStringList sl = QStringList::split ( QChar ( '/' ), ymd, false );
	d. setYMD ( sl [0]. toInt ( ), sl [1]. toInt ( ), sl [2]. toInt ( ));
	return d;
}

} // namespace

void BrickLink::inventoryJobFinished ( CTransfer::Job *j )
{
	if ( !j || !j-> data ( ) || !j-> userObject ( ))
		return;

	Inventory *inv = static_cast <Inventory *> ( j-> userObject ( ));

	inv-> m_update_status = UpdateFailed;

	if ( !j-> failed ( )) {
		if ( inv-> isOrder ( )) {
			Order *ord = static_cast <Order *> ( inv );

			if ( j-> data ( )-> size ( )) {
				QBuffer order_buffer ( *j-> data ( ));

				if ( order_buffer. open ( IO_ReadOnly )) {
//	        		uint invalid_items = 0;
					InvItemList *items = 0;

					QString emsg;
					int eline = 0, ecol = 0;
					QDomDocument doc;

					if ( doc. setContent ( &order_buffer, &emsg, &eline, &ecol )) {
						QDomElement root = doc. documentElement ( );

						if (( root. nodeName ( ) == "ORDERS" ) && ( root. firstChild ( ). nodeName ( ) == "ORDER" ))
							items = BrickLink::inst ( )-> parseItemListXML ( root. firstChild ( ). toElement ( ), BrickLink::XMLHint_Order /*, &invalid_items*/ );

						for ( QDomNode node = root. firstChild ( ). firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
							if ( !node. isElement ( ))
								continue;

							QString tag = node. toElement ( ). tagName ( );
							QString val = node. toElement ( ). text ( );

							if ( tag == "ORDERID" )
								ord-> m_id = val;
							else if ( tag == "ORDERDATE" )
								ord-> m_date = ymd2date ( val );
							else if ( tag == "ORDERSTATUSCHANGED" )
								ord-> m_status_change = ymd2date ( val );
							else if ( tag == "BUYER" )
								ord-> m_buyer = val;
							else if ( tag == "ORDERSHIPPING" )
								ord-> m_shipping = money_t::fromCString ( val );
							else if ( tag == "ORDERINSURANCE" )
								ord-> m_insurance = money_t::fromCString ( val );
							else if ( tag == "ORDERDELIVERY" )
								ord-> m_delivery = money_t::fromCString ( val );
							else if ( tag == "ORDERCREDIT" )
								ord-> m_credit = money_t::fromCString ( val );
							else if ( tag == "GRANDTOTAL" )
								ord-> m_grand_total = money_t::fromCString ( val );
							else if ( tag == "ORDERSTATUS" )
								ord-> m_status = val;
							else if ( tag == "PAYMENTTYPE" )
								ord-> m_payment = val;
							else if ( tag == "ORDERREMARKS" )
								ord-> m_remarks = val;
						}
					}
					else
						CMessageBox::warning ( 0, tr( "Could not parse the XML data for order #%1:<br /><i>Line %2, column %3: %4</i>" ). arg ( CMB_BOLD( ord-> id ( ))). arg ( eline ). arg ( ecol ). arg ( emsg ));

					if ( items ) {
						inv-> m_list += *items;
						inv-> m_valid = true;
						delete items;

						//if ( invalid_items )
						//	CMessageBox::information ( 0, tr( "This order contains %1 invalid item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));
					}
					else
						CMessageBox::warning ( 0, tr( "Could not parse the XML data for order #%1." ). arg ( CMB_BOLD( ord-> id ( ))));
				}
			}
			inv-> m_fetched = QDateTime ( );
			inv-> m_update_status = Ok;
		}
		else if ( inv-> m_item ) {
			QString path = BrickLink::inst ( )-> dataPath ( inv-> m_item );
	
			bool ok = false;
	
			if ( !path. isEmpty ( )) {
				path += "inventory.xml";
	
				QFile f ( path );
				if ( j-> data ( )-> size ( ) && f. open ( IO_WriteOnly )) {
					if ( inv-> m_from_peeron ) {
						QBuffer peeron ( *j-> data ( ));
	
						if ( peeron. open ( IO_ReadOnly )) {
							ok = convert_inv_peeron_to_bricklink ( &peeron, &f );
						}
					}
					else {
						ok = ( f. writeBlock ( *j-> data ( )) == Q_LONG( j-> data ( )-> size ( )));
					}
					f. close ( );
	
					if ( ok )
						inv-> load_from_disk ( );
				}
			}
		}
		else {
			if ( j-> data ( )-> size ( )) {
				QBuffer store_buffer ( *j-> data ( ));

				if ( store_buffer. open ( IO_ReadOnly )) {
	  //      		uint invalid_items = 0;
					InvItemList *items = 0;

					QString emsg;
					int eline = 0, ecol = 0;
					QDomDocument doc;

					if ( doc. setContent ( &store_buffer, &emsg, &eline, &ecol )) {
						QDomElement root = doc. documentElement ( );

						if (( root. nodeName ( ) == "INVENTORY" ))
							items = BrickLink::inst ( )-> parseItemListXML ( root, BrickLink::XMLHint_MassUpload /*, &invalid_items */);
					}
					else
						CMessageBox::warning ( 0, tr( "Could not parse the XML data for the store inventory:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));

					if ( items ) {
						inv-> m_list += *items;
						inv-> m_valid = true;
						delete items;

						//if ( invalid_items )
						//	CMessageBox::information ( 0, tr( "This store inventory contains %1 invalid item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));
					}
					else
						CMessageBox::warning ( 0, tr( "Could not parse the XML data for the store inventory." ));
				}
			}
			inv-> m_fetched = QDateTime ( );
			inv-> m_update_status = Ok;
		}
	}

	emit inventoryUpdated ( inv );
	inv-> release ( );

}

