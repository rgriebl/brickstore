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

#include <qfile.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qtextstream.h>
#include <qdom.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qpixmapcache.h>
#include <qpainter.h>
#include <qdict.h>

#include "cconfig.h"
#include "cresource.h"
#include "cutility.h"
#include "bricklink.h"

#define DEFAULT_DATABASE_VERSION  0
#define DEFAULT_DATABASE_NAME     "database-v%1"


QCString BrickLink::url ( UrlList u, const void *opt, const void *opt2 )
{
	QCString url;

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
			if ( opt )
				url. sprintf ( "http://www.bricklink.com/catalogItem.asp?%c=%s", static_cast <const Item *> ( opt )-> itemType ( )-> id ( ),
				                                                                 static_cast <const Item *> ( opt )-> id ( ));
			break;
		
		case URL_PriceGuideInfo:
			if ( opt && opt2 ) {
				url. sprintf ( "http://www.bricklink.com/catalogPriceGuide.asp?%c=%s", static_cast <const Item *> ( opt )-> itemType ( )-> id ( ),
				                                                                       static_cast <const Item *> ( opt )-> id ( ));

				if ( static_cast <const Item *> ( opt )-> itemType ( )-> hasColors ( )) {
					QCString col;
					
					col. sprintf ( "&colorID=%d", static_cast <const Color *> ( opt2 )-> id ( ));
					url += col;
				}
			}
			break;

		case URL_LotsForSale:
			if ( opt && opt2 ) {
				url. sprintf ( "http://www.bricklink.com/search.asp?viewFrom=sa&itemType=%c&q=%s", static_cast <const Item *> ( opt )-> itemType ( )-> id ( ),
				                                                                                   static_cast <const Item *> ( opt )-> id ( ));
				
				// workaround for BL not accepting the -X suffix for sets, instructions and boxes
				char itt = static_cast <const Item *> ( opt )-> itemType ( )-> id ( );
				
				if ( itt == 'S' || itt == 'I' || itt == 'O' ) {
					int pos = url. findRev ( '-' );
					if ( pos >= 0 )
						url. truncate ( pos );
				}
				
				if ( static_cast <const Item *> ( opt )-> itemType ( )-> hasColors ( )) {
					QCString col;
					
					col. sprintf ( "&colorID=%d", static_cast <const Color *> ( opt2 )-> id ( ));
					url += col;
				}
			}
			break;
			
		case URL_AppearsInSets:
			if ( opt && opt2 ) {
				url. sprintf ( "http://www.bricklink.com/catalogItemIn.asp?%c=%s&in=S", static_cast <const Item *> ( opt )-> itemType ( )-> id ( ),
				                                                                        static_cast <const Item *> ( opt )-> id ( ));
				if ( static_cast <const Item *> ( opt )-> itemType ( )-> hasColors ( )) {
					QCString col;
					
					col. sprintf ( "&colorID=%d", static_cast <const Color *> ( opt2 )-> id ( ));
					url += col;
				}
			}
			break;

		case URL_ColorChangeLog:
			url = "http://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=R";
			break;
			
		case URL_ItemChangeLog:
			url = "http://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=I";

			if ( opt ) {
				url. append ( "&q=" );
				url. append ( static_cast <const char *> ( opt ));
			}
			break;

		case URL_PeeronInfo:
			if ( opt )
				url. sprintf ( "http://peeron.com/cgi-bin/invcgis/psearch?query=%s&limit=none", static_cast <const Item *> ( opt )-> id ( ));
			break;

		case URL_StoreItemDetail:
			if ( opt )
				url. sprintf ( "http://www.bricklink.com/inventory_detail.asp?itemID=%u", *static_cast <const unsigned int *> ( opt ));
			break;

		default:
			break;
	}
	return url;
}


const QPixmap *BrickLink::noImage ( const QSize &s )
{
	QString key = QString( "%1x%2" ). arg( s. width ( )). arg( s. height ( ));

	QPixmap *pix = m_noimages [key];

	if ( !pix ) {
		pix = new QPixmap ( s * 2 );
		pix-> fill ( Qt::white );
		QPainter p ( pix );

		int w = pix-> width ( );
		int h = pix-> height ( );
		bool high = ( w < h );

		QRect r ( high ? 0 : ( w - h ) / 2, high ? ( w -h ) / 2 : 0, high ? w : h, high ? w : h );
		int w4 = r. width ( ) / 4;
		r. addCoords ( w4, w4, -w4, -w4 );
		
		QColor coltable [] = {
			QColor ( 0x00, 0x00, 0x00 ),
			QColor ( 0x3f, 0x3f, 0x3f ),
			QColor ( 0xff, 0x7f, 0x7f )
		};
		
		for ( int i = 0; i < 3; i++ ) {
			r. addCoords ( -1, -1, -1, -1 );

			p. setPen ( QPen( coltable [i], 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ));
			p. drawLine ( r. x ( ), r. y ( ), r. right ( ), r. bottom ( ));
			p. drawLine ( r. right ( ), r. y ( ), r. x ( ), r. bottom ( ));
		}
		p. end ( );

		pix-> convertFromImage ( pix-> convertToImage ( ). smoothScale ( s ));
		m_noimages. insert ( key, pix );
	}
	return pix;
}


QImage BrickLink::colorImage ( const Color *col, int w, int h ) const
{
	if ( !col || w <= 0 || h <= 0 )
		return QImage ( );
		
	QColor c = col-> color ( );
	bool is_valid = c. isValid ( );
	
	QPixmap pix ( w, h );
	
	QPainter p ( &pix );
	QRect r = pix. rect ( );
	
	if ( is_valid ) {
		p. fillRect ( r, c );
		
		if ( col-> isGlitter ( )) {
			p. setPen ( Qt::NoPen );
			p. setBackgroundColor ( c );
			p. setBrush ( QBrush( CUtility::contrastColor ( c, 0.25f ), Qt::Dense6Pattern ));
			p. drawRect ( r );
		}
		else if ( col-> isSpeckle ( )) {
			p. setPen ( Qt::NoPen );
			p. setBackgroundColor ( c );
			p. setBrush ( QBrush( CUtility::contrastColor ( c, 0.20f ), Qt::Dense7Pattern ));
			p. drawRect ( r );
		}
		else if ( col-> isMetallic ( )) {
			if ( h >= 7 ) {
				int mid = ( h & 1 ) ? 3 : 4;
				QColor c2 = CUtility::gradientColor ( c, Qt::black );
				QImage tgrad = CUtility::createGradient ( QSize ( w, ( h - mid ) / 2 ), Qt::Vertical, c, c2,  110.f );
				QImage bgrad = tgrad. mirror ( false, true );
				
				p. drawImage ( 0, 0, tgrad );
				p. drawImage ( 0, tgrad. height ( ) + mid, bgrad );
			}
		}
		else if ( col-> isChrome ( )) {
			if ( w >= 7 ) {
				int mid = ( w & 1 ) ? 3 : 4;
				QColor c2 = CUtility::gradientColor ( c, Qt::black );
				QImage lgrad = CUtility::createGradient ( QSize (( w - mid ) / 2, h ), Qt::Horizontal, c, c2,  115.f );
				QImage rgrad = lgrad. mirror ( true, false );

				p. drawImage ( 0, 0, lgrad );
				p. drawImage ( lgrad. width ( ) + mid, 0, rgrad );
			}
		}
	}
	else {
		p. fillRect ( r, Qt::white );
		p. setPen ( Qt::darkGray );
		p. setBackgroundColor ( Qt::white );
		p. setBrush ( Qt::NoBrush );
		p. drawRect ( r );
		p. drawLine ( 0, 0,   w-1, h-1 );
		p. drawLine ( 0, h-1, w-1, 0   );
	}
	
	p. end ( );
	QImage img = pix. convertToImage ( );
	
	if ( col-> isTransparent ( ) && CResource::inst ( )-> pixmapAlphaSupported ( )) {
		img. setAlphaBuffer ( true );
		
		float e = float( w ) / float( h );
		float f = e;
		
		for ( int y = 0; y < h; y++ ) {
			QRgb *line = (QRgb *) img. scanLine ( y );

			for ( int x = 0; x < w; x++ ) {
				int a = ( x > ( w - 1 - int( f ))) ? 128 : 255;

				*line = qRgba ( qRed ( *line ), qGreen ( *line ), qBlue ( *line ), a );
				line++;
			}
			f += e;
		}
		
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

QString BrickLink::dataPath ( ) const
{
	return m_datadir;
}

QString BrickLink::dataPath ( const ItemType *item_type ) const
{
	QString p = dataPath ( );
	p += item_type-> id ( );
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}

QString BrickLink::dataPath ( const Item *item ) const
{
	QString p = dataPath ( item-> itemType ( ));
	p += item-> m_id;
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}

QString BrickLink::dataPath ( const Item *item, const Color *color ) const
{
	QString p = dataPath ( item );
	p += QString::number ( color-> id ( ));
	p += '/';

	if ( !check_and_create_path ( p ))
		return QString ( );

	return p;
}



BrickLink *BrickLink::s_inst = 0;

BrickLink *BrickLink::inst ( )
{
	return s_inst;
}

BrickLink *BrickLink::inst ( const QString &datadir, QString *errstring )
{
	if ( !s_inst ) {
		s_inst = new BrickLink ( datadir );

		QString test = s_inst-> dataPath ( );
		if ( !test || !check_and_create_path ( test )) {
			delete s_inst;
			s_inst = 0;

			if ( errstring )
				*errstring = tr( "Data directory \'%1\' is not both read- and writable." ). arg ( datadir );
		}
		else {
			if (!( s_inst-> m_pictures. transfer-> init ( ) &&
			       s_inst-> m_price_guides. transfer-> init ( ))) {
				delete s_inst;
				s_inst = 0;

				if ( errstring )
					*errstring = tr( "HTTP transfer threads could not be started." );
			}
		}

	}
	return s_inst;
}

BrickLink::BrickLink ( const QString &datadir )
	: m_datadir ( datadir ), m_c_locale ( QLocale::c ( ))
{
	if ( m_datadir. isEmpty ( ))
		m_datadir = "./";

	m_datadir = QDir::cleanDirPath ( datadir );

	if ( m_datadir. right ( 1 ) != "/" )
		m_datadir += "/";
		
	m_online = true;

	m_noimages. setAutoDelete ( true );

	m_databases. colors. resize ( 151 );
	m_databases. categories. resize ( 503 );
	m_databases. item_types. resize ( 13 );

	m_databases. colors. setAutoDelete ( true );
	m_databases. categories. setAutoDelete ( true );
	m_databases. item_types. setAutoDelete ( true );
	m_databases. items. setAutoDelete ( true );

	m_price_guides. transfer = new CTransfer ( );
	m_price_guides. update_iv = 0;

	m_pictures. transfer = new CTransfer ( );
	m_pictures. update_iv = 0;

	QPixmapCache::setCacheLimit ( 20 * 1024 * 1024 );  // 80 x 60 x 32 (w x h x bpp) == 20kB -> room for ~1000 pixmaps

	connect ( m_price_guides. transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( priceGuideJobFinished ( CTransfer::Job * )));
	connect ( m_pictures.     transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( pictureJobFinished    ( CTransfer::Job * )));

	connect ( m_pictures.     transfer, SIGNAL( progress ( int, int )), this, SIGNAL( pictureProgress ( int, int )));
	connect ( m_price_guides. transfer, SIGNAL( progress ( int, int )), this, SIGNAL( priceGuideProgress ( int, int )));
}

BrickLink::~BrickLink ( )
{
	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	delete m_pictures. transfer;
	delete m_price_guides. transfer;

	s_inst = 0;
}

void BrickLink::setHttpProxy ( bool enable, const QString &name, int port )
{
	QCString aname = name. latin1 ( );

	m_pictures.     transfer-> setProxy ( enable, aname, port );
	m_price_guides. transfer-> setProxy ( enable, aname, port );
}

void BrickLink::setUpdateIntervals ( int pic, int pg )
{
	m_pictures.     update_iv = pic;
	m_price_guides. update_iv = pg;
}

bool BrickLink::updateNeeded ( const QDateTime &last, int iv )
{
	return ( iv > 0 ) && ( last. secsTo ( QDateTime::currentDateTime ( )) > iv );
}

void BrickLink::setOnlineStatus ( bool on )
{
	m_online = on;
}

bool BrickLink::onlineStatus ( ) const
{
	return m_online;
}


const QIntDict<BrickLink::Color> &BrickLink::colors ( ) const
{
	return m_databases. colors;
}

const QIntDict<BrickLink::Category> &BrickLink::categories ( ) const
{
	return m_databases. categories;
}

const QIntDict<BrickLink::ItemType> &BrickLink::itemTypes ( ) const
{
	return m_databases. item_types;
}

const QPtrVector<BrickLink::Item> &BrickLink::items ( ) const
{
	return m_databases. items;
}

const BrickLink::Category *BrickLink::category ( uint id ) const
{
	return m_databases. categories [id];
}

const BrickLink::Color *BrickLink::color ( uint id ) const
{
	return m_databases. colors [id];
}

const BrickLink::Color *BrickLink::colorFromPeeronName ( const char *peeron_name ) const
{
	if ( !peeron_name || !peeron_name [0] )
		return 0;

	for ( QIntDictIterator <Color> it ( m_databases. colors ); it. current ( ); ++it ) {
		if ( qstricmp ( it. current ( )-> peeronName ( ), peeron_name ) == 0 )
			return it. current ( );
	}
	return 0;
}


const BrickLink::Color *BrickLink::colorFromLDrawId ( int ldraw_id ) const
{
	for ( QIntDictIterator <Color> it ( m_databases. colors ); it. current ( ); ++it ) {
		if ( it. current ( )-> ldrawId ( ) == ldraw_id )
			return it. current ( );
	}
	return 0;
}
        

const BrickLink::ItemType *BrickLink::itemType ( char id ) const
{
	return m_databases. item_types [id];
}

const BrickLink::Item *BrickLink::item ( char tid, const char *id ) const
{
	BrickLink::Item key;
	key. m_item_type = itemType ( tid );
	key. m_id = const_cast <char *> ( id );

	BrickLink::Item *keyp = &key;

	Item **itp = (Item **) bsearch ( &keyp, m_databases. items. data ( ), m_databases. items. count ( ), sizeof( Item * ), (int (*) ( const void *, const void * )) Item::compare );

	key. m_id = 0;
	key. m_item_type = 0;

	return itp ? *itp : 0;
}

void BrickLink::cancelPictureTransfers ( )
{
	while ( !m_pictures. diskload. isEmpty ( ))
		m_pictures. diskload. take ( 0 )-> release ( );
	m_pictures. transfer-> cancelAllJobs ( );
}

void BrickLink::cancelPriceGuideTransfers ( )
{
	m_price_guides. transfer-> cancelAllJobs ( );
}

QString BrickLink::defaultDatabaseName ( ) const
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


#if defined( Q_OS_WIN32 )
#include <io.h>

#else
#include <sys/mman.h>

#endif

namespace {

class CMappedFile {
public:
	CMappedFile ( const QString &filename )
		: m_file ( filename ), m_memptr ( 0 ), m_ds ( 0 )
	{ 
#if defined( Q_OS_WIN32 )
		m_maphandle = 0;
#endif
	}

	~CMappedFile ( )
	{ close ( ); }

	QDataStream *open ( )
	{
		if ( m_file. isOpen ( ))
			close ( );

		if ( m_file. open ( IO_ReadOnly | IO_Raw )) {
			m_filesize = m_file. size ( );

			if ( m_filesize ) {
#if defined( Q_OS_WIN32 )
				m_maphandle = CreateFileMapping ((HANDLE) _get_osfhandle ( m_file. handle ( )), 0, PAGE_READONLY, 0, 0, 0 );
				if ( m_maphandle ) {		
					m_memptr = (const char *) MapViewOfFile ( m_maphandle, FILE_MAP_READ, 0, 0, 0 );
#else
				if ( true ) {
					m_memptr = (const char *) mmap ( 0, m_filesize, PROT_READ, MAP_SHARED, m_file. handle ( ), 0 );

#endif
					if ( m_memptr ) {
						m_mem. setRawData ( m_memptr, m_filesize );

						m_ds = new QDataStream ( m_mem, IO_ReadOnly );
						return m_ds;
					}
				}
			}
		}
		close ( );
		return 0;
	}

	void close ( )
	{
		delete m_ds;
		m_ds = 0;
		if ( m_memptr ) {
			m_mem. resetRawData ( m_memptr, m_filesize );
#if defined( Q_OS_WIN32 )
			UnmapViewOfFile ( m_memptr );
#else
			munmap ((void *) m_memptr, m_filesize );
#endif
			m_memptr = 0;
		}
#if defined( Q_OS_WIN32 )
		if ( m_maphandle ) {
			CloseHandle ( m_maphandle );
			m_maphandle = 0;
		}
#endif
		m_file. close ( );
	}

	Q_UINT32 size ( ) const
	{
		return m_filesize;
	}

private:
	QFile        m_file;
	Q_UINT32     m_filesize;
	const char * m_memptr;
	QByteArray   m_mem;
	QDataStream *m_ds;

#if defined( Q_OS_WIN32 )
	HANDLE       m_maphandle;
#endif
};

}

bool BrickLink::readDatabase ( const QString &fname )
{
	QString filename = fname. isNull ( ) ? dataPath ( ) + defaultDatabaseName ( ) : fname;

	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	m_price_guides. cache. clear ( );
	m_pictures. cache. clear ( );
	QPixmapCache::clear ( );

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
		Q_UINT32 magic = 0, filesize = 0, version = 0;

		ds >> magic >> filesize >> version;
		
		if (( magic != Q_UINT32( 0xb91c5703 )) || ( filesize != f. size ( )) || ( version != DEFAULT_DATABASE_VERSION ))
			return false;

		ds. setByteOrder ( QDataStream::LittleEndian );

		// colors
		Q_UINT32 colc = 0;
		ds >> colc;

		for ( Q_UINT32 i = colc; i; i-- ) {
			Color *col = new Color ( );
			ds >> col;
			m_databases. colors. insert ( col-> id ( ), col );
		}

		// categories
		Q_UINT32 catc = 0;
		ds >> catc;

		for ( Q_UINT32 i = catc; i; i-- ) {
			Category *cat = new Category ( );
			ds >> cat;
			m_databases. categories. insert ( cat-> id ( ), cat );
		}

		// types
		Q_UINT32 ittc = 0;
		ds >> ittc;

		for ( Q_UINT32 i = ittc; i; i-- ) {
			ItemType *itt = new ItemType ( );
			ds >> itt;
			m_databases. item_types. insert ( itt-> id ( ), itt );
		}

		// items
		Q_UINT32 itc = 0;
		ds >> itc;

		m_databases. items. resize ( itc );
		for ( Q_UINT32 i = 0; i < itc; i++ ) {
			Item *item = new Item ( );
			ds >> item;
			m_databases. items. insert ( i, item );
		}
		Q_UINT32 allc = 0;
		ds >> allc >> magic;

		if (( allc == ( colc + ittc + catc + itc )) && ( magic == Q_UINT32( 0xb91c5703 ))) {
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




BrickLink::InvItemList *BrickLink::parseItemListXML ( QDomElement root, ItemListXMLHint hint, uint *invalid_items )
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
						case 0: ii-> setStatus ( InvItem::Exclude ); break;
						case 1: ii-> setStatus ( InvItem::Include ); break;
						case 3: ii-> setStatus ( InvItem::Extra   ); break;
						case 5: ii-> setStatus ( InvItem::Unknown ); break;
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
					ii-> setStatus ( InvItem::Extra );
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
					InvItem::Status st = InvItem::Include;

					if ( val == "X" )
						st = InvItem::Exclude;
					else if ( val == "I" )
						st = InvItem::Include;
					else if ( val == "E" )
						st = InvItem::Extra;
					else if ( val == "?" )
						st = InvItem::Unknown;

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
				else if ( tag == "OrigPrice" )
					ii-> setOrigPrice ( money_t::fromCString ( val ));
				else if ( tag == "OrigQty" )
					ii-> setOrigQuantity ( val. toInt ( ));
			}
		}

		bool ok = true;

		//qDebug ( "item (id=%s, color=%s, type=%s, cat=%s", itemid. latin1 ( ), colorid. latin1 ( ), itemtypeid. latin1 ( ), categoryid. latin1 ( ));

		if ( !colorid. isEmpty ( ) && !itemid. isEmpty ( ) && !itemtypeid. isEmpty ( )) {
			ii-> setItem ( item ( itemtypeid [0]. latin1 ( ), itemid. latin1 ( )));

			if ( !ii-> item ( )) {
				qWarning ( "failed: invalid itemid (%s) and/or itemtypeid (%c)", itemid. latin1 ( ), itemtypeid [0]. latin1 ( ));
				ok = false;
			}
			
			ii-> setColor ( color ( colorid. toInt ( )));

			if ( !ii-> color ( )) {
				qWarning ( "failed: invalid color (%d)", colorid. toInt ( ));
				ok = false;
			}
		}
		else
			ok = false;

		if ( !ok ) {
			qWarning ( "failed: insufficient data (item=%s, itemtype=%c, category=%d, color=%d", itemid. latin1 ( ), itemtypeid [0]. latin1 ( ), categoryid. toInt ( ), colorid. toInt ( ));

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
		qWarning ( "Parse XML (hint=%d): %d items have incomplete records", int( hint ), incomplete );

	return inv;
}



QDomElement BrickLink::createItemListXML ( QDomDocument doc, ItemListXMLHint hint, const InvItemList *items, QMap <QString, QString> *extra )
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
		
		if (( ii-> status ( ) == InvItem::Exclude ) && ( hint != XMLHint_BrickStore && hint != XMLHint_BrikTrak ))
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
				case InvItem::Exclude: cb = 0; break;
				case InvItem::Include: cb = 1; break;
				case InvItem::Extra  : cb = 3; break;
				case InvItem::Unknown: cb = 5; break;
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
				case InvItem::Unknown: st = "?"; break;
				case InvItem::Extra  : st = "E"; break;
				case InvItem::Exclude: st = "X"; break;
				case InvItem::Include:
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
			if ( ii-> reserved ( ))
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
				item. appendChild ( doc. createElement ( "OrigPrice"  ). appendChild ( doc. createTextNode ( cLocale ( ). toString ( ii-> weight ( ), 'f', 4 ))). parentNode ( ));
			if ( ii-> origQuantity ( ) != ii-> quantity ( ))
				item. appendChild ( doc. createElement ( "OrigQty"  ). appendChild ( doc. createTextNode ( cLocale ( ). toString ( ii-> weight ( ), 'f', 4 ))). parentNode ( ));
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
			if ( ii-> reserved ( ))
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
		}

		// ### INVENTORY REQUEST ###
		else if ( hint == XMLHint_Inventory ) {
			item. appendChild ( doc. createElement ( "ITEMID"    ). appendChild ( doc. createTextNode ( QString ( ii-> item ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "ITEMTYPE"  ). appendChild ( doc. createTextNode ( QChar ( ii-> itemType ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "COLOR"     ). appendChild ( doc. createTextNode ( QString::number ( ii-> color ( )-> id ( )))). parentNode ( ));
			item. appendChild ( doc. createElement ( "QTY"       ). appendChild ( doc. createTextNode ( QString::number ( ii-> quantity ( )))). parentNode ( ));
			
			if ( ii-> status ( ) == InvItem::Extra )
				item. appendChild ( doc. createElement ( "EXTRA" ). appendChild ( doc. createTextNode ( "Y" )). parentNode ( ));
		}

		// optional: additonal tags
		if ( extra ) {
			for ( QMap <QString, QString>::Iterator it = extra-> begin ( ); it != extra-> end ( ); ++it )
				item. appendChild ( doc. createElement ( it. key ( )). appendChild ( doc. createTextNode ( it. data ( ))). parentNode ( ));
		}
	}

	return root;
}



bool BrickLink::parseLDrawModel ( QFile &f, InvItemList &items, uint *invalid_items )
{
	QDict <InvItem> mergehash;
	
	return BrickLink::parseLDrawModelInternal ( f, QString::null, items, invalid_items, mergehash );
}

bool BrickLink::parseLDrawModelInternal ( QFile &f, const QString &model_name, InvItemList &items, uint *invalid_items, QDict <InvItem> &mergehash )
{
	QStringList searchpath;
	QString ldrawdir = CConfig::inst ( )-> lDrawDir ( );
	uint invalid = 0;
	int linecount = 0;

	bool is_mpd = false;
	int current_mpd_index = -1;
	QString current_mpd_model;
	bool is_mpd_model_found = false;


	searchpath. append ( QFileInfo ( f ). dirPath ( true ));
	if ( !ldrawdir. isEmpty ( )) {
		searchpath. append ( ldrawdir + "/models" );
	}

	if ( f. isOpen ( )) {
		QTextStream in ( &f );
		QString line;
		
		while (( line = in. readLine ( ))) {
			linecount++;

			if ( !line. isEmpty ( ) && line [0] == '0' ) {
				QStringList sl = QStringList::split ( ' ', line. simplifyWhiteSpace ( ));

				if (( sl. count ( ) >= 2 ) && ( sl [1] == "FILE" )) {
					is_mpd = true;
					current_mpd_model = sl [2]. lower ( );
					current_mpd_index++;

					if ( is_mpd_model_found )
						break;

					if (( current_mpd_model == model_name. lower ( )) || ( model_name. isEmpty ( ) && ( current_mpd_index == 0 )))
						is_mpd_model_found = true;

					if ( !f. isDirectAccess ( ))
						return false; // we need to seek!
				}
			}
			else if ( !line. isEmpty ( ) && line [0] == '1' ) {
				if ( is_mpd && !is_mpd_model_found )
					continue;

				QStringList sl = QStringList::split ( ' ', line. simplifyWhiteSpace ( ));
				
				if ( sl. count ( ) >= 15 ) {
					int colid = sl [1]. toInt ( );
					QString partname = sl [14]. lower ( );
					
					QString partid = partname;
					partid. truncate ( partid. findRev ( '.' ));
										
					const Color *colp = colorFromLDrawId ( colid );
					const Item *itemp = item ( 'P', partid );

					
					if ( !itemp ) {
						bool got_subfile = false;
					
						if ( is_mpd ) {
							uint sub_invalid_items = 0;

							QFile::Offset oldpos = f. at ( );
							f. at ( 0 );

							got_subfile = parseLDrawModelInternal ( f, partname, items, &sub_invalid_items, mergehash );

							invalid += sub_invalid_items;
							f. at ( oldpos );
						}

						if ( !got_subfile ) {
							for ( QStringList::iterator it = searchpath. begin ( ); it != searchpath. end ( ); ++it ) {
								QFile subf ( *it + "/" + partname );
								
								if ( subf. open ( IO_ReadOnly )) {
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
void BrickLink::setDatabase_ConsistsOf ( const QMap<const Item *, InvItemList> &map )
{
	for ( QMap<const Item *, InvItemList>::const_iterator it = map. begin ( ); it != map. end ( ); ++it )
		it. key ( )-> setConsistsOf ( it. data ( ));
}

void BrickLink::setDatabase_AppearsIn ( const QMap<const Item *, Item::AppearsInMap> &map )
{
	for ( QMap<const Item *, Item::AppearsInMap>::const_iterator it = map. begin ( ); it != map. end ( ); ++it )
		it. key ( )-> setAppearsIn ( it. data ( ));
}

void BrickLink::setDatabase_Basics ( const QIntDict<Color> &colors, 
									 const QIntDict<Category> &categories,
									 const QIntDict<ItemType> &item_types,
									 const QPtrVector<Item> &items )
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

bool BrickLink::writeDatabase ( const QString &fname )
{
	QString filename = fname. isNull ( ) ? dataPath ( ) + defaultDatabaseName ( ) : fname;

	QFile f ( filename + ".new" );
	if ( f. open ( IO_WriteOnly )) {
		QDataStream ds ( &f );

		ds << Q_UINT32( 0 /*magic*/ ) << Q_UINT32 ( 0 /*filesize*/ ) << Q_UINT32( DEFAULT_DATABASE_VERSION /*version*/ );
		
		ds. setByteOrder ( QDataStream::LittleEndian );

		// colors
		Q_UINT32 colc = m_databases. colors. count ( );
		ds << colc;
		
		for ( QIntDictIterator<Color> it ( m_databases. colors ); it. current ( ); ++it )
			ds << it. current ( );

		// categories
		Q_UINT32 catc = m_databases. categories. count ( );
		ds << catc;

		for ( QIntDictIterator<Category> it ( m_databases. categories ); it. current ( ); ++it )
			ds << it. current ( );
		
		// types
		Q_UINT32 ittc = m_databases. item_types. count ( );
		ds << ittc;
		
		for ( QIntDictIterator<ItemType> it ( m_databases. item_types ); it. current ( ); ++it )
			ds << it. current ( );

		// items
		Q_UINT32 itc = m_databases. items. count ( );
		ds << itc;

		Item **itp = m_databases. items. data ( );
		for ( Q_UINT32 i = itc; i; itp++, i-- )
			ds << *itp;

		ds << ( colc + ittc + catc + itc );
		ds << Q_UINT32( 0xb91c5703 );

		Q_UINT32 filesize = f. at ( );

		if ( f. status ( ) == IO_Ok ) {
			f. close ( );

			if ( f. open ( IO_ReadWrite )) {
				QDataStream ds2 ( &f );
				ds2 << Q_UINT32( 0xb91c5703 ) << filesize;

				if ( f. status ( ) == IO_Ok ) {
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
