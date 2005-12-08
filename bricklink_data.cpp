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

#include "bricklink.h"

// directly use C-library str* functions instead of Qt's qstr* function
// to improve runtime performance

#if defined( Q_OS_WIN32 )
#define strdup _strdup
#endif


BrickLink::Color::Color ( ) : m_name ( 0 ), m_peeron_name ( 0 ) { }
BrickLink::Color::~Color ( ) { free ( m_name ); free ( m_peeron_name ); }

BrickLink::Color *BrickLink::Color::parse ( const BrickLink *, uint count, const char **strs, void * )
{
	if ( count < 2 )
		return 0;

	Color *col = new Color ( );
	col-> m_id   = strtol ( strs [0], 0, 10 );
	col-> m_name = strdup ( strs [1] );
	col-> m_ldraw_id    = -1;
	col-> m_is_trans    = ( strstr ( strs [1], "Trans-"    ) != 0 );
	col-> m_is_glitter  = ( strstr ( strs [1], "Glitter "  ) != 0 );
	col-> m_is_speckle  = ( strstr ( strs [1], "Speckle "  ) != 0 );
	col-> m_is_metallic = ( strstr ( strs [1], "Metallic " ) != 0 );
	col-> m_is_chrome   = ( strstr ( strs [1], "Chrome "   ) != 0 );
	return col;
}

BrickLink::ItemType::ItemType ( ) : m_name ( 0 ) { }
BrickLink::ItemType::~ItemType ( ) { free ( m_name ); }

BrickLink::ItemType *BrickLink::ItemType::parse ( const BrickLink *bl, uint count, const char **strs, void * )
{
	if ( count < 2 )
		return 0;

	char c = strs [0][0];
	ItemType *itt = new ItemType ( );
	itt-> m_id = c;
	itt-> m_picture_id = ( c == 'I' ) ? 'S' : c;
	itt-> m_name = strdup ( strs [1] );
	itt-> m_has_inventories = false;
	itt-> m_has_colors = ( c == 'P' || c == 'G' );
	itt-> m_has_weight = ( c == 'B' || c == 'P' || c == 'G' || c == 'S' );
	itt-> m_has_year   = ( c == 'B' || c == 'C' || c == 'G' || c == 'S' );
	itt-> m_noimage = ( c == 'M' ) ? bl-> noImageM ( ) : bl-> noImage ( );

	return itt;
}

BrickLink::Category::Category ( ) : m_name ( 0 ) { }
BrickLink::Category::~Category ( ) { free ( m_name ); }

BrickLink::Category *BrickLink::Category::parse ( const BrickLink *, uint count, const char **strs, void * )
{
	if ( count < 2 )
		return 0;

	Category *cat = new Category ( );
	cat-> m_id   = strtol ( strs [0], 0, 10 );
	cat-> m_name = strdup ( strs [1] );
	return cat;
}

BrickLink::Item::Item ( ) : m_categories ( 0 ) { }
BrickLink::Item::~Item ( ) { free ( m_categories ); }

BrickLink::Item *BrickLink::Item::parse ( const BrickLink *bl, uint count, const char **strs, void *itemtype )
{
	if ( count < 4 )
		return 0;

	Item *item = new Item ( );
	item-> m_item_type = (ItemType *) itemtype;
		
	// only allocate one block for name, id and category to speed up things

	const char *pos = strs [1];
	const Category *maincat = bl-> category ( strtol ( strs [0], 0, 10 ));
	
	if ( maincat && pos [0] ) {
		pos += strlen ( maincat-> name ( ));
	
		if ( *pos )
			pos += 3;
	}
	const char *auxcat = pos;

	uint catcount = 1;
	const Category *catlist [64] = { maincat };

	while (( pos = strstr ( pos, " / " ))) {
		if ( auxcat [0] ) {
			const Category *cat = bl-> category ( auxcat, pos - auxcat );
			if ( cat )
				catlist [catcount++] = cat;
			else {
				// The field separator ' / ' also appears in category names !!!
				pos += 3;
				continue;
			}
		}
		pos += 3;
		auxcat = pos;
	}
	if ( auxcat [0] ) {
		const Category *cat = bl-> category ( auxcat );
		if ( cat )
			catlist [catcount++] = cat;
		else
			qWarning ( "Invalid category name: %s", auxcat );
	}
	catlist [catcount++] = 0;

	uint catsize  = catcount * sizeof( const Category * );
	uint idsize   = strlen ( strs [2] ) + 1;
	uint namesize = strlen ( strs [3] ) + 1;

	void *ptr = malloc ( catsize + idsize + namesize );

	item-> m_categories = (const Category **) ptr;
	item-> m_id   = ((char *) ptr ) + catsize;
	item-> m_name = ((char *) ptr ) + catsize + idsize;
		
	memcpy ( item-> m_categories, catlist,  catsize );
	memcpy ( item-> m_id,         strs [2], idsize );
	memcpy ( item-> m_name,       strs [3], namesize );

	uint parsedfields = 4;

	if (( parsedfields < count ) && ( item-> m_item_type-> hasYearReleased ( ))) {
		item-> m_year = strtol ( strs [parsedfields], 0, 10 );
		parsedfields++;
	}
	else
		item-> m_year = 0;

	if (( parsedfields < count ) && ( item-> m_item_type-> hasWeight ( ))) {
		item-> m_weight = bl-> cLocale ( ). toFloat ( strs [parsedfields] );
		parsedfields++;
	}
	else
		item-> m_weight = 0;

	if ( parsedfields < count )
		item-> m_color = bl-> color ( strtol ( strs [parsedfields], 0, 10 ));
	else
		item-> m_color = 0;

	return item;
}

bool BrickLink::Item::hasCategory ( const BrickLink::Category *cat ) const
{
	for ( const Category **catp = m_categories; *catp; catp++ ) {
		if ( *catp == cat )
			return true;
	}
	return false;
}

int BrickLink::Item::compare ( const BrickLink::Item **a, const BrickLink::Item **b )
{
	int d = ( (*a)-> m_item_type-> id ( ) - (*b)-> m_item_type-> id ( ));

	return d ? d : qstrcmp ( (*a)-> m_id, (*b)-> m_id );
}




BrickLink::InvItem::InvItem ( const Color *color, const Item *item )
{
	m_item = item;
	m_color = color;
	
	m_custom_picture = 0;
	
	m_status = Include;
	m_condition = New;
	m_retain = m_stockroom = false;
	m_weight = 0;

	m_quantity = m_orig_quantity = 0;
	m_bulk_quantity = 1;
	m_price = m_orig_price = .0;
	m_sale = 0;

	
	for ( int i = 0; i < 3; i++ ) {
		m_tier_quantity [i] = 0;
		m_tier_price [i] = .0;
	}

	m_incomplete = 0;
	m_lot_id = 0;
}

BrickLink::InvItem::~InvItem ( )
{
	delete m_incomplete;
}

bool BrickLink::InvItem::mergeFrom ( const InvItem *from, bool prefer_from )
{
	if (( from == this ) ||
	    ( from-> item ( ) != item ( )) ||
	    ( from-> color ( ) != color ( )) ||
	    ( from-> condition ( ) != condition ( )))
		return false;

	if (( from-> price ( ) != 0 ) && (( price ( ) == 0 ) || prefer_from ))
		setPrice ( from-> price ( ));
	if (( from-> bulkQuantity ( ) != 1 ) && (( bulkQuantity ( ) == 1 ) || prefer_from ))
		setBulkQuantity ( from-> bulkQuantity ( ));
	if (( from-> sale ( )) && ( !( sale ( )) || prefer_from ))
		setSale ( from-> sale ( ));

	for ( int i = 0; i < 3; i++ ) {
		if (( from-> tierPrice ( i ) != 0 ) && (( tierPrice ( i ) == 0 ) || prefer_from ))
			setTierPrice ( i, from-> tierPrice ( i ));
		if (( from-> tierQuantity ( i )) && ( !( tierQuantity ( i )) || prefer_from ))
			setTierQuantity ( i, from-> tierQuantity ( i ));
	}
	
	if ( !from-> remarks ( ). isEmpty ( ) && ( remarks ( ). isEmpty ( ) || prefer_from ))
		setRemarks ( from-> remarks ( ));
	if ( !from-> comments ( ). isEmpty ( ) && ( comments ( ). isEmpty ( ) || prefer_from ))
		setComments ( from-> comments ( ));
	if ( !from-> reserved ( ). isEmpty ( ) && ( reserved ( ). isEmpty ( ) || prefer_from ))
		setReserved ( from-> reserved ( ));	
	if ( !from-> customPictureUrl ( ). isEmpty ( ) && ( customPictureUrl ( ). isEmpty ( ) || prefer_from ))
		setCustomPictureUrl ( from-> customPictureUrl ( ));
	
	if ( prefer_from ) {
		setStatus ( from-> status ( ));
		setRetain ( from-> retain ( ));
		setStockroom ( from-> stockroom ( ));
	}

	setQuantity ( quantity ( ) + from-> quantity ( ));
	setOrigQuantity ( origQuantity ( ) + from-> origQuantity ( ));

	return true;
}


QDataStream &operator << ( QDataStream &ds, const BrickLink::InvItem &ii )
{
	ds << QCString( ii. item ( ) ? ii. item ( )-> id ( ) : "" );
	ds << Q_INT8( ii. itemType ( ) ? ii. itemType ( )-> id ( ) : -1 );
	ds << Q_INT32( ii. color ( ) ? ii. color ( )-> id ( ) : 0xffffffff );
		
	ds << Q_INT32( ii. status ( )) << Q_INT32( ii. condition ( )) << ii. comments ( ) << ii. remarks ( ) << ii. customPictureUrl ( )
	   << ii. quantity ( ) << ii. bulkQuantity ( ) << ii. tierQuantity ( 0 ) << ii. tierQuantity ( 1 ) << ii. tierQuantity ( 2 )
	   << ii. price ( ) << ii. tierPrice ( 0 ) << ii. tierPrice ( 1 ) << ii. tierPrice ( 2 ) << ii. sale ( )
	   << Q_INT8( ii. retain ( ) ? 1 : 0 ) << Q_INT8( ii. stockroom ( ) ? 1 : 0 ) << ii. m_reserved
	   << ii. origQuantity ( ) << ii. origPrice ( );
	return ds;
}

QDataStream &operator >> ( QDataStream &ds, BrickLink::InvItem &ii )
{
	QCString itemid;
	Q_INT32 colorid = 0;
	Q_INT8 itemtypeid = 0;
	Q_INT8 retain = 0, stockroom = 0;

	ds >> itemid;
	ds >> itemtypeid;
	ds >> colorid;

	const BrickLink::Item *item = BrickLink::inst ( )-> item ( itemtypeid, itemid );

	ii. setItem ( item );		
	ii. setColor ( BrickLink::inst ( )-> color ( colorid ));
	
	Q_INT32 status = 0, cond = 0;
	
	ds >> status >> cond >> ii. m_comments >> ii. m_remarks >> ii. m_custom_picture_url
	   >> ii. m_quantity >> ii. m_bulk_quantity >> ii. m_tier_quantity [0] >> ii. m_tier_quantity [1] >> ii. m_tier_quantity [2]
	   >> ii. m_price >> ii. m_tier_price [0] >> ii. m_tier_price [1] >> ii. m_tier_price [2] >> ii. m_sale
	   >> retain >> stockroom >> ii. m_reserved
	   >> ii. m_orig_quantity >> ii. m_orig_price;

	ii. m_status = (BrickLink::InvItem::Status) status;
	ii. m_condition = (BrickLink::Condition) cond; 
	ii. m_retain = ( retain );
	ii. m_stockroom = ( stockroom );

	if ( ii. m_custom_picture ) {
		ii. m_custom_picture-> release ( );
		ii. m_custom_picture = 0;
	}
	   
	return ds;
}

const char *BrickLink::InvItemDrag::s_mimetype = "application/x-bricklink-invitems";

BrickLink::InvItemDrag::InvItemDrag( const QPtrList <BrickLink::InvItem> &items, QWidget *dragsource, const char *name )
    : QDragObject( dragsource, name )
{
    setItems ( items );
}

BrickLink::InvItemDrag::InvItemDrag( QWidget *dragsource, const char *name )
    : QDragObject( dragsource, name )
{
	setItems ( QPtrList <BrickLink::InvItem> ( ));
}

const char *BrickLink::InvItemDrag::format ( int n ) const
{
	switch ( n ) {
		case  0: return s_mimetype;
		case  1: return "text/plain";
		default: return 0;
	}
}

void BrickLink::InvItemDrag::setItems ( const QPtrList <BrickLink::InvItem> &items )
{
	m_text. truncate ( 0 );
	m_data. truncate ( 0 );
	QDataStream ds ( m_data, IO_WriteOnly );
	
	ds << items. count ( );
	for ( QPtrListIterator <BrickLink::InvItem> it ( items ); it. current ( ); ++it ) {
		ds << *it. current ( );
		if ( !m_text. isEmpty ( ))
			m_text. append ( "\n" );
		m_text. append ( it. current ( )-> item ( )-> id ( ));
	}
}

QByteArray BrickLink::InvItemDrag::encodedData ( const char *mime ) const
{
	if ( !qstricmp ( mime, format ( 0 )))      // normal data		
		return m_data;
	else if ( !qstricmp ( mime, format ( 1 ))) // text/plain
		return m_text;
	else
		return QByteArray ( );
}

bool BrickLink::InvItemDrag::canDecode ( QMimeSource *e )
{
    return e-> provides ( s_mimetype );
}

bool BrickLink::InvItemDrag::decode ( QMimeSource *e, QPtrList <BrickLink::InvItem> &items )
{
    QByteArray data = e-> encodedData ( s_mimetype );
    QDataStream ds ( data, IO_ReadOnly );

    if ( !data. size ( ))
    	return false;

    Q_UINT32 count = 0;
    ds >> count;

	items. clear ( );
	
	for ( ; count && !ds. atEnd ( ); count-- ) {
		BrickLink::InvItem *ii = new BrickLink::InvItem ( 0, 0 );
	    ds >> *ii;
	    items. append ( ii );
	}
	
    return true;
}
