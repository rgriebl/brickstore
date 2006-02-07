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
#include <stdlib.h>

#include <qfileinfo.h>
#include <qtimer.h>
#include <qpixmapcache.h>

#include "bricklink.h"

// directly use C-library str* functions instead of Qt's qstr* function
// to improve runtime performance

#if defined( Q_OS_WIN32 )
#define strdup _strdup
#endif



const QPixmap BrickLink::Picture::pixmap ( ) const
{
	QPixmap p;

	if ( !QPixmapCache::find ( m_key, p )) {
		p. convertFromImage ( m_image );
		QPixmapCache::insert ( m_key, p );
	}
	return p;
}
        

BrickLink::Picture *BrickLink::picture ( const Item *item, const BrickLink::Color *color, bool high_priority )
{
	if ( !item )
		return 0;

	QCString key;
	if ( color )
		key. sprintf ( "%c@%s@%d", item-> itemType ( )-> pictureId ( ), item-> id ( ), color-> id ( ));
	else
		key. sprintf ( "%c@%s", item-> itemType ( )-> pictureId ( ), item-> id ( ));
		
	Picture *pic = m_pictures. cache [key];
	bool need_to_load = false;

	if ( !pic ) {
		pic = new Picture ( item, color, key );
		if ( color )
			m_pictures. cache. insert ( key, pic );
		need_to_load = true;
	}

	if ( high_priority ) {
		if ( !pic-> valid ( ))
			pic-> load_from_disk ( );

		if ( !pic-> valid ( ) || updateNeeded ( pic-> lastUpdate ( ), m_pictures. update_iv ))
			updatePicture ( pic, high_priority );
	}
	else if ( need_to_load ) {
		pic-> addRef ( );
		m_pictures. diskload. append ( pic );

		if ( m_pictures. diskload. count ( ) == 1 )
			QTimer::singleShot ( 0, BrickLink::inst ( ), SLOT( pictureIdleLoader ( )));
	}

	return pic;
}

BrickLink::Picture *BrickLink::largePicture ( const Item *item, bool high_priority )
{
	if ( !item )
		return 0;
	return picture ( item, 0, high_priority );
}

// loading pictures directly when creating Picture objects locks
// up the machine. Loading only one per idle loop generates nearly
// NO cpu usage, but it is very slow.
// Loading 3 pictures per idle loop is a good compromise.

void BrickLink::pictureIdleLoader ( )
{
	for ( int i = 0; i < 3; i++ )
		pictureIdleLoader2 ( );

	if ( !m_pictures. diskload. isEmpty ( ))
		QTimer::singleShot ( 0, this, SLOT( pictureIdleLoader ( )));
}

void BrickLink::pictureIdleLoader2 ( )
{
	Picture *pic = 0;

	while ( !m_pictures. diskload. isEmpty ( )) {
		pic = m_pictures. diskload. take ( 0 );
	
		if ( !pic ) {
			continue;
		}
		// already loaded? .. or .. nobody listening? 
		else if ( pic-> valid ( ) || ( pic-> refCount ( ) == 1 )) {
			pic-> release ( );
			pic = 0;
			continue;
		}	
		else {
			break;
		}
	}
	
	if ( pic ) {
		pic-> load_from_disk ( );

		if ( !pic-> valid ( ) || updateNeeded ( pic-> lastUpdate ( ), m_pictures. update_iv ))
			updatePicture ( pic, false );
		else
			emit pictureUpdated ( pic );
	
		pic-> release ( );
	}
}


BrickLink::Picture::Picture ( const Item *item, const Color *color, const char *key )
{
	m_item = item;
	m_color = color;

	m_valid = false;
	m_update_status = Ok;
	m_key = key ? strdup ( key ) : 0;
}

BrickLink::Picture::~Picture ( )
{
	if ( m_key ) {
		QPixmapCache::remove ( m_key );
		free ( m_key );
	}
}

void BrickLink::Picture::load_from_disk ( )
{
	QString path;

	bool large = ( !m_color );

	if ( !large && m_item-> itemType ( )-> hasColors ( ))
		path = BrickLink::inst ( )-> dataPath ( m_item, m_color );
	else
		path = BrickLink::inst ( )-> dataPath ( m_item );

	if ( path. isEmpty ( ))
		return;
	path += large ? "large.png" : "small.png";

	QFileInfo fi ( path );

	if ( fi. exists ( )) {
		if ( fi. size ( ) > 0 ) {
			m_valid = m_image. load ( path );
		}
		else {
			if ( !large && m_item && m_item-> itemType ( ))
				m_image = BrickLink::inst ( )-> noImage ( m_item-> itemType ( )-> imageSize ( ))-> convertToImage ( );
			else
				m_image. create ( 0, 0, 32 );

			m_valid = true;
		}

		m_fetched = fi. lastModified ( );
	}
	else
		m_valid = false;

	if ( m_valid && !large && m_image. depth ( ) > 8 ) {
		m_image = m_image. convertDepth ( 8 );
	}

	QPixmapCache::remove ( m_key );
}


void BrickLink::Picture::update ( bool high_priority )
{
	BrickLink::inst ( )-> updatePicture ( this, high_priority );
}

void BrickLink::updatePicture ( BrickLink::Picture *pic, bool high_priority )
{
	if ( !pic || ( pic-> m_update_status == Updating ))
		return;

	if ( !m_online ) {
		pic-> m_update_status = UpdateFailed;
		emit pictureUpdated ( pic );
		return;
	}

	pic-> m_update_status = Updating;
	pic-> addRef ( );

	bool large = ( !pic-> color ( ));

	QCString url;
	CKeyValueList query;
	
	if ( large ) {
		url. sprintf ( "http://www.bricklink.com/%cL/%s.jpg", pic-> item ( )-> itemType ( )-> pictureId ( ), pic-> item ( )-> id ( ));
	}
	else {
		url = "http://www.bricklink.com/getPic.asp";
		// ?itemType=%c&colorID=%d&itemNo=%s", pic-> item ( )-> itemType ( )-> pictureId ( ), pic-> color ( )-> id ( ), pic-> item ( )-> id ( ));

		query << CKeyValue ( "itemType", QChar ( pic-> item ( )-> itemType ( )-> pictureId ( )))
		      << CKeyValue ( "colorID",  QString::number ( pic-> color ( )-> id ( )))
			  << CKeyValue ( "itemNo",   pic-> item ( )-> id ( ));
	}

	//qDebug ( "PIC request started for %s", (const char *) url );
	m_pictures. transfer-> get ( url, query, 0, pic, high_priority );
}


void BrickLink::pictureJobFinished ( CTransfer::Job *j )
{
	if ( !j || !j-> data ( ) || !j-> userObject ( ))
		return;

	Picture *pic = static_cast <Picture *> ( j-> userObject ( ));
	bool large = ( !pic-> color ( ));

	pic-> m_update_status = UpdateFailed;

	if ( !j-> failed ( )) {
		QString path;
		QImage img;

		if ( !large && pic-> item ( )-> itemType ( )-> hasColors ( ))
			path = BrickLink::inst ( )-> dataPath ( pic-> item ( ), pic-> color ( ));
		else
			path = BrickLink::inst ( )-> dataPath ( pic-> item ( ));

		if ( !path. isEmpty ( )) {
			path. append ( large ? "large.png" : "small.png" );

			pic-> m_update_status = Ok;

			if (( j-> effectiveUrl ( ). find ( "noimage", 0, false ) == -1 ) && j-> data ( )-> size ( ) && img. loadFromData ( *j-> data ( ))) {
				if ( !large )
					img = img. convertDepth ( 8 );
				img. save ( path, "PNG" );
			}
			else {
				QFile f ( path );
				f. open ( IO_WriteOnly | IO_Truncate );
				f. close ( );

				qWarning ( "No image !" );
			}

			pic-> load_from_disk ( );
		}
		else {
			qWarning ( "Couldn't get path to save image" );
		}
	}
	else if ( large && ( j-> responseCode ( ) == 404 ) && ( j-> url ( ). right ( 3 ) == "jpg" )) {
		// no large JPG image -> try a GIF image instead

		pic-> m_update_status = Updating;

	    QCString url = j-> url ( );
	    url. replace ( url. length ( ) - 3, 3, "gif" );

	    //qDebug ( "PIC request started for %s", (const char *) url );
	    m_pictures. transfer-> get ( url, CKeyValueList ( ), 0, pic );
		return;
	}
	else
		qWarning ( "Image download failed" );

	emit pictureUpdated ( pic );
	pic-> release ( );
}

