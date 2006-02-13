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
#ifndef __CIMPORT_H__
#define __CIMPORT_H__

#include <qfile.h>
#include <qbuffer.h>

#include "liblzmadec/lzmadec.h"

#include "cconfig.h"
#include "bricklink.h"
#include "cprogressdialog.h"


class CImportBLStore : public QObject {
	Q_OBJECT

public:
	CImportBLStore ( CProgressDialog *pd )
		: m_progress ( pd )
	{
		connect ( pd, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

		pd-> setHeaderText ( tr( "Importing BrickLink Store" ));
		pd-> setMessageText ( tr( "Download: %1/%2 KB" ));
		
		const char *url = "http://www.bricklink.com/invExcelFinal.asp"; 

		CKeyValueList query;
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
		
		pd-> post ( url, query, 0 );

		pd-> layout ( );
	}

	const BrickLink::InvItemList &items ( ) const 
	{
		return m_items;
	}

private slots:
	virtual void gotten ( )
	{
		CTransfer::Job *j = m_progress-> job ( );
		QByteArray *data = j-> data ( );
		bool ok = false;

		if ( data && data-> size ( )) {
			QBuffer store_buffer ( *data );

			if ( store_buffer. open ( IO_ReadOnly )) {
				BrickLink::InvItemList *items = 0;

				QString emsg;
				int eline = 0, ecol = 0;
				QDomDocument doc;

				if ( doc. setContent ( &store_buffer, &emsg, &eline, &ecol )) {
					QDomElement root = doc. documentElement ( );

					if (( root. nodeName ( ) == "INVENTORY" ))
						items = BrickLink::inst ( )-> parseItemListXML ( root, BrickLink::XMLHint_MassUpload /*, &invalid_items */);
				}
				else
					m_progress-> setErrorText ( tr( "Could not parse the XML data for the store inventory:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));

				if ( items ) {
					m_items += *items;
					delete items;
					ok = true;
				}
				else
					m_progress-> setErrorText ( tr( "Could not parse the XML data for the store inventory." ));
			}
		}
		m_progress-> setFinished ( ok );
	}

private:
	CProgressDialog *m_progress;
	BrickLink::InvItemList m_items;
};







class CImportBLOrder : public QObject {
	Q_OBJECT

public:
	CImportBLOrder ( const QString &order, BrickLink::Order::Type type, CProgressDialog *pd )
		: m_progress ( pd ), m_order_id ( order ), m_order_type ( type ), m_order ( 0 )
	{
		connect ( pd, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

		pd-> setHeaderText ( tr( "Importing BrickLink Order" ));
		pd-> setMessageText ( tr( "Download: %1/%2 KB" ));
		
		const char *url = "http://www.bricklink.com/orderExcelFinal.asp";

		CKeyValueList query;
		query << CKeyValue ( "action",        "save" )
		      << CKeyValue ( "orderID",       order )
			  << CKeyValue ( "orderType",     type == BrickLink::Order::Placed ? "placed" : "received" )
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
		
		pd-> post ( url, query, 0 );

		pd-> layout ( );
	}

	const BrickLink::InvItemList &items ( ) const 
	{
		return m_items;
	}

	BrickLink::Order *order ( ) const
	{
		return m_order;
	}


private slots:
	virtual void gotten ( )
	{
		CTransfer::Job *j = m_progress-> job ( );
		QByteArray *data = j-> data ( );
		bool ok = false;

		if ( data && data-> size ( )) {
			QBuffer order_buffer ( *data );

			if ( order_buffer. open ( IO_ReadOnly )) {
				QString emsg;
				int eline = 0, ecol = 0;
				QDomDocument doc;

				if ( doc. setContent ( &order_buffer, &emsg, &eline, &ecol )) {
					QDomElement root = doc. documentElement ( );

					BrickLink::InvItemList *items = 0;

					if (( root. nodeName ( ) == "ORDERS" ) && ( root. firstChild ( ). nodeName ( ) == "ORDER" ))
						items = BrickLink::inst ( )-> parseItemListXML ( root. firstChild ( ). toElement ( ), BrickLink::XMLHint_Order /*, &invalid_items*/ );

					if ( items ) {
						m_items += *items;
						delete items;
						ok = true;

						m_order = new BrickLink::Order ( m_order_id, m_order_type );

						for ( QDomNode node = root. firstChild ( ). firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
							if ( !node. isElement ( ))
								continue;

							QString tag = node. toElement ( ). tagName ( );
							QString val = node. toElement ( ). text ( );

							if ( tag == "ORDERDATE" )
								m_order-> setDate ( ymd2date ( val ));
							else if ( tag == "ORDERSTATUSCHANGED" )
								m_order-> setStatusChange ( ymd2date ( val ));
							else if ( tag == "BUYER" )
								m_order-> setBuyer ( val );
							else if ( tag == "ORDERSHIPPING" )
								m_order-> setShipping ( money_t::fromCString ( val ));
							else if ( tag == "ORDERINSURANCE" )
								m_order-> setInsurance ( money_t::fromCString ( val ));
							else if ( tag == "ORDERDELIVERY" )
								m_order-> setDelivery ( money_t::fromCString ( val ));
							else if ( tag == "ORDERCREDIT" )
								m_order-> setCredit ( money_t::fromCString ( val ));
							else if ( tag == "GRANDTOTAL" )
								m_order-> setGrandTotal ( money_t::fromCString ( val ));
							else if ( tag == "ORDERSTATUS" )
								m_order-> setStatus ( val );
							else if ( tag == "PAYMENTTYPE" )
								m_order-> setPayment ( val );
							else if ( tag == "ORDERREMARKS" )
								m_order-> setRemarks ( val );
						}
					}
					else
						m_progress-> setErrorText ( tr( "Could not parse the XML data for order #%1." ). arg ( m_order_id ));
				}
				else
					m_progress-> setErrorText ( tr( "Could not parse the XML data for order #%1:<br /><i>Line %2, column %3: %4</i>" ). arg ( m_order_id ). arg ( eline ). arg ( ecol ). arg ( emsg ));
			}
		}
		m_progress-> setFinished ( ok );
	}

private:
	static QDate ymd2date ( const QString &ymd )
	{
		QDate d;
		QStringList sl = QStringList::split ( QChar ( '/' ), ymd, false );
		d. setYMD ( sl [0]. toInt ( ), sl [1]. toInt ( ), sl [2]. toInt ( ));
		return d;
	}

private:
	CProgressDialog *m_progress;
	QString m_order_id;
	BrickLink::Order::Type m_order_type;
	BrickLink::InvItemList m_items;
	BrickLink::Order *m_order;
};







class CImportPeeronInventory : public QObject {
	Q_OBJECT

public:
	CImportPeeronInventory ( const QString &peeronid, CProgressDialog *pd )
		: m_progress ( pd ), m_peeronid ( peeronid )
	{
		connect ( pd, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

		pd-> setHeaderText ( tr( "Importing Peeron Inventory" ));
		pd-> setMessageText ( tr( "Download: %1/%2 KB" ));

		QCString url;
		url. sprintf ( "http://www.peeron.com/inv/sets/%s", peeronid. latin1 ( ));

		pd-> get ( url, CKeyValueList ( ), QDateTime ( ), 0 );

		pd-> layout ( );
	}

	const BrickLink::InvItemList &items ( ) const 
	{
		return m_items;
	}

private slots:
	virtual void gotten ( )
	{
		CTransfer::Job *j = m_progress-> job ( );
		QByteArray *data = j-> data ( );
		bool ok = false;

		if ( data && data-> size ( )) {
			QBuffer peeron_buffer ( *data );

			if ( peeron_buffer. open ( IO_ReadOnly )) {
				BrickLink::InvItemList *items = fromPeeron ( &peeron_buffer );

				if ( items ) {
					m_items += *items;
					delete items;
					ok = true;
				}
				else
					m_progress-> setErrorText ( tr( "Could not parse the Peeron inventory." ));
			}
		}
		m_progress-> setFinished ( ok );
	}

private:
	BrickLink::InvItemList *fromPeeron ( QIODevice *peeron )
	{
		if ( !peeron )
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

		return count ? BrickLink::inst ( )-> parseItemListXML ( root, BrickLink::XMLHint_Inventory /*, &invalid_items */) : 0;
	}


private:
	CProgressDialog *m_progress;
	BrickLink::InvItemList m_items;
	QString m_peeronid;
};

#endif
