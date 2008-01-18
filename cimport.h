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
#include <qdatetime.h>
#include <qtextstream.h>
#include <qregexp.h>

#include "lzmadec.h"

#include "cconfig.h"
#include "bricklink.h"
#include "cprogressdialog.h"
#include "cutility.h"


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
		
		pd-> post ( url, query );

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

					if ( items ) {
						m_items += *items;
						delete items;
						ok = true;
					}
					else
						m_progress-> setErrorText ( tr( "Could not parse the XML data for the store inventory." ));
				}
				else {
					if (( QCString ( data-> data ( ), 7 ) == "<HTML>" ) && ( QCString ( data-> data ( ), data-> size ( )). find ( "Invalid password", 0, false ) != -1 ))
						m_progress-> setErrorText ( tr( "Either your username or password are incorrect." ));
					else
						m_progress-> setErrorText ( tr( "Could not parse the XML data for the store inventory:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));
				}
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
	CImportBLOrder ( const QDate &from, const QDate &to, BrickLink::Order::Type type, CProgressDialog *pd )
		: m_progress ( pd ), m_order_from ( from ), m_order_to ( to ), m_order_type ( type )
	{
		init ( );
	}

	CImportBLOrder ( const QString &order, BrickLink::Order::Type type, CProgressDialog *pd )
		: m_progress ( pd ), m_order_id ( order ), m_order_type ( type )
	{
		init ( );
	}

	void init ( )
	{
        m_current_address = -1;
		m_retry_placed = ( m_order_type == BrickLink::Order::Any );

		if ( !m_order_id. isEmpty ( )) {
			m_order_to = QDate::currentDate ( );
			m_order_from = m_order_to. addDays ( -1 );
		}

		connect ( m_progress, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

		m_progress-> setHeaderText ( tr( "Importing BrickLink Order" ));
		m_progress-> setMessageText ( tr( "Download: %1/%2 KB" ));
		
		m_url = "http://www.bricklink.com/orderExcelFinal.asp";

		m_query << CKeyValue ( "orderType",     m_order_type == BrickLink::Order::Placed ? "placed" : "received" )
		        << CKeyValue ( "action",        "save" )
		        << CKeyValue ( "orderID",       m_order_id )
		        << CKeyValue ( "viewType",      "X" )    // XML
		        << CKeyValue ( "getDateFormat", "1" )    // YYYY/MM/DD
		        << CKeyValue ( "getOrders",     m_order_id. isEmpty ( ) ? "date" : "" )
		        << CKeyValue ( "fDD",           QString::number ( m_order_from. day ( )))
		        << CKeyValue ( "fMM",           QString::number ( m_order_from. month ( )))
		        << CKeyValue ( "fYY",           QString::number ( m_order_from. year ( )))
		        << CKeyValue ( "tDD",           QString::number ( m_order_to. day ( )))
		        << CKeyValue ( "tMM",           QString::number ( m_order_to. month ( )))
		        << CKeyValue ( "tYY",           QString::number ( m_order_to. year ( )))
		        << CKeyValue ( "getDetail",     "y" )    // get items (that's why we do this in the first place...)
		        << CKeyValue ( "getFiled",      "Y" )    // regardless of filed state
		        << CKeyValue ( "getStatus",     "" )     // regardless of status
		        << CKeyValue ( "statusID",      "" )
		        << CKeyValue ( "frmUsername",   CConfig::inst ( )-> blLoginUsername ( ))
		        << CKeyValue ( "frmPassword",   CConfig::inst ( )-> blLoginPassword ( ));

		m_progress-> post ( m_url, m_query );
		m_progress-> layout ( );
	}

	const QValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &orders ( ) const 
	{
		return m_orders;
	}


private slots:
	virtual void gotten ( )
	{
		CTransfer::Job *j = m_progress-> job ( );
		QByteArray *data = j-> data ( );
		bool ok = false;
		QString error;


		if ( data && data-> size ( )) {
            if ( m_current_address >= 0 ) {
                QString s = QString::fromLatin1 ( data-> data ( ), data-> size ( ));

                QRegExp rx1 ( "<B>Name:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>" );
                QRegExp rx2 ( "<B>Address:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>" );
                QRegExp rx3 ( "<B>Country:</B></FONT></TD>\\s*<TD NOWRAP><FONT FACE=\"Tahoma, Arial\" SIZE=\"2\">(.+)</FONT></TD>" );

                rx1. setMinimal ( true );
                rx1. search ( s );
                rx2. setMinimal ( true );
                rx2. search ( s );
                rx3. setMinimal ( true );
                rx3. search ( s );

                QString a = rx1. cap ( 1 ) + "\n" + rx2. cap ( 1 ) + "\n" + rx3. cap ( 1 );
                a.replace("<BR>", "\n");
                
                m_orders[m_current_address].first->setAddress(a);
            }
            else {
			    QBuffer order_buffer ( *data );

			    if ( order_buffer. open ( IO_ReadOnly )) {
				    QString emsg;
				    int eline = 0, ecol = 0;
				    QDomDocument doc;

				    if ( doc. setContent ( &order_buffer, &emsg, &eline, &ecol )) {
					    QDomElement root = doc. documentElement ( );

					    BrickLink::InvItemList *items = 0;

					    if (( root. nodeName ( ) == "ORDERS" ) && ( root. firstChild ( ). nodeName ( ) == "ORDER" )) {
						    for ( QDomNode ordernode = root. firstChild ( ); !ordernode. isNull ( ); ordernode = ordernode. nextSibling ( )) {
							    if ( !ordernode. isElement ( ))
								    continue;

							    items = BrickLink::inst ( )-> parseItemListXML ( ordernode. toElement ( ), BrickLink::XMLHint_Order /*, &invalid_items*/ );

							    if ( items ) {
								    BrickLink::Order *order = new BrickLink::Order ( "", BrickLink::Order::Placed );

								    for ( QDomNode node = ordernode. firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
									    if ( !node. isElement ( ))
										    continue;

									    QString tag = node. toElement ( ). tagName ( );
									    QString val = node. toElement ( ). text ( );

									    if ( tag == "BUYER" )
										    order-> setBuyer ( val );
									    else if ( tag == "SELLER" )
										    order-> setSeller ( val );
									    else if ( tag == "ORDERID" )
										    order-> setId ( val );
									    else if ( tag == "ORDERDATE" )
										    order-> setDate ( ymd2date ( val ));
									    else if ( tag == "ORDERSTATUSCHANGED" )
										    order-> setStatusChange ( ymd2date ( val ));
									    else if ( tag == "ORDERSHIPPING" )
										    order-> setShipping ( money_t::fromCString ( val ));
									    else if ( tag == "ORDERINSURANCE" )
										    order-> setInsurance ( money_t::fromCString ( val ));
									    else if ( tag == "ORDERDELIVERY" )
										    order-> setDelivery ( money_t::fromCString ( val ));
									    else if ( tag == "ORDERCREDIT" )
										    order-> setCredit ( money_t::fromCString ( val ));
									    else if ( tag == "GRANDTOTAL" )
										    order-> setGrandTotal ( money_t::fromCString ( val ));
									    else if ( tag == "ORDERSTATUS" )
										    order-> setStatus ( val );
									    else if ( tag == "PAYMENTTYPE" )
										    order-> setPayment ( val );
									    else if ( tag == "ORDERREMARKS" )
										    order-> setRemarks ( val );
								    }

								    if ( !order-> id ( ). isEmpty ( )) {
									    m_orders << qMakePair ( order, items );
									    ok = true;
								    }
								    else {
									    delete items;
								    }
							    }
						    }
					    }
				    }
				    // find a better way - we shouldn't display widgets here
				    //else
				    //	CMessageBox::warning ( 0, tr( "Could not parse the XML data for your orders:<br /><i>Line %1, column %2: %3</i>" ). arg ( eline ). arg ( ecol ). arg ( emsg ));
			    }
		    }
        }

		if ( m_retry_placed ) {
			m_query [0]. second = "placed";

			m_progress-> post ( m_url, m_query, 0 );
			m_progress-> layout ( );

			m_retry_placed = false;
		}
        else if (( m_current_address + 1 ) < int( m_orders. size ( ))) {
            m_current_address++;

            QString url = QString( "http://www.bricklink.com/memberInfo.asp?u=" ) + m_orders [m_current_address]. first-> other( );
    		m_progress-> setHeaderText ( tr( "Importing address records" ));
            m_progress-> get ( url );
			m_progress-> layout ( );
        }
		else {
			m_progress-> setFinished ( true );
		}
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
	CProgressDialog *      m_progress;
	QString                m_order_id;
	QDate                  m_order_from;
	QDate                  m_order_to;
	BrickLink::Order::Type m_order_type;
	QCString               m_url;
	CKeyValueList          m_query;
	bool                   m_retry_placed;
    int                    m_current_address;
	QValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > m_orders;
};




class CImportBLCart : public QObject {
	Q_OBJECT

public:
	CImportBLCart ( int shopid, int cartid, CProgressDialog *pd )
		: m_progress ( pd )
	{
		connect ( pd, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

		pd-> setHeaderText ( tr( "Importing BrickLink Shopping Cart" ));
		pd-> setMessageText ( tr( "Download: %1/%2 KB" ));
		
		const char *url = "http://www.bricklink.com/storeCart.asp"; 

		CKeyValueList query;
		query << CKeyValue ( "h", QString::number ( shopid ))
		      << CKeyValue ( "b", QString::number ( cartid ));
		
		pd-> get ( url, query );
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
			QBuffer cart_buffer ( *data );

			if ( cart_buffer. open ( IO_ReadOnly )) {
				QTextStream ts ( &cart_buffer );
				QString line;
				QString items_line;
				QString sep = "<TR><TD HEIGHT=\"65\" ALIGN=\"CENTER\">";
				int invalid_items = 0;
				bool parsing_items = false;

				while ( true ) { 
					line = ts. readLine ( );
					if ( line. isNull ( ))
						break;
					if ( line. startsWith ( sep ) && !parsing_items )
						parsing_items = true;

					if ( parsing_items )
						items_line += line;

					if (( line == "</TABLE>" ) && parsing_items )
						break;					
				}

				QStringList strlist = QStringList::split ( sep, items_line, false );

				foreach ( const QString &str, strlist ) {
					BrickLink::InvItem *ii = 0;

					QRegExp rx_type ( " ([A-Z])-No: " );
					QRegExp rx_ids ( "HEIGHT='60' SRC='/([A-Z])/([^ ]+).gif' NAME=" );
					QRegExp rx_qty_price ( " VALUE=\"([0-9]+)\">(&nbsp;\\(x[0-9]+\\))?<BR>Qty Available: <B>[0-9]+</B><BR>Each:&nbsp;<B>\\$([0-9.]+)</B>" );
					QRegExp rx_names ( "<TD><FONT FACE=\"MS Sans Serif,Geneva\" SIZE=\"1\">(.+)</FONT></TD><TD VALIGN=\"TOP\" NOWRAP>" );
					QString str_cond ( "<B>New</B>" );

					rx_type. search ( str );
					rx_ids. search ( str );
					rx_names. search ( str );

					const BrickLink::Item *item = 0;
					const BrickLink::Color *col = 0;

					if ( rx_type. cap ( 1 ). length ( ) == 1 ) {
						int slash = rx_ids. cap ( 2 ). find ( '/' );
						
						if ( slash >= 0 ) { // with color
							item = BrickLink::inst ( )-> item ( rx_type. cap ( 1 ) [0]. latin1 ( ), rx_ids. cap ( 2 ). mid ( slash + 1 ). latin1 ( ));
							col = BrickLink::inst ( )-> color ( rx_ids. cap ( 2 ). left ( slash ). toInt ( ));
						}
						else {
							item = BrickLink::inst ( )-> item ( rx_type. cap ( 1 ) [0]. latin1 ( ), rx_ids. cap ( 2 ). latin1 ( ));
							col = BrickLink::inst ( )-> color ( 0 );
						}
					}

					QString color_and_item = rx_names. cap ( 1 ). stripWhiteSpace ( );

					if ( !col || !color_and_item. startsWith ( col-> name ( ))) {
						uint longest_match = 0;

						for ( QIntDictIterator<BrickLink::Color> it ( BrickLink::inst ( )-> colors ( )); it. current ( ); ++it )  {
							QString n ( it. current ( )-> name ( ));

							if (( n. length ( ) > longest_match ) &&
								( color_and_item. startsWith ( n ))) {
								longest_match = n. length ( );
								col = it. current ( );
							}
						}

						if ( !longest_match )
							col = BrickLink::inst ( )-> color ( 0 );
					}

					if ( !item /*|| !color_and_item. endsWith ( item-> name ( ))*/ ) {
						uint longest_match = 0;

						const QPtrVector<BrickLink::Item> &all_items = BrickLink::inst ( )-> items ( );
						for ( uint i = 0; i < all_items. count ( ); i++ ) {
							const BrickLink::Item *it = all_items [i];
							QString n ( it-> name ( ));

							if (( n. length ( ) > longest_match ) &&
								( color_and_item. find ( n )) >= 0 ) {
								longest_match = n. length ( );
								item = it;
							}
						}

						if ( !longest_match )
							item = 0;
					}

					if ( item && col ) {
						rx_qty_price. search ( str );

						int qty = rx_qty_price. cap ( 1 ). toInt ( );
						money_t price = QLocale::c ( ). toDouble ( rx_qty_price. cap ( 3 ));

						BrickLink::Condition cond = ( str. find ( str_cond ) >= 0 ? BrickLink::New : BrickLink::Used );

						QString comment;
						int comment_pos = color_and_item. find ( item-> name ( ));

						if ( comment_pos >= 0 )
							comment = color_and_item. mid ( comment_pos + QString ( item-> name ( )). length ( ) + 1 );

						if ( qty && ( price != 0 )) {
							ii = new BrickLink::InvItem ( col, item );
							ii-> setCondition ( cond );
							ii-> setQuantity ( qty );
							ii-> setPrice ( price );
							ii-> setComments ( comment );
						}
					}

					if ( ii )
						m_items << ii;
					else
						invalid_items++;
				}

				if ( !m_items. isEmpty ( )) {
					ok = true;

					if ( invalid_items ) {
						m_progress-> setMessageText ( tr( "%1 lots of your Shopping Cart could not be imported." ). arg ( invalid_items ));
						m_progress-> setAutoClose ( false );
					}
				}
				else {
					m_progress-> setErrorText ( tr( "Could not parse the Shopping Cart contents." ));
				}
			}
		}
		m_progress-> setFinished ( ok );
	}

private:
	CProgressDialog *m_progress;
	BrickLink::InvItemList m_items;
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

		pd-> get ( url );

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
					char itemtype = 'P';
					
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

					if ( itemid. find ( QRegExp ( "-\\d+$" )) > 0 )
						itemtype = 'S';

					if ( qty > 0 ) {
						QDomElement item = doc. createElement ( "ITEM" );
						root. appendChild ( item );

						// <ITEMNAME> and <COLORNAME> are inofficial extension 
						// to help with incomplete items in Peeron inventories.

						item. appendChild ( doc. createElement ( "ITEMTYPE"  ). appendChild ( doc. createTextNode ( QChar ( itemtype ))). parentNode ( ));
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
