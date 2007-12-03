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
#include <stdio.h>
#include <stdlib.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QtXml/QDomDocument>
#include <QRegExp>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QDebug>

#include "cconfig.h"
#include "cutility.h"
#include "cmappedfile.h"
#include "bricklink.h"

#define DEFAULT_DATABASE_VERSION  0
#define DEFAULT_DATABASE_NAME     "database-v%1"


QUrl BrickLink::Core::url ( UrlList u, const void *opt, const void *opt2 )
{
	QUrl url;

	switch ( u ) {
		case URL_InventoryRequest:
			url = "http://www.bricklink.com/bbsiXML.asp";
			break;
			
		case URL_WantedListUpload:
			url = "http://www.bricklink.com/wantedXML.asp";
			break;
			
		case URL_InventoryUpload:
			url = "http://www.bricklink.com/invXML.asp";
			break;
			
		case URL_InventoryUpdate:
			url = "http://www.bricklink.com/invXML.asp#update";
			break;
			
		case URL_CatalogInfo:
			if ( opt ) {
				const Item *item = static_cast <const Item *> ( opt );

				url = "http://www.bricklink.com/catalogItem.asp";
				url. addQueryItem ( QChar( item-> itemType ( )-> id ( )), item-> id ( ));
			}
			break;
		
		case URL_PriceGuideInfo:
			if ( opt && opt2 ) {
				const Item *item = static_cast <const Item *> ( opt );

				url = "http://www.bricklink.com/catalogPriceGuide.asp";
				url. addQueryItem ( QChar( item-> itemType ( )-> id ( )), item-> id ( ));

				if ( item-> itemType ( )-> hasColors ( ))
					url. addQueryItem ( "colorID", QString::number ( static_cast <const Color *> ( opt2 )-> id ( )));
			}
			break;

		case URL_LotsForSale:
			if ( opt && opt2 ) {
				const Item *item = static_cast <const Item *> ( opt );

				url = "http://www.bricklink.com/search.asp";
				url. addQueryItem ( "viewFrom", "sa" );
				url. addQueryItem ( "itemType", QChar( item-> itemType ( )-> id ( )));
				
				// workaround for BL not accepting the -X suffix for sets, instructions and boxes
				QString id = item-> id ( );
				char itt = item-> itemType ( )-> id ( );
				
				if ( itt == 'S' || itt == 'I' || itt == 'O' ) {
					int pos = id. lastIndexOf ( '-' );
					if ( pos >= 0 )
						id. truncate ( pos );
				}
				url. addQueryItem ( "q", id );
				
				if ( item-> itemType ( )-> hasColors ( ))
					url. addQueryItem ( "colorID", QString::number ( static_cast <const Color *> ( opt2 )-> id ( )));
			}
			break;
			
		case URL_AppearsInSets:
			if ( opt && opt2 ) {
				const Item *item = static_cast <const Item *> ( opt );

				url = "http://www.bricklink.com/catalogItemIn.asp";
				url. addQueryItem ( QChar( item-> itemType ( )-> id ( )), item-> id ( ));
				url. addQueryItem ( "in", "S" );
				                                                                        
				if ( item-> itemType ( )-> hasColors ( ))
					url. addQueryItem ( "colorID", QString::number ( static_cast <const Color *> ( opt2 )-> id ( )));
			}
			break;

		case URL_ColorChangeLog:
			url = "http://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=R";
			break;
			
		case URL_ItemChangeLog:
			url = "http://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=I";

			if ( opt )
				url. addQueryItem ( "q", static_cast <const char *> ( opt ));
			break;

		case URL_PeeronInfo:
			if ( opt ) {
				url = "http://peeron.com/cgi-bin/invcgis/psearch";
				url. addQueryItem ( "query", static_cast <const Item *> ( opt )-> id ( ));
				url. addQueryItem ( "limit", "none" );
			}
			break;

		case URL_StoreItemDetail:
			if ( opt ) {
				url = "http://www.bricklink.com/inventory_detail.asp";
				url. addQueryItem ( "itemID", QString::number ( *static_cast <const unsigned int *> ( opt )));
			}
			break;

		default:
			break;
	}
	return url;
}


const QPixmap *BrickLink::Core::noImage ( const QSize &s )
{
	QString key = QString( "%1x%2" ). arg( s. width ( )). arg( s. height ( ));

	QPixmap *pix = m_noimages [key];

	if ( !pix ) {
		pix = new QPixmap(s);
		QPainter p ( pix );

		int w = pix-> width ( );
		int h = pix-> height ( );
		bool high = ( w < h );

		p. fillRect ( 0, 0, w, h, Qt::white );

		QRect r ( high ? 0 : ( w - h ) / 2, high ? ( w -h ) / 2 : 0, high ? w : h, high ? w : h );
		int w4 = r. width ( ) / 4;
		r. adjust ( w4, w4, -w4, -w4 );
		
		QColor coltable [] = {
			QColor ( 0x00, 0x00, 0x00 ),
			QColor ( 0x3f, 0x3f, 0x3f ),
			QColor ( 0xff, 0x7f, 0x7f )
		};
		
		for ( int i = 0; i < 3; i++ ) {
			r. adjust ( -1, -1, -1, -1 );

			p. setPen ( QPen( coltable [i], 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ));
			p. drawLine ( r. x ( ), r. y ( ), r. right ( ), r. bottom ( ));
			p. drawLine ( r. right ( ), r. y ( ), r. x ( ), r. bottom ( ));
		}
		p. end ( );

		m_noimages. insert ( key, pix );
	}
	return pix;
}


QImage BrickLink::Core::colorImage ( const Color *col, int w, int h ) const
{
	if ( !col || w <= 0 || h <= 0 )
		return QImage ( );
		
	QColor c = col-> color ( );
	bool is_valid = c. isValid ( );
	
	QImage img ( w, h, QImage::Format_ARGB32_Premultiplied );
	QRect r ( 0, 0, w, h );	
	QPainter p;

	p. begin ( &img );
	
	if ( is_valid ) {
		QBrush brush;
		
		if ( col-> isGlitter ( )) {
			brush = QBrush( CUtility::contrastColor ( c, 0.25f ), Qt::Dense6Pattern );
		}
		else if ( col-> isSpeckle ( )) {
			brush = QBrush( CUtility::contrastColor ( c, 0.20f ), Qt::Dense7Pattern );
		}
		else if ( col-> isMetallic ( )) {
			QColor c2 = CUtility::gradientColor ( c, Qt::black );

			QLinearGradient gradient ( 0, 0, 0, r. height ( ));
			gradient. setColorAt ( 0,   c2 );
			gradient. setColorAt ( 0.3, c  );
			gradient. setColorAt ( 0.7, c  );
			gradient. setColorAt ( 1,   c2 );
			brush = gradient;
		}
		else if ( col-> isChrome ( )) {
			QColor c2 = CUtility::gradientColor ( c, Qt::black );

			QLinearGradient gradient ( 0, 0, r. width ( ), 0 );
			gradient. setColorAt ( 0,   c2 );
			gradient. setColorAt ( 0.3, c  );
			gradient. setColorAt ( 0.7, c  );
			gradient. setColorAt ( 1,   c2 );
			brush = gradient;
		}

		p. fillRect ( r, c );
		p. fillRect ( r, brush );

	}
	else {
		p. setRenderHint ( QPainter::Antialiasing );
		p. setPen ( Qt::darkGray );
		p. setBrush ( QColor ( 255, 255, 255, 128 ));
		p. drawRect ( r );
		p. drawLine ( 0, 0, w, h );
		p. drawLine ( 0, h, w, 0 );
	}
	
	p. end ( );
	
	if ( col-> isTransparent ( ) /*&& CResource::inst ( )-> pixmapAlphaSupported ( )*/) {
		QImage alpha ( w, h, QImage::Format_ARGB32_Premultiplied );
		
		QLinearGradient gradient ( 0, 0, r. width ( ), r. height ( ));
		gradient. setColorAt ( 0,   Qt::white );
		gradient. setColorAt ( 0.4, Qt::white );
		gradient. setColorAt ( 0.6, Qt::darkGray );
		gradient. setColorAt ( 1,   Qt::darkGray );
		
		QPainter ap;
		ap. begin ( &alpha );
		ap. fillRect ( r, gradient );
		ap. end ( );

		img. setAlphaChannel ( alpha );
	}
	
	return img;
}



namespace {

static bool check_and_create_path ( const QString &p )
{
	QFileInfo fi ( p );

	if ( !fi. exists ( )) {
		QDir::current ( ). mkdir ( p );
		fi. refresh ( );
	}
	return ( fi. exists ( ) && fi. isDir ( ) && fi. isReadable ( ) && fi. isWritable ( ));
}

} // namespace

QString BrickLink::Core::dataPath ( ) const
{
	return m_datadir;
}

QString BrickLink::Core::dataPath ( const ItemType *item_type ) const
{
	QString p = dataPath ( );
	p += item_type-> id ( );
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}

QString BrickLink::Core::dataPath ( const Item *item ) const
{
	QString p = dataPath ( item-> itemType ( ));
	p += item-> m_id;
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}

QString BrickLink::Core::dataPath ( const Item *item, const Color *color ) const
{
	QString p = dataPath ( item );
	p += QString::number ( color-> id ( ));
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}



BrickLink::Core *BrickLink::Core::s_inst = 0;

BrickLink::Core *BrickLink::Core::create ( const QString &datadir, QString *errstring )
{
	if ( !s_inst ) {
		s_inst = new Core ( datadir );

		QString test = s_inst-> dataPath ( );
		if ( test. isNull ( ) || !check_and_create_path ( test )) {
			delete s_inst;
			s_inst = 0;

			if ( errstring )
				*errstring = tr( "Data directory \'%1\' is not both read- and writable." ). arg ( datadir );
		}
	}
	return s_inst;
}

BrickLink::Core::Core ( const QString &datadir )
	: m_datadir ( datadir ), m_c_locale ( QLocale::c ( ))
{
	if ( m_datadir. isEmpty ( ))
		m_datadir = "./";

	m_datadir = QDir::cleanPath ( datadir );

	if ( m_datadir. right ( 1 ) != "/" )
		m_datadir += "/";
		
	m_online = true;

	/*
	m_databases. colors. setAutoDelete ( true );
	m_databases. categories. setAutoDelete ( true );
	m_databases. item_types. setAutoDelete ( true );
	m_databases. items. setAutoDelete ( true );
	*/

	m_price_guides.transfer = new CTransfer(1);
	m_price_guides.update_iv = 0;

	m_pictures.transfer = new CTransfer(1);
	m_pictures.update_iv = 0;

	QPixmapCache::setCacheLimit ( 20 * 1024 );  // 80 x 60 x 32 (w x h x bpp) == 20kB -> room for ~1000 pixmaps

	connect ( m_price_guides. transfer, SIGNAL( finished ( CTransferJob * )), this, SLOT( priceGuideJobFinished ( CTransferJob * )));
	connect ( m_pictures.     transfer, SIGNAL( finished ( CTransferJob * )), this, SLOT( pictureJobFinished    ( CTransferJob * )));

	connect ( m_pictures.     transfer, SIGNAL( progress ( int, int )), this, SIGNAL( pictureProgress ( int, int )));
	connect ( m_price_guides. transfer, SIGNAL( progress ( int, int )), this, SIGNAL( priceGuideProgress ( int, int )));
}

BrickLink::Core::~Core ( )
{
	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	delete m_pictures. transfer;
	delete m_price_guides. transfer;

	qDeleteAll ( m_noimages );

	s_inst = 0;
}

void BrickLink::Core::setHttpProxy (const QNetworkProxy &proxy)
{
	m_pictures.transfer->setProxy(proxy);
	m_price_guides.transfer->setProxy(proxy);
}

void BrickLink::Core::setUpdateIntervals ( int pic, int pg )
{
	m_pictures.     update_iv = pic;
	m_price_guides. update_iv = pg;
}

bool BrickLink::Core::updateNeeded ( const QDateTime &last, int iv )
{
	return ( iv > 0 ) && ( last. secsTo ( QDateTime::currentDateTime ( )) > iv );
}

void BrickLink::Core::setOnlineStatus ( bool on )
{
	m_online = on;
}

bool BrickLink::Core::onlineStatus ( ) const
{
	return m_online;
}


const QHash<int, const BrickLink::Color *> &BrickLink::Core::colors ( ) const
{
	return m_databases. colors;
}

const QHash<int, const BrickLink::Category *> &BrickLink::Core::categories ( ) const
{
	return m_databases. categories;
}

const QHash<int, const BrickLink::ItemType *> &BrickLink::Core::itemTypes ( ) const
{
	return m_databases. item_types;
}

const QVector<const BrickLink::Item *> &BrickLink::Core::items ( ) const
{
	return m_databases. items;
}

const BrickLink::Category *BrickLink::Core::category ( uint id ) const
{
	return m_databases. categories [id];
}

const BrickLink::Color *BrickLink::Core::color ( uint id ) const
{
	return m_databases. colors [id];
}

const BrickLink::Color *BrickLink::Core::colorFromPeeronName ( const char *peeron_name ) const
{
	if ( !peeron_name || !peeron_name [0] )
		return 0;

	for ( QHash<int, const Color *>::const_iterator it = m_databases. colors. begin ( ); it != m_databases. colors. end ( ); ++it ) {
		if ( qstricmp ( it. value ( )-> peeronName ( ), peeron_name ) == 0 )
			return it. value ( );
	}
	return 0;
}


const BrickLink::Color *BrickLink::Core::colorFromLDrawId ( int ldraw_id ) const
{
	for ( QHash<int, const Color *>::const_iterator it = m_databases. colors. begin ( ); it != m_databases. colors. end ( ); ++it ) {
		if ( it. value ( )-> ldrawId ( ) == ldraw_id )
			return it. value ( );
	}
	return 0;
}
        

const BrickLink::ItemType *BrickLink::Core::itemType ( char id ) const
{
	return m_databases. item_types [id];
}

const BrickLink::Item *BrickLink::Core::item ( char tid, const char *id ) const
{
	Item key;
	key. m_item_type = itemType ( tid );
	key. m_id = const_cast <char *> ( id );

	Item *keyp = &key;

	Item **itp = (Item **) bsearch ( &keyp, m_databases. items. data ( ), m_databases. items. count ( ), sizeof( Item * ), (int (*) ( const void *, const void * )) Item::compare );

	key. m_id = 0;
	key. m_item_type = 0;

	return itp ? *itp : 0;
}

void BrickLink::Core::cancelPictureTransfers ( )
{
	while ( !m_pictures. diskload. isEmpty ( ))
		m_pictures. diskload. takeFirst ( )-> release ( );
	m_pictures.transfer->abortAllJobs ( );
}

void BrickLink::Core::cancelPriceGuideTransfers ( )
{
	m_price_guides.transfer->abortAllJobs ( );
}

QString BrickLink::Core::defaultDatabaseName ( ) const
{
	return QString( DEFAULT_DATABASE_NAME ). arg ( DEFAULT_DATABASE_VERSION );
}

namespace {

class stopwatch {
public:
	stopwatch ( const char *desc ) 
	{ 
		m_label = desc;
		m_start = clock ( ); 
	}
	~stopwatch ( ) 
	{
		uint msec = uint( clock ( ) - m_start ) * 1000 / CLOCKS_PER_SEC;
		qWarning ( "%s: %d'%d [sec]", m_label, msec / 1000, msec % 1000 );
	}
private:
	const char *m_label;
	clock_t m_start;
};

} // namespace


bool BrickLink::Core::readDatabase ( const QString &fname )
{
	QString filename = fname. isNull ( ) ? dataPath ( ) + defaultDatabaseName ( ) : fname;

	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	m_price_guides. cache. clear ( );
	m_pictures. cache. clear ( );
	QPixmapCache::clear ( );

	qDeleteAll ( m_databases. colors );
	qDeleteAll ( m_databases. item_types );
	qDeleteAll ( m_databases. categories );
	qDeleteAll ( m_databases. items );

	m_databases. colors. clear ( );
	m_databases. item_types. clear ( );
	m_databases. categories. clear ( );
	m_databases. items. clear ( );


	bool result = false;
	stopwatch *sw = 0; //new stopwatch( "readDatabase" );

	CMappedFile f ( filename );
	QDataStream *pds = f. open ( );

	if ( pds ) {
		QDataStream &ds = *pds;
		quint32 magic = 0, filesize = 0, version = 0;

		ds >> magic >> filesize >> version;
		
		if (( magic != quint32( 0xb91c5703 )) || ( filesize != f. size ( )) || ( version != DEFAULT_DATABASE_VERSION ))
			return false;

		ds. setByteOrder ( QDataStream::LittleEndian );
		ds. setVersion ( QDataStream::Qt_3_3 );

		// colors
		quint32 colc = 0;
		ds >> colc;

		for ( quint32 i = colc; i; i-- ) {
			Color *col = new Color ( );
			ds >> col;
			m_databases. colors. insert ( col-> id ( ), col );
		}

		// categories
		quint32 catc = 0;
		ds >> catc;

		for ( quint32 i = catc; i; i-- ) {
			Category *cat = new Category ( );
			ds >> cat;
			m_databases. categories. insert ( cat-> id ( ), cat );
		}

		// types
		quint32 ittc = 0;
		ds >> ittc;

		for ( quint32 i = ittc; i; i-- ) {
			ItemType *itt = new ItemType ( );
			ds >> itt;
			m_databases. item_types. insert ( itt-> id ( ), itt );
		}

		// items
		quint32 itc = 0;
		ds >> itc;

		m_databases. items. resize ( itc );
		for ( quint32 i = 0; i < itc; i++ ) {
			Item *item = new Item ( );
			ds >> item;
			m_databases. items. replace ( i, item );
		}
		quint32 allc = 0;
		ds >> allc >> magic;

		if (( allc == ( colc + ittc + catc + itc )) && ( magic == quint32( 0xb91c5703 ))) {
			delete sw;
#ifdef _MSC_VER
#define PF_SIZE_T   "I"
#else
#define PF_SIZE_T   "z"
#endif
			qDebug ( "Color: %8u  (%11" PF_SIZE_T "u bytes)", m_databases. colors. count ( ),     m_databases. colors. count ( )     * ( sizeof( Color )    + 20 ));
			qDebug ( "Types: %8u  (%11" PF_SIZE_T "u bytes)", m_databases. item_types. count ( ), m_databases. item_types. count ( ) * ( sizeof( ItemType ) + 20 ));
			qDebug ( "Cats : %8u  (%11" PF_SIZE_T "u bytes)", m_databases. categories. count ( ), m_databases. categories. count ( ) * ( sizeof( Category ) + 20 ));
			qDebug ( "Items: %8u  (%11" PF_SIZE_T "u bytes)", m_databases. items. count ( ),      m_databases. items. count ( )      * ( sizeof( Item )     + 20 ));
#undef PF_SIZE_T

			result = true;
		}
	}
	if ( !result ) {
		m_databases. colors. clear ( );
		m_databases. item_types. clear ( );
		m_databases. categories. clear ( );
		m_databases. items. clear ( );

		qWarning ( "Error reading databases!" );
	}
	return result;
}


BrickLink::InvItemList *BrickLink::Core::parseItemListXML ( QDomElement root, ItemListXMLHint hint, uint *invalid_items )
{
	QString roottag, itemtag;

	switch ( hint ) {
		case XMLHint_BrikTrak  : roottag = "GRID"; itemtag = "ITEM"; break;
		case XMLHint_Order     : roottag = "ORDER"; itemtag = "ITEM"; break;
		case XMLHint_MassUpload:
		case XMLHint_MassUpdate:
		case XMLHint_WantedList:
		case XMLHint_Inventory : roottag = "INVENTORY"; itemtag = "ITEM"; break;
		case XMLHint_BrickStore: roottag = "Inventory"; itemtag = "Item"; break;
	}

	if ( root. nodeName ( ) != roottag )
		return 0;

	InvItemList *inv = new InvItemList ( );

	uint incomplete = 0;

	for ( QDomNode itemn = root. firstChild ( ); !itemn. isNull ( ); itemn = itemn. nextSibling ( )) {
		if ( itemn. nodeName ( ) != itemtag )
			continue;

		InvItem *ii = new InvItem ( );

		QString itemid, itemname;
		QString itemtypeid, itemtypename;
		QString colorid, colorname;
		QString categoryid, categoryname;

		bool has_orig_price = false;
		bool has_orig_qty = false;

		for ( QDomNode n = itemn. firstChild ( ); !n. isNull ( ); n = n. nextSibling ( )) {
			if ( !n. isElement ( ))
				continue;
			QString tag = n. toElement ( ). tagName ( );
			QString val = n. toElement ( ). text ( );

			// ### BrickLink XML & BrikTrak ###
			if ( hint != XMLHint_BrickStore ) {
				if ( tag == ( hint == XMLHint_BrikTrak ? "PART_NO" : "ITEMID" ))
					itemid = val;
				else if ( tag == ( hint == XMLHint_BrikTrak ? "COLOR_ID" : "COLOR" ))
					colorid = val;
				else if ( tag == ( hint == XMLHint_BrikTrak ? "CATEGORY_ID" : "CATEGORY" ))
					categoryid = val;
				else if ( tag == ( hint == XMLHint_BrikTrak ? "TYPE" : "ITEMTYPE" ))
					itemtypeid = val;
				else if ( tag == "IMAGE" )
					ii-> setCustomPictureUrl ( val );
				else if ( tag == "PRICE" )
					ii-> setPrice ( money_t::fromCString ( val ));
				else if ( tag == "BULK" )
					ii-> setBulkQuantity ( val. toInt ( ));
				else if ( tag == "QTY" )
					ii-> setQuantity ( val. toInt ( ));
				else if ( tag == "SALE" )
					ii-> setSale ( val. toInt ( ));
				else if ( tag == "CONDITION" )
					ii-> setCondition ( val == "N" ? New : Used );
				else if ( tag == ( hint == XMLHint_BrikTrak ? "NOTES" : "DESCRIPTION" ))
					ii-> setComments ( val );
				else if ( tag == "REMARKS" )
					ii-> setRemarks ( val );
				else if ( tag == "TQ1" )
					ii-> setTierQuantity ( 0, val. toInt ( ));
				else if ( tag == "TQ2" )
					ii-> setTierQuantity ( 1, val. toInt ( ));
				else if ( tag == "TQ3" )
					ii-> setTierQuantity ( 2, val. toInt ( ));
				else if ( tag == "TP1" )
					ii-> setTierPrice ( 0, money_t::fromCString ( val ));
				else if ( tag == "TP2" )
					ii-> setTierPrice ( 1, money_t::fromCString ( val ));
				else if ( tag == "TP3" )
					ii-> setTierPrice ( 2, money_t::fromCString ( val ));
				else if ( tag == "LOTID" )
					ii-> setLotId ( val. toUInt ( ));
			}

			// ### BrickLink Order (workaround for broken BL script) ###
			if ( hint == XMLHint_Order ) {
				// The remove(',') stuff is a workaround for the 
				// broken Order XML generator: the XML contains , as 
				// thousands-separator: 1,752 instead of 1752

				if ( tag == "QTY" )
					ii-> setQuantity ( val. remove ( ',' ). toInt ( ));
			}

			// ### BrikTrak import ###
			if ( hint == XMLHint_BrikTrak ) {
				if ( tag == "PART_DESCRIPTION" )
					itemname = val;
				else if ( tag == "COLOR" )
					colorname = val;
				else if ( tag == "CATEGORY" )
					categoryname = val;
				else if ( tag == "CHECKBOX" ) {
					switch ( val. toInt ( )) {
						case 0: ii-> setStatus ( Exclude ); break;
						case 1: ii-> setStatus ( Include ); break;
						case 3: ii-> setStatus ( Extra   ); break;
						case 5: ii-> setStatus ( Unknown ); break;
					}
				}

				// the following tags are BrickStore extensions
				else if ( tag == "RETAIN" )
					ii-> setRetain ( val == "Y" );
				else if ( tag == "STOCKROOM" )
					ii-> setStockroom ( val == "Y" );
				else if ( tag == "BUYERUSERNAME" )
					ii-> setReserved ( val );
			}

			// ### Inventory Request ###
			else if ( hint == XMLHint_Inventory ) {
				if (( tag == "EXTRA" ) && ( val == "Y" ))
					ii-> setStatus ( Extra );
				else if ( tag == "ITEMNAME" )  // BrickStore extension for Peeron inventories
					itemname = val;
				else if ( tag == "COLORNAME" ) // BrickStore extension for Peeron inventories
					colorname = val;
			}

			// ### BrickStore BSX ###
			else if ( hint == XMLHint_BrickStore ) {
				if ( tag == "ItemID" )
					itemid = val;
				else if ( tag == "ColorID" )
					colorid = val;
				else if ( tag == "CategoryID" )
					categoryid = val;
				else if ( tag == "ItemTypeID" )
					itemtypeid = val;
				else if ( tag == "ItemName" )
					itemname = val;
				else if ( tag == "ColorName" )
					colorname = val;
				else if ( tag == "CategoryName" )
					categoryname = val;
				else if ( tag == "ItemTypeName" )
					itemtypename = val;
				else if ( tag == "Image" )
					ii-> setCustomPictureUrl ( val );
				else if ( tag == "Price" )
					ii-> setPrice ( money_t::fromCString ( val ));
				else if ( tag == "Bulk" )
					ii-> setBulkQuantity ( val. toInt ( ));
				else if ( tag == "Qty" )
					ii-> setQuantity ( val. toInt ( ));
				else if ( tag == "Sale" )
					ii-> setSale ( val. toInt ( ));
				else if ( tag == "Condition" )
					ii-> setCondition ( val == "N" ? New : Used );
				else if ( tag == "Comments" )
					ii-> setComments ( val );
				else if ( tag == "Remarks" )
					ii-> setRemarks ( val );
				else if ( tag == "TQ1" )
					ii-> setTierQuantity ( 0, val. toInt ( ));
				else if ( tag == "TQ2" )
					ii-> setTierQuantity ( 1, val. toInt ( ));
				else if ( tag == "TQ3" )
					ii-> setTierQuantity ( 2, val. toInt ( ));
				else if ( tag == "TP1" )
					ii-> setTierPrice ( 0, money_t::fromCString ( val ));
				else if ( tag == "TP2" )
					ii-> setTierPrice ( 1, money_t::fromCString ( val ));
				else if ( tag == "TP3" )
					ii-> setTierPrice ( 2, money_t::fromCString ( val ));
				else if ( tag == "Status" ) {
					Status st = Include;

					if ( val == "X" )
						st = Exclude;
					else if ( val == "I" )
						st = Include;
					else if ( val == "E" )
						st = Extra;
					else if ( val == "?" )
						st = Unknown;

					ii-> setStatus ( st );
				}
				else if ( tag == "LotID" )
					ii-> setLotId ( val. toUInt ( ));
				else if ( tag == "Retain" )
					ii-> setRetain ( true );
				else if ( tag == "Stockroom" )
					ii-> setStockroom ( true );
				else if ( tag == "Reserved" )
					ii-> setReserved ( val );
				else if ( tag == "TotalWeight" )
					ii-> setWeight ( cLocale ( ). toDouble ( val ));
				else if ( tag == "OrigPrice" ) {
					ii-> setOrigPrice ( money_t::fromCString ( val ));
					has_orig_price = true;
				}
				else if ( tag == "OrigQty" ) {
					ii-> setOrigQuantity ( val. toInt ( ));
					has_orig_qty = true;
				}
			}
		}

		bool ok = true;

		//qDebug ( "item (id=%s, color=%s, type=%s, cat=%s", itemid. latin1 ( ), colorid. latin1 ( ), itemtypeid. latin1 ( ), categoryid. latin1 ( ));

		if ( !has_orig_price )
			ii-> setOrigPrice ( ii-> price ( ));
		if ( !has_orig_qty )
			ii-> setOrigQuantity ( ii-> quantity ( ));

		if ( !colorid. isEmpty ( ) && !itemid. isEmpty ( ) && !itemtypeid. isEmpty ( )) {
			ii-> setItem ( item ( itemtypeid [0]. toLatin1 ( ), itemid. toLatin1 ( )));

			if ( !ii-> item ( )) {
				qWarning ( ) << "failed: invalid itemid (" << itemid << ") and/or itemtypeid (" << itemtypeid [0] << ")";
				ok = false;
			}
			
			ii-> setColor ( color ( colorid. toInt ( )));

			if ( !ii-> color ( )) {
				qWarning ( ) << "failed: invalid color (" << colorid << ")";
				ok = false;
			}
		}
		else
			ok = false;

		if ( !ok ) {
			qWarning ( ) << "failed: insufficient data (item=" << itemid << ", itemtype=" << itemtypeid [0] << ", category=" << categoryid << ", color=" << colorid << ")";

			InvItem::Incomplete *inc = new InvItem::Incomplete;
			
			if ( !ii-> item ( )) {
				inc-> m_item_id = itemid;
				inc-> m_item_name = itemname;
			}
			if ( !ii-> itemType ( )) {
				inc-> m_itemtype_id = itemtypeid;
				inc-> m_itemtype_name = itemtypename;
			}
			if ( !ii-> category ( )) {
				inc-> m_category_id = categoryid;
				inc-> m_category_name = categoryname;
			}
			if ( !ii-> color ( )) {
				inc-> m_color_id = colorid;
				inc-> m_color_name = colorname;
			}
			ii-> setIncomplete ( inc );
			
			ok = true;
			incomplete++;
		}

		if ( ok )
			inv-> append ( ii );
		else
			delete ii;
	}

	if ( invalid_items )
		*invalid_items = incomplete;
	if ( incomplete )
		qWarning ( ) << "Parse XML (hint=" << int( hint ) << "): " << incomplete << " items have incomplete records";

	return inv;
}



QDomElement BrickLink::Core::createItemListXML ( QDomDocument doc, ItemListXMLHint hint, const InvItemList *items, QMap <QString, QString> *extra )
{
	QString roottag, itemtag;

	switch ( hint ) {
		case XMLHint_BrikTrak  : roottag = "GRID"; itemtag = "ITEM"; break;
		case XMLHint_MassUpload:
		case XMLHint_MassUpdate:
		case XMLHint_WantedList:
		case XMLHint_Inventory : roottag = "INVENTORY"; itemtag = "ITEM"; break;
		case XMLHint_BrickStore: roottag = "Inventory"; itemtag = "Item"; break;
		case XMLHint_Order     : break;
	}

	QDomElement root = doc. createElement ( roottag );
	
	if ( root. isNull ( ) || roottag. isNull ( ) || itemtag. isEmpty ( ) || !items )
		return root;

	foreach ( const InvItem *ii, *items ) {
		if ( ii-> isIncomplete ( ))
			continue;
		
		if (( ii-> status ( ) == Exclude ) && ( hint != XMLHint_BrickStore && hint != XMLHint_BrikTrak ))
			continue;

		if ( hint == XMLHint_MassUpdate ) {
			bool diffs = (( ii-> quantity ( ) != ii-> origQuantity ( )) || ( ii-> price ( ) != ii-> origPrice ( )));

			if ( !ii-> lotId ( ) || !diffs )
				continue;
		}

		QDomElement item = doc. createElement ( itemtag );
		root. appendChild ( item );

		// ### MASS UPDATE ###
		if ( hint == XMLHint_MassUpdate ) {
			item. appendChild ( doc. createElement ( "LOTID" ). appendChild ( doc. createTextNode ( QString::number ( ii-> lotId ( )))). parentNode ( ));

			int qdiff = ii-> quantity ( ) - ii-> origQuantity ( );
			money_t pdiff = ii-> price ( ) - ii-> origPrice ( );

			if ( pdiff != 0 )
				item. appendChild ( doc. createElement ( "PRICE"  ). appendChild ( doc. createTextNode ( ii-> price ( ). toCString ( ))). parentNode ( ));
			if ( qdiff && ( ii-> quantity ( ) > 0 ))
				item. appendChild ( doc. createElement ( "QTY"    ). appendChild ( doc. createTextNode (( qdiff > 0 ? "+" : "" ) + QString::number ( qdiff ))). parentNode ( ));
			else if ( qdiff && ( ii-> quantity ( ) <= 0 ))
				item. appendChild ( doc. createElement ( "DELETE" ));
		}

		// ### BRIK TRAK EXPORT ###
		else if ( hint == XMLHint_BrikTrak ) {
			item. appendChild ( doc. createElement ( "PART_NO"     ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR_ID"    ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CATEGORY_ID" ). appendChild ( doc. createTextNode ( QString::number ( ii-> category ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "TYPE"        ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));

			item. appendChild ( doc. createElement ( "CATEGORY"         ). appendChild ( doc. createTextNode ( ii-> category ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "PART_DESCRIPTION" ). appendChild ( doc. createTextNode ( ii-> item ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR"            ). appendChild ( doc. createTextNode ( ii-> color ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "TOTAL_VALUE"      ). appendChild ( doc. createTextNode (( ii-> price ( ) * ii-> quantity ( )). toCString ( ))). parentNode ( ));
			
			int cb = 1;
			switch ( ii-> status ( )) {
				case Exclude: cb = 0; break;
				case Include: cb = 1; break;
				case Extra  : cb = 3; break;
				case Unknown: cb = 5; break;
			}
			item. appendChild ( doc. createElement ( "CHECKBOX"  ). appendChild ( doc. createTextNode ( QString::number ( cb ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "QTY"       ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "PRICE"     ). appendChild ( doc. createTextNode ( ii-> price ( ). toCString ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CONDITION" ). appendChild ( doc. createTextNode (( ii-> condition ( ) == New ) ? "N" : "U" )). parentNode ( ));

			if ( !ii-> customPictureUrl ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "IMAGE"   ). appendChild ( doc. createTextNode ( ii-> customPictureUrl ( ))). parentNode ( ));
			if ( ii-> bulkQuantity ( ) != 1 )
				item. appendChild ( doc. createElement ( "BULK"    ). appendChild ( doc. createTextNode ( QString::number ( ii-> bulkQuantity ( )))). parentNode ( ));
			if ( ii-> sale ( ))
				item. appendChild ( doc. createElement ( "SALE"    ). appendChild ( doc. createTextNode ( QString::number ( ii-> sale ( )))). parentNode ( ));
			if ( !ii-> comments ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "NOTES"   ). appendChild ( doc. createTextNode ( ii-> comments ( ))). parentNode ( ));
			if ( !ii-> remarks ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "REMARKS" ). appendChild ( doc. createTextNode ( ii-> remarks ( ))). parentNode ( ));

			if ( ii-> tierQuantity ( 0 )) {
				item. appendChild ( doc. createElement ( "TQ1"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 0 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP1"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 0 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ2"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 1 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP2"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 1 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ3"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 2 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP3"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 2 ). toCString ( ))). parentNode ( ));
			}
		}

		// ### BrickStore BSX ###
		else if ( hint == XMLHint_BrickStore ) {
			item. appendChild ( doc. createElement ( "ItemID"       ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ItemTypeID"   ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ColorID"      ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));

			// this extra information is useful, if the e.g. the color- or item-id 
			// are no longer available after a database update
			item. appendChild ( doc. createElement ( "ItemName"     ). appendChild ( doc. createTextNode ( ii-> item ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ItemTypeName" ). appendChild ( doc. createTextNode ( ii-> itemType ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ColorName"    ). appendChild ( doc. createTextNode ( ii-> color ( )-> name ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CategoryID"   ). appendChild ( doc. createTextNode ( QString::number ( ii-> category ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CategoryName" ). appendChild ( doc. createTextNode ( ii-> category ( )-> name ( ))). parentNode ( ));
			
			const char *st;
			switch ( ii-> status ( )) {
				case Unknown: st = "?"; break;
				case Extra  : st = "E"; break;
				case Exclude: st = "X"; break;
				case Include:
				default              : st = "I"; break;
			}
			item. appendChild ( doc. createElement ( "Status"       ). appendChild ( doc. createTextNode ( st )). parentNode ( ));
			item. appendChild ( doc. createElement ( "Qty"          ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "Price"        ). appendChild ( doc. createTextNode ( ii-> price ( ). toCString ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "Condition"    ). appendChild ( doc. createTextNode (( ii-> condition ( ) == New ) ? "N" : "U" )). parentNode ( ));

			if ( !ii-> customPictureUrl ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "Image"    ). appendChild ( doc. createTextNode ( ii-> customPictureUrl ( ))). parentNode ( ));
			if ( ii-> bulkQuantity ( ) != 1 )
				item. appendChild ( doc. createElement ( "Bulk"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> bulkQuantity ( )))). parentNode ( ));
			if ( ii-> sale ( ))
				item. appendChild ( doc. createElement ( "Sale"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> sale ( )))). parentNode ( ));
			if ( !ii-> comments ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "Comments" ). appendChild ( doc. createTextNode ( ii-> comments ( ))). parentNode ( ));
			if ( !ii-> remarks ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "Remarks"  ). appendChild ( doc. createTextNode ( ii-> remarks ( ))). parentNode ( ));
			if ( ii-> retain ( ))
				item. appendChild ( doc. createElement ( "Retain"   ));
			if ( ii-> stockroom ( ))
				item. appendChild ( doc. createElement ( "Stockroom" ));
			if ( !ii-> reserved ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "Reserved" ). appendChild ( doc. createTextNode ( ii-> reserved ( ))). parentNode ( ));
			if ( ii-> lotId ( ))
				item. appendChild ( doc. createElement ( "LotID"    ). appendChild ( doc. createTextNode ( QString::number ( ii-> lotId ( )))). parentNode ( ));

			if ( ii-> tierQuantity ( 0 )) {
				item. appendChild ( doc. createElement ( "TQ1" ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 0 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP1" ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 0 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ2" ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 1 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP2" ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 1 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ3" ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 2 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP3" ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 2 ). toCString ( ))). parentNode ( ));
			}

			if ( ii-> m_weight > 0 )
				item. appendChild ( doc. createElement ( "TotalWeight"  ). appendChild ( doc. createTextNode ( cLocale ( ). toString ( ii-> weight ( ), 'f', 4 ))). parentNode ( ));
			if ( ii-> origPrice ( ) != ii-> price ( )) 
				item. appendChild ( doc. createElement ( "OrigPrice"  ). appendChild ( doc. createTextNode ( ii-> origPrice ( ). toCString ( ))). parentNode ( ));
			if ( ii-> origQuantity ( ) != ii-> quantity ( ))
				item. appendChild ( doc. createElement ( "OrigQty"  ). appendChild ( doc. createTextNode ( QString::number ( ii-> origQuantity ( )))). parentNode ( ));
		}

		// ### MASS UPLOAD ###
		else if ( hint == XMLHint_MassUpload ) {
			item. appendChild ( doc. createElement ( "ITEMID"   ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR"    ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CATEGORY" ). appendChild ( doc. createTextNode ( QString::number ( ii-> category ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ITEMTYPE" ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));

			item. appendChild ( doc. createElement ( "QTY"       ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "PRICE"     ). appendChild ( doc. createTextNode ( ii-> price ( ). toCString ( ))). parentNode ( ));
			item. appendChild ( doc. createElement ( "CONDITION" ). appendChild ( doc. createTextNode (( ii-> condition ( ) == New ) ? "N" : "U" )). parentNode ( ));

			if ( !ii-> customPictureUrl ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "IMAGE"     ). appendChild ( doc. createTextNode ( ii-> customPictureUrl ( ))). parentNode ( ));
			if ( ii-> bulkQuantity ( ) != 1 )
				item. appendChild ( doc. createElement ( "BULK"      ). appendChild ( doc. createTextNode ( QString::number ( ii-> bulkQuantity ( )))). parentNode ( ));
			if ( ii-> sale ( ))
				item. appendChild ( doc. createElement ( "SALE"      ). appendChild ( doc. createTextNode ( QString::number ( ii-> sale ( )))). parentNode ( ));
			if ( !ii-> comments ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "DESCRIPTION" ). appendChild ( doc. createTextNode ( ii-> comments ( ))). parentNode ( ));
			if ( !ii-> remarks ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "REMARKS"   ). appendChild ( doc. createTextNode ( ii-> remarks ( ))). parentNode ( ));
			if ( ii-> retain ( ))
				item. appendChild ( doc. createElement ( "RETAIN"    ). appendChild ( doc. createTextNode ( "Y" )). parentNode ( ));
			if ( ii-> stockroom ( ))
				item. appendChild ( doc. createElement ( "STOCKROOM" ). appendChild ( doc. createTextNode ( "Y" )). parentNode ( ));
			if ( !ii-> reserved ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "BUYERUSERNAME" ). appendChild ( doc. createTextNode ( ii-> reserved ( ))). parentNode ( ));

			if ( ii-> tierQuantity ( 0 )) {
				item. appendChild ( doc. createElement ( "TQ1"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 0 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP1"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 0 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ2"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 1 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP2"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 1 ). toCString ( ))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TQ3"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> tierQuantity ( 2 )))). parentNode ( ));
				item. appendChild ( doc. createElement ( "TP3"     ). appendChild ( doc. createTextNode ( ii-> tierPrice ( 2 ). toCString ( ))). parentNode ( ));
			}
		}

		// ### WANTED LIST ###
		else if ( hint == XMLHint_WantedList ) {
			item. appendChild ( doc. createElement ( "ITEMID"       ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ITEMTYPE"     ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR"        ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));

			if ( ii-> quantity ( ))
				item. appendChild ( doc. createElement ( "MINQTY"   ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			if ( ii-> price ( ) != 0 )
				item. appendChild ( doc. createElement ( "MAXPRICE" ). appendChild ( doc. createTextNode ( ii-> price ( ). toCString ( ))). parentNode ( ));
			if ( !ii-> remarks ( ). isEmpty ( ))
				item. appendChild ( doc. createElement ( "REMARKS"  ). appendChild ( doc. createTextNode ( ii-> remarks ( ))). parentNode ( ));
			if ( ii-> condition ( ) == New )
				item. appendChild ( doc. createElement ( "CONDITION"). appendChild ( doc. createTextNode ( "N" )). parentNode ( ));
		}

		// ### INVENTORY REQUEST ###
		else if ( hint == XMLHint_Inventory ) {
			item. appendChild ( doc. createElement ( "ITEMID"    ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ITEMTYPE"  ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "QTY"       ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			
			if ( ii-> status ( ) == Extra )
				item. appendChild ( doc. createElement ( "EXTRA" ). appendChild ( doc. createTextNode ( "Y" )). parentNode ( ));
		}

		// optional: additonal tags
		if ( extra ) {
			for ( QMap <QString, QString>::iterator it = extra-> begin ( ); it != extra-> end ( ); ++it )
				item. appendChild ( doc. createElement ( it. key ( )). appendChild ( doc. createTextNode ( it. value ( ))). parentNode ( ));
		}
	}

	return root;
}



bool BrickLink::Core::parseLDrawModel ( QFile &f, InvItemList &items, uint *invalid_items )
{
	QHash<QString, InvItem *> mergehash;
	
	return parseLDrawModelInternal ( f, QString::null, items, invalid_items, mergehash );
}

bool BrickLink::Core::parseLDrawModelInternal ( QFile &f, const QString &model_name, InvItemList &items, uint *invalid_items, QHash<QString, InvItem *> &mergehash )
{
	QStringList searchpath;
	QString ldrawdir = CConfig::inst ( )-> lDrawDir ( );
	uint invalid = 0;
	int linecount = 0;

	bool is_mpd = false;
	int current_mpd_index = -1;
	QString current_mpd_model;
	bool is_mpd_model_found = false;


	searchpath. append ( QFileInfo ( f ). dir ( ). absolutePath ( ));
	if ( !ldrawdir. isEmpty ( )) {
		searchpath. append ( ldrawdir + "/models" );
	}

	if ( f. isOpen ( )) {
		QTextStream in ( &f );
		QString line;
		
		while (!( line = in. readLine ( )). isNull ( )) {
			linecount++;

			if ( !line. isEmpty ( ) && line [0] == '0' ) {
				QStringList sl = line. simplified ( ). split( ' ' );

				if (( sl. count ( ) >= 2 ) && ( sl [1] == "FILE" )) {
					is_mpd = true;
					current_mpd_model = sl [2]. toLower ( );
					current_mpd_index++;

					if ( is_mpd_model_found )
						break;

					if (( current_mpd_model == model_name. toLower ( )) || ( model_name. isEmpty ( ) && ( current_mpd_index == 0 )))
						is_mpd_model_found = true;

					if ( f. isSequential ( ))
						return false; // we need to seek!
				}
			}
			else if ( !line. isEmpty ( ) && line [0] == '1' ) {
				if ( is_mpd && !is_mpd_model_found )
					continue;

				QStringList sl = line. simplified ( ). split ( ' ' );
				
				if ( sl. count ( ) >= 15 ) {
					int colid = sl [1]. toInt ( );
					QString partname = sl [14]. toLower ( );
					
					QString partid = partname;
					partid. truncate ( partid. lastIndexOf ( '.' ));
										
					const Color *colp = colorFromLDrawId ( colid );
					const Item *itemp = item ( 'P', partid. toLatin1 ( ));

					
					if ( !itemp ) {
						bool got_subfile = false;
					
						if ( is_mpd ) {
							uint sub_invalid_items = 0;

							qint64 oldpos = f. pos ( );
							f. seek ( 0 );

							got_subfile = parseLDrawModelInternal ( f, partname, items, &sub_invalid_items, mergehash );

							invalid += sub_invalid_items;
							f. seek ( oldpos );
						}

						if ( !got_subfile ) {
							for ( QStringList::iterator it = searchpath. begin ( ); it != searchpath. end ( ); ++it ) {
								QFile subf ( *it + "/" + partname );
								
								if ( subf. open ( QIODevice::ReadOnly )) {
									uint sub_invalid_items = 0;
									
									(void) parseLDrawModelInternal ( subf, partname, items, &sub_invalid_items, mergehash );
									
									invalid += sub_invalid_items;
									got_subfile = true;
									break;
								}
							}
						}
						if ( got_subfile )
							continue;
					}
					
					QString key = QString( "%1@%2" ). arg( partid ). arg( colid );

					InvItem *ii = mergehash [key];
					
					if ( ii ) {
						ii-> m_quantity++;
					}
					else {
						ii = new InvItem ( colp, itemp );
						ii-> m_quantity = 1;
						
						if ( !colp || !itemp ) {
							InvItem::Incomplete *inc = new InvItem::Incomplete;
							
							if ( !itemp ) {
								inc-> m_item_id = partid;
								inc-> m_item_name = QString( "LDraw ID: %1" ). arg ( partid );
								inc-> m_itemtype_id = 'P';
							}
							if ( !colp ) {
								inc-> m_color_id = -1;
								inc-> m_color_name = QString( "LDraw #%1" ). arg ( colid );
							}
							ii-> setIncomplete ( inc );					
							invalid++;
						}
						
						items. append ( ii );
						mergehash. insert ( key, ii );
					}
				}
			}
		}
	}
	
	if ( invalid_items )
		*invalid_items = invalid;
		
	return is_mpd ? is_mpd_model_found : true;
}



/*
 * Support routines to rebuild the DB from txt files
 */
void BrickLink::Core::setDatabase_ConsistsOf ( const QHash<const Item *, InvItemList> &hash )
{
	for ( QHash<const Item *, InvItemList>::const_iterator it = hash. begin ( ); it != hash. end ( ); ++it )
		it. key ( )-> setConsistsOf ( it. value ( ));
}

void BrickLink::Core::setDatabase_AppearsIn ( const QHash<const Item *, Item::AppearsIn> &hash )
{
	for ( QHash<const Item *, Item::AppearsIn>::const_iterator it = hash. begin ( ); it != hash. end ( ); ++it )
		it. key ( )-> setAppearsIn ( it. value ( ));
}

void BrickLink::Core::setDatabase_Basics ( const QHash<int, const Color *> &colors, 
									 const QHash<int, const Category *> &categories,
									 const QHash<int, const ItemType *> &item_types,
									 const QVector<const Item *> &items )
{
	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	m_price_guides. cache. clear ( );
	m_pictures. cache. clear ( );
	QPixmapCache::clear ( );

	m_databases. colors. clear ( );
	m_databases. item_types. clear ( );
	m_databases. categories. clear ( );
	m_databases. items. clear ( );

	m_databases. colors     = colors;
	m_databases. item_types = item_types;
	m_databases. categories = categories;
	m_databases. items      = items;
}

bool BrickLink::Core::writeDatabase ( const QString &fname )
{
	QString filename = fname. isNull ( ) ? dataPath ( ) + defaultDatabaseName ( ) : fname;

	QFile f ( filename + ".new" );
	if ( f. open ( QIODevice::WriteOnly )) {
		QDataStream ds ( &f );

		ds << quint32( 0 /*magic*/ ) << quint32 ( 0 /*filesize*/ ) << quint32( DEFAULT_DATABASE_VERSION /*version*/ );
		
		ds. setByteOrder ( QDataStream::LittleEndian );

		// colors
		quint32 colc = m_databases. colors. count ( );
		ds << colc;
		
		foreach ( const Color *col, m_databases. colors )
			ds << col;

		// categories
		quint32 catc = m_databases. categories. count ( );
		ds << catc;

		foreach ( const Category *cat, m_databases. categories )
			ds << cat;
		
		// types
		quint32 ittc = m_databases. item_types. count ( );
		ds << ittc;
		
		foreach ( const ItemType *itt, m_databases. item_types )
			ds << itt;

		// items
		quint32 itc = m_databases. items. count ( );
		ds << itc;

		const Item **itp = m_databases. items. data ( );
		for ( quint32 i = itc; i; itp++, i-- )
			ds << *itp;

		ds << ( colc + ittc + catc + itc );
		ds << quint32( 0xb91c5703 );

		quint32 filesize = quint32( f. pos ( ));

		if ( f. error ( ) == QFile::NoError ) {
			f. close ( );

			if ( f. open ( QIODevice::ReadWrite )) {
				QDataStream ds2 ( &f );
				ds2 << quint32( 0xb91c5703 ) << filesize;

				if ( f. error ( ) == QFile::NoError ) {
					f. close ( );

					QString err = CUtility::safeRename ( filename );

					if ( err. isNull ( ))
						return true;
				}
			}
		}
	}
	if ( f. isOpen ( ))
		f. close ( );
	
	QFile::remove ( filename + ".new" );
	return false;
}
