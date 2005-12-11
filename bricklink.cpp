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
#include "cutility.h"
#include "bricklink.h"


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

		default:
			break;
	}
	return url;
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
	
	if ( col-> isTransparent ( )) {
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
	p += item_type-> m_id;
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
	p += QString::number ( color-> m_id );
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
			if (!( s_inst-> m_databases. transfer-> init ( ) &&
			       s_inst-> m_inventories. transfer-> init ( ) &&
			       s_inst-> m_pictures. transfer-> init ( ) &&
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

	m_databases. colors. resize ( 151 );
	m_databases. categories. resize ( 503 );
	m_databases. item_types. resize ( 13 );

	m_databases. colors. setAutoDelete ( true );
	m_databases. categories. setAutoDelete ( true );
	m_databases. item_types. setAutoDelete ( true );
	m_databases. items. setAutoDelete ( true );

	m_databases. transfer = new CTransfer ( );
	m_databases. updating = false;
	m_databases. updatefail = 0;
	m_databases. update_iv = 0;

	m_price_guides. transfer = new CTransfer ( );
	m_price_guides. update_iv = 0;

	m_inventories. transfer = new CTransfer ( );
	m_inventories. update_iv = 0;

	m_pictures. transfer = new CTransfer ( );
	m_pictures. update_iv = 0;

	QPixmapCache::setCacheLimit ( 20 * 1024 * 1024 );  // 80 x 60 x 32 (w x h x bpp) == 20kB -> room for ~1000 pixmaps

	connect ( m_databases.    transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( databaseJobFinished   ( CTransfer::Job * )));
	connect ( m_price_guides. transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( priceGuideJobFinished ( CTransfer::Job * )));
	connect ( m_inventories.  transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( inventoryJobFinished  ( CTransfer::Job * )));
	connect ( m_pictures.     transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( pictureJobFinished    ( CTransfer::Job * )));

	connect ( m_inventories.  transfer, SIGNAL( progress ( int, int )), this, SIGNAL( inventoryProgress ( int, int )));
	connect ( m_pictures.     transfer, SIGNAL( progress ( int, int )), this, SIGNAL( pictureProgress ( int, int )));
	connect ( m_price_guides. transfer, SIGNAL( progress ( int, int )), this, SIGNAL( priceGuideProgress ( int, int )));
}

BrickLink::~BrickLink ( )
{
	cancelInventoryTransfers ( );
	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	delete m_databases. transfer;
	delete m_inventories. transfer;
	delete m_pictures. transfer;
	delete m_price_guides. transfer;

	s_inst = 0;
}

void BrickLink::setHttpProxy ( bool enable, const QString &name, int port )
{
	QCString aname = name. latin1 ( );

	m_databases.    transfer-> setProxy ( enable, aname, port );
	m_inventories.  transfer-> setProxy ( enable, aname, port );
	m_pictures.     transfer-> setProxy ( enable, aname, port );
	m_price_guides. transfer-> setProxy ( enable, aname, port );
}

void BrickLink::setUpdateIntervals ( int db, int inv, int pic, int pg )
{
	m_databases.    update_iv = db;
	m_inventories.  update_iv = inv;
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


namespace {

static bool modified_strncmp ( const char *s1, const char *s2, int len )
{
	while ( *s1 && ( len-- > 0 )) {
		if ( *s1++ != *s2++ )
			return false;
	}
	return ( len == 0 ) && ( *s1 == 0 );
}

} // namespace

const BrickLink::Category *BrickLink::category ( const char *name, int len ) const
{
	if ( !name || !name [0] )
		return 0;
	if ( len < 0 )
		len = strlen ( name );

	for ( QIntDictIterator<Category> it ( m_databases. categories ); it. current ( ); ++it ) {
		if ( modified_strncmp ( it. current ( )-> name ( ), name, len ))
			return it. current ( );
	}
	return 0;
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

void BrickLink::cancelInventoryTransfers ( )
{
	m_inventories. transfer-> cancelAllJobs ( );
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

void BrickLink::databaseJobFinished ( CTransfer::Job *j )
{
	if ( !j || !j-> file ( ))
		return;

	QString spath = static_cast <QFile *> ( j-> file ( ))-> name ( );

	// itemtype U generates a 500 instead of an empty list .. sigh
	if ( !j-> failed ( ) || ( j-> responseCode ( ) == 500 )) {
		if ( spath. right ( 4 ) == ".new" ) {
			QString dpath = spath;
			dpath. truncate ( dpath. length ( ) - 4 );

			if ( j-> file ( )-> isOpen ( ))
				j-> file ( )-> close ( );

			QFile::remove ( dpath );

			if ( j-> responseCode ( ) == 500 ) {
				QFile::remove ( spath );

				QFile f ( dpath );
				f. open ( IO_WriteOnly | IO_Truncate );
				f. close ( );
			}
			else {
				QDir d;
				d. rename ( spath, dpath );
			}
		}
	}
	else {
		qWarning ( "Database update failed for \"%s\" - error: %s", j-> url ( ). data ( ), j-> errorString ( ). latin1 ( ));

		m_databases. updatefail++;
	}
	QString *msg = static_cast <QString *> ( j-> userObject ( ));

	m_databases. progress_now++;
	emit databaseProgress ( m_databases. progress_now, m_databases. progress_max, msg ? *msg : QString::null );

	if ( msg )
		delete msg;
}


void BrickLink::databaseDonePart1 ( )
{
	if ( m_databases. updatefail )
		qWarning ( "Database update part 1 failed for some or all files" );

	QString path = dataPath ( );

	QIntDict <ItemType> temp_item_types;
	if ( !readDB <> ( path + "itemtypes.txt", temp_item_types ))
		m_databases. updatefail++;

	disconnect ( m_databases. transfer, SIGNAL( done ( )), this, SLOT( databaseDonePart1 ( )));
	connect    ( m_databases. transfer, SIGNAL( done ( )), this, SLOT( databaseDonePart2 ( )));

	m_databases. progress_max /= 2; // we are only at 50% now

	int request_count = 0;

	for ( QIntDictIterator<ItemType> it ( temp_item_types ); it. current ( ); ++it ) {
		char c = it. current ( )-> m_id;

		QFile *f = new QFile ( path + "items_" + c + ".txt.new" );

		if ( f-> open ( IO_WriteOnly )) {
			CKeyValueList query;  //?a=a&viewType=0&itemType=" ) + c
			query << CKeyValue ( "a", "a" )
			      << CKeyValue ( "viewType", "0" )
			      << CKeyValue ( "itemType", QChar ( c ))
			      << CKeyValue ( "selItemColor", "Y" )  // special BrickStore flag to get default color - thanks Dan
			      << CKeyValue ( "selWeight", "Y" )
			      << CKeyValue ( "selYear", "Y" );


			m_databases. transfer-> get ( "http://www.bricklink.com/catalogDownload.asp", query, f, new QString ( tr( "Items of type: %1" ). arg ( it. current ( )-> m_name )));
			request_count++;

			m_databases. progress_max++;
		}
		else {
			qWarning ( "Database update failed for \"item_%c.txt.new\": %s", c, f-> errorString ( ). latin1 ( ));
			delete f;

			m_databases. updatefail++;
		}
	}
	emit databaseProgress ( m_databases. progress_now, m_databases. progress_max, QString::null );

	if ( !request_count )
		databaseDonePart2 ( );
}

void BrickLink::databaseDonePart2 ( )
{
	if ( m_databases. updatefail )
		qWarning ( "Database update part 2 failed for some or all files" );

	disconnect ( m_databases. transfer, SIGNAL( done ( )), this, SLOT( databaseDonePart2 ( )));

	qDebug ( "Database update finished" );

	readDatabase ( );

	m_databases. progress_max = m_databases. progress_now = 0;
	emit databaseProgress ( 0, 0, QString::null );

	m_databases. updating = false;
	emit databaseUpdated (( m_databases. updatefail ), ( m_databases. updatefail ) ?
	                       tr( "Not all BrickLink database were successfully updated (%1 failed)." ). arg ( m_databases. updatefail ) :
	                       tr( "Updating the BrickLink database files was successfull." ));
}

bool BrickLink::updateDatabase ( )
{
	if ( !m_online )
		return false;

	if ( m_databases. updating )
		return false;

	QString path = dataPath ( );

	m_databases. updating = true;
	m_databases. updatefail = 0;
	m_databases. progress_max = m_databases. progress_now = 0;

	struct {
		const char *m_url;
		const CKeyValueList m_query;
		const char *m_file;
		const char *m_label;
	} * tptr, table [] = {
		{ "http://www.bricklink.com/catalogDownload.asp" /*?a=a&viewType=1"*/, CKeyValueList ( ) << CKeyValue ( "a", "a" )<< CKeyValue ( "viewType", "1" ), "itemtypes.txt",  QT_TR_NOOP( "Item types" )      },
		{ "http://www.bricklink.com/catalogDownload.asp" /*?a=a&viewType=2"*/, CKeyValueList ( ) << CKeyValue ( "a", "a" )<< CKeyValue ( "viewType", "2" ), "categories.txt", QT_TR_NOOP( "Categories" )      },
		{ "http://www.bricklink.com/catalogDownload.asp" /*?a=a&viewType=3"*/, CKeyValueList ( ) << CKeyValue ( "a", "a" )<< CKeyValue ( "viewType", "3" ), "colors.txt",     QT_TR_NOOP( "Colors" )          },
		{ "http://www.bricklink.com/catalogColors.asp",   CKeyValueList ( ),  "colorguide.html",    QT_TR_NOOP( "Color guide" )     },
//		{ "http://www.peeron.com/inv/colors",             CKeyValueList ( ),  "peeron_colors.html", QT_TR_NOOP( "Peeron color table" ) },
		{ "http://brickforge.de/brickstore-data/peeron-colorchart.html", CKeyValueList ( ),  "peeron_colors.html", QT_TR_NOOP( "Peeron color table" ) },
		{ "http://www.bricklink.com/btinvlist.asp",       CKeyValueList ( ),  "btinvlist.txt",      QT_TR_NOOP( "Inventory list" )  },
		{ "http://www.bricklink.com/images/noImage.gif",  CKeyValueList ( ),  "noimage.gif",        QT_TR_NOOP( "Default image 1" ) },
		{ "http://www.bricklink.com/images/noImageM.gif", CKeyValueList ( ),  "noimageM.gif",       QT_TR_NOOP( "Default Image 2" ) },
		{ 0, CKeyValueList ( ), 0, 0 }
	};

	connect ( m_databases. transfer, SIGNAL( done ( )), this, SLOT( databaseDonePart1 ( )));

	int request_count = 0;

	for ( tptr = table; tptr-> m_url; tptr++ ) {
		QFile *f = new QFile ( path + tptr-> m_file + ".new" );

		if ( f-> open ( IO_WriteOnly )) {
			m_databases. transfer-> get ( tptr-> m_url, tptr-> m_query, f, new QString ( tr( tptr-> m_label )));
			request_count++;

			m_databases. progress_max += 2; // we only want to achieve 50% in part 1
			emit databaseProgress ( m_databases. progress_now, m_databases. progress_max, QString::null );
		}
		else {
			qWarning ( "Database update failed for \"%s\": %s", tptr-> m_file, f-> errorString ( ). latin1 ( ));
			delete f;

			m_databases. updatefail++;
		}
	}

	// done() will not be triggered in this case...
	if ( !request_count )
		QTimer::singleShot ( 0, this, SLOT( databaseDonePart2 ( )));

	return true;
}


#if 0

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

#endif


bool BrickLink::readDatabase ( )
{
	QString path = dataPath ( );
	bool ok = true;

	cancelInventoryTransfers ( );
	cancelPictureTransfers ( );
	cancelPriceGuideTransfers ( );

	m_price_guides. cache. clear ( );
	m_inventories. cache. clear ( );
	m_pictures. cache. clear ( );
	QPixmapCache::clear ( );

	m_databases. colors. clear ( );
	m_databases. item_types. clear ( );
	m_databases. categories. clear ( );
	m_databases. items. clear ( );

	ok &= m_databases. noimage. load ( dataPath ( ) + "noimage.gif" );
	ok &= m_databases. noimageM. load ( dataPath ( ) + "noimageM.gif" );

	ok &= readDB <> ( path + "colors.txt",     m_databases. colors );
	ok &= readDB <> ( path + "itemtypes.txt",  m_databases. item_types );
	ok &= readDB <> ( path + "categories.txt", m_databases. categories );

	ok &= readColorGuide ( path + "colorguide.html" );
	ok &= readPeeronColors ( path + "peeron_colors.html" );

	// hack to speed up loading (exactly 36759 items on 22.08.2005)
	m_databases. items. resize ( 37000 );

	for ( QIntDictIterator<ItemType> it ( m_databases. item_types ); it. current ( ); ++it )
		ok &= readDB <> ( path + "items_" + char( it. current ( )-> m_id ) + ".txt", m_databases. items, it. current ( ));

	qsort ( m_databases. items. data ( ), m_databases. items. count ( ), sizeof( Item * ), (int (*) ( const void *, const void * )) Item::compare );

	Item **itp = m_databases. items. data ( );

	for ( uint i = m_databases. items. count ( ); i; itp++, i-- ) {
		ItemType *itt = const_cast <ItemType *> (( *itp )-> m_item_type );

		if ( itt ) {
			for ( const Category **catp = ( *itp )-> m_categories; *catp; catp++ ) {
				if ( itt-> m_categories. findRef ( *catp ) < 0 )
					itt-> m_categories. append ( *catp );
			}
		}
	}

	btinvlist_dummy btinvlist_dummy;
	ok &= readDB <> ( path + "btinvlist.txt", btinvlist_dummy );

	if ( !ok )
		qWarning ( "Error reading databases !" );

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

	return ok;
}

template <typename T> bool BrickLink::readDB_processLine ( QIntDict<T> &d, uint tokencount, const char **tokens, void *userdata )
{
	T *t = T::parse ( this, tokencount, (const char **) tokens, userdata );

	if ( t ) {
		d. insert ( t-> id ( ), t );
		return true;
	}
	else
		return false;
}

template <typename T> bool BrickLink::readDB_processLine ( QPtrVector<T> &v, uint tokencount, const char **tokens, void *userdata )
{
	T *t = T::parse ( this, tokencount, (const char **) tokens, userdata );

	if ( t ) {
		if ( v. size ( ) == v. count ( ))
 			v. resize ( v. size ( ) + QMAX( 40, QMIN( 320, v. size ( ))));
		v. insert ( v. count ( ), t );
		return true;
	}
	else
		return false;
}

bool BrickLink::readDB_processLine ( btinvlist_dummy & /*dummy*/, uint count, const char **strs, void * )
{
	if ( count < 2 || !strs [0][0] || !strs [1][0] )
		return false;

	const BrickLink::Item *itm = item ( strs [0][0], strs [1] );

	if ( itm ) {
		QDateTime dt;
		dt. setTime_t ( 0 );

		if ( strs [2][0] ) {
			char ampm;
			int d, m, y, hh, mm, ss;

			if ( sscanf ( strs [2], "%d/%d/%d %d:%d:%d %cM", &m, &d, &y, &hh, &mm, &ss, &ampm ) == 7 ) {
				dt. setDate ( QDate ( y, m, d ));
				if ( ampm == 'P' )
					hh += ( hh == 12 ) ? -12 : 12;
				dt. setTime ( QTime ( hh, mm, ss ));
			}
		}

		const_cast <Item *> ( itm )-> m_inv_updated = dt;
		const_cast <ItemType *> ( itm-> m_item_type )-> m_has_inventories = true;
	}
	else
		qWarning ( "WARNING: parsing btinvlist: item %s [%c] doesn't exist!", strs [1], strs [0][0] );

	return true;
}


template <typename C> bool BrickLink::readDB ( const QString &name, C &container, void *userdata )
{
	// plain C is way faster than Qt on top of C++
	// and this routine has to be fast to reduce the startup time

	FILE *f = fopen ( name, "r" );
	if ( f ) {
		char line [1000];
		int lineno = 1;

		(void) fgets ( line, 1000, f ); // skip header

		while ( !feof ( f )) {
			if ( !fgets ( line, 1000, f ))
				break;

			lineno++;

			if ( !line [0] || line [0] == '#' || line [0] == '\r' || line [0] == '\n' )
				continue;

			char *tokens [20];
			uint tokencount = 0;

			for ( char *pos = line; tokencount < 20; ) {
				char *newpos = pos;

				while ( *newpos && ( *newpos != '\t' ) && ( *newpos != '\r' ) && ( *newpos != '\n' ))
					newpos++;

				bool stop = ( *newpos != '\t' );

				tokens [tokencount++] = pos;

				*newpos = 0;
				pos = newpos + 1;

				if ( stop )
					break;
			}

			if ( !readDB_processLine ( container, tokencount, (const char **) tokens, userdata )) {
				qWarning ( "ERROR: parse error in file \"%s\", line %d: \"%s\"", name. latin1 ( ), lineno, (const char *) line );
				return false;
			}
		}

		fclose ( f );
		return true;
	}
	qWarning ( "ERROR: could not open file \"%s\"", name. latin1 ( ));
	return false;
}

bool BrickLink::readColorGuide ( const QString &name )
{
	QFile f ( name );
	if ( f. open ( IO_ReadOnly )) {
		QTextStream ts ( &f );
		QString s = ts. read ( );
		f. close ( );

		QRegExp rxp ( ">([0-9]+)&nbsp;</TD><TD[ ]+BGCOLOR=\"#?([a-fA-F0-9]{6,6})\">" );

		int pos = 0;

		while (( pos = rxp. search ( s, pos )) != -1 ) {
			int id = rxp. cap ( 1 ). toInt ( );

			Color *colp = m_databases. colors [id];
			if ( colp )
				colp-> m_color = QColor ( "#" + rxp. cap ( 2 ));

			pos += rxp. matchedLength ( );
		}
		return true;
	}
	return false;
}


bool BrickLink::readPeeronColors ( const QString &name )
{
	QFile f ( name );
	if ( f. open ( IO_ReadOnly )) {
		QTextStream in ( &f );
		
		QString line;
		int count = 0;

		QRegExp namepattern ( "<a href=[^>]+>(.+)</a>" );
		
		while (( line = in. readLine ( ))) {
			if ( line. startsWith ( "<td>" ) && line. endsWith ( "</td>" )) {
				QString tmp = line. mid ( 4, line. length ( ) - 9 );
				QStringList sl = QStringList::split ( "</td><td>", tmp, true );

				bool line_ok = false;
				
				if ( sl. count ( ) >= 5 ) {
					int ldraw_id = -1, id = -1;
					QString peeron_name;
				
					if ( !sl [3]. isEmpty ( ))
						id = sl [3]. toInt ( );
					if ( !sl [4]. isEmpty ( ))
						ldraw_id = sl [4]. toInt ( );
					
					if ( namepattern. exactMatch ( sl [0] ))
						peeron_name = namepattern. cap ( 1 );

					if ( id != -1 ) {
						Color *colp = m_databases. colors [id];
						if ( colp ) {
							if ( !peeron_name. isEmpty ( ))
								colp-> m_peeron_name = strdup ( peeron_name. latin1 ( ));
							colp-> m_ldraw_id = ldraw_id;

							count++;			
						}
					}					
					line_ok = true;
				}
				
				if ( !line_ok )
					qWarning ( "Failed to parse item line: %s", line. latin1 ( ));
			}
		}

		return ( count > 0 );
	}
	return false;
}


QPtrList<BrickLink::InvItem> *BrickLink::parseItemListXML ( QDomElement root, ItemListXMLHint hint, uint *invalid_items )
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

	QPtrList<BrickLink::InvItem> *inv = new QPtrList<InvItem> ( );
	inv-> setAutoDelete ( true );

	uint incomplete = 0;

	for ( QDomNode itemn = root. firstChild ( ); !itemn. isNull ( ); itemn = itemn. nextSibling ( )) {
		if ( itemn. nodeName ( ) != itemtag )
			continue;

		InvItem *ii = new InvItem ( 0, 0 );

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
				else if ( tag == "LOTID" )
					ii-> setLotId ( val. toUInt ( ));
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



QDomElement BrickLink::createItemListXML ( QDomDocument doc, ItemListXMLHint hint, const QPtrList<InvItem> *items, QMap <QString, QString> *extra )
{
	QString roottag, itemtag;

	switch ( hint ) {
		case XMLHint_BrikTrak  : roottag = "GRID"; itemtag = "ITEM"; break;
		case XMLHint_MassUpload:
		case XMLHint_MassUpdate:
		case XMLHint_WantedList:
		case XMLHint_Inventory : roottag = "INVENTORY"; itemtag = "ITEM"; break;
		case XMLHint_BrickStore: roottag = "Inventory"; itemtag = "Item"; break;
	}

	QDomElement root = doc. createElement ( roottag );
	
	if ( root. isNull ( ) || roottag. isNull ( ) || itemtag. isEmpty ( ) || !items )
		return root;

	for ( QPtrListIterator<InvItem> it ( *items ); it. current ( ); ++it ) {
		InvItem *ii = it. current ( );

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
			item. appendChild ( doc. createElement ( "LOTID" )). appendChild ( doc. createTextNode ( QString::number ( ii-> lotId ( ))). parentNode ( ));

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



bool BrickLink::parseLDrawModel ( QFile &f, QPtrList <InvItem> &items, uint *invalid_items )
{
	QDict <InvItem> mergehash;
	
	return BrickLink::parseLDrawModelInternal ( f, QString::null, items, invalid_items, mergehash );
}

bool BrickLink::parseLDrawModelInternal ( QFile &f, const QString &model_name, QPtrList <InvItem> &items, uint *invalid_items, QDict <InvItem> &mergehash )
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
