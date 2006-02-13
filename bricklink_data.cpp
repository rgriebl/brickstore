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

#include "cutility.h"
#include "bricklink.h"


BrickLink::Color::Color ( ) : m_name ( 0 ), m_peeron_name ( 0 ) { }
BrickLink::Color::~Color ( ) { delete [] m_name; delete [] m_peeron_name; }

QDataStream &operator << ( QDataStream &ds, const BrickLink::Color *col )
{
	Q_UINT8 flags = 0;
	flags |= ( col-> m_is_trans    ? 0x01 : 0 );
	flags |= ( col-> m_is_glitter  ? 0x02 : 0 );
	flags |= ( col-> m_is_speckle  ? 0x04 : 0 );
	flags |= ( col-> m_is_metallic ? 0x08 : 0 );
	flags |= ( col-> m_is_chrome   ? 0x10 : 0 );

	return ds << col-> m_id << col-> m_name << col-> m_peeron_name << col-> m_ldraw_id << col-> m_color << flags;
}

QDataStream &operator >> ( QDataStream &ds, BrickLink::Color *col )
{
	delete [] col-> m_name; delete [] col-> m_peeron_name;

	Q_UINT8 flags;
	ds >> col-> m_id >> col-> m_name >> col-> m_peeron_name >> col-> m_ldraw_id >> col-> m_color >> flags;

	col-> m_is_trans    = flags & 0x01;
	col-> m_is_glitter  = flags & 0x02;
	col-> m_is_speckle  = flags & 0x04;
	col-> m_is_metallic = flags & 0x08;
	col-> m_is_chrome   = flags & 0x10;

	return ds;
}


BrickLink::ItemType::ItemType ( ) : m_name ( 0 ), m_categories ( 0 ) { }
BrickLink::ItemType::~ItemType ( ) { delete [] m_name; delete [] m_categories; }

QSize BrickLink::ItemType::imageSize ( ) const
{
	QSize s ( 80, 60 );
	if ( m_id == 'M' ) 
		s. transpose ( );
	return s;
}

QDataStream &operator << ( QDataStream &ds, const BrickLink::ItemType *itt )
{
	Q_UINT8 flags = 0;
	flags |= ( itt-> m_has_inventories ? 0x01 : 0 );
	flags |= ( itt-> m_has_colors      ? 0x02 : 0 );
	flags |= ( itt-> m_has_weight      ? 0x04 : 0 );
	flags |= ( itt-> m_has_year        ? 0x08 : 0 );

	ds << Q_UINT8( itt-> m_id ) << Q_UINT8( itt-> m_picture_id ) << itt-> m_name << flags;
	
	Q_UINT32 catcount = 0;
	for ( const BrickLink::Category **catp = itt-> m_categories; *catp; catp++ )
		catcount++;

	ds << catcount;
	for ( const BrickLink::Category **catp = itt-> m_categories; *catp; catp++ )
		ds << Q_INT32(( *catp )-> id ( ));
	
	return ds;
}

QDataStream &operator >> ( QDataStream &ds, BrickLink::ItemType *itt )
{
	delete [] itt-> m_name;
	delete [] itt-> m_categories;

	Q_UINT8 flags = 0;
	Q_UINT32 catcount = 0;
	Q_UINT8 id = 0, picid = 0;
	ds >> id >> picid >> itt-> m_name >> flags >> catcount;

	itt-> m_id = id;
	itt-> m_picture_id = id;

	itt-> m_categories = new const BrickLink::Category * [catcount + 1];

	for ( Q_UINT32 i = 0; i < catcount; i++ ) {
		Q_INT32 id = 0;
		ds >> id;
		itt-> m_categories [i] = BrickLink::inst ( )-> category ( id );
	}
	itt-> m_categories [catcount] = 0;

	itt-> m_has_inventories = flags & 0x01;
	itt-> m_has_colors      = flags & 0x02;
	itt-> m_has_weight      = flags & 0x04;
	itt-> m_has_year        = flags & 0x08;
	return ds;
}


BrickLink::Category::Category ( ) : m_name ( 0 ) { }
BrickLink::Category::~Category ( ) { delete [] m_name; }

QDataStream &operator << ( QDataStream &ds, const BrickLink::Category *cat )
{
	return ds << cat-> m_id << cat-> m_name;
}

QDataStream &operator >> ( QDataStream &ds, BrickLink::Category *cat )
{
	delete [] cat-> m_name;
	return ds >> cat-> m_id >> cat-> m_name;
}


BrickLink::Item::Item ( ) : m_id ( 0 ), m_name ( 0 ), m_categories ( 0 ), m_appears_in ( 0 ), m_consists_of ( 0 ) { }
BrickLink::Item::~Item ( ) { delete [] m_id; delete [] m_name; delete [] m_categories; delete [] m_appears_in; delete [] m_consists_of; }

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

uint _dwords_for_appears  = 0;
uint _qwords_for_consists = 0;

void BrickLink::Item::setAppearsIn ( const AppearsInMap &map ) const
{
	delete [] m_appears_in;

	int s = 2;

	for ( AppearsInMap::const_iterator it = map. begin ( ); it != map. end ( ); ++it )
		s += ( 1 + it. data ( ). size ( ));

	_dwords_for_appears += s;

	Q_UINT32 *ptr = new Q_UINT32 [s];
	m_appears_in = ptr;

	*ptr++ = map. size ( ); // how many colors
	*ptr++ = s;             // only useful in save/restore -> size in DWORDs

	for ( AppearsInMap::const_iterator it = map. begin ( ); it != map. end ( ); ++it ) {
		appears_in_record *color_header = reinterpret_cast <appears_in_record *> ( ptr );

		color_header-> m12 = it. key ( )-> id ( );    // color id
		color_header-> m20 = it. data ( ). size ( );  // # of entries

		ptr++;

		for ( AppearsInMapVector::const_iterator itvec = it. data ( ). begin ( ); itvec != it. data ( ). end ( ); ++itvec ) {
			appears_in_record *color_entry = reinterpret_cast <appears_in_record *> ( ptr );

			color_entry-> m12 = itvec-> first;            // qty
			color_entry-> m20 = itvec-> second-> m_index; // item index

			ptr++;
		}
	}
}

BrickLink::Item::AppearsInMap BrickLink::Item::appearsIn ( const Color *only_color ) const
{
	AppearsInMap map;

	BrickLink::Item **items = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	if ( m_appears_in ) {
		Q_UINT32 *ptr = m_appears_in + 2;

		for ( Q_UINT32 i = 0; i < m_appears_in [0]; i++ ) {
			appears_in_record *color_header = reinterpret_cast <appears_in_record *> ( ptr );
			ptr++;

			const BrickLink::Color *color = BrickLink::inst ( )-> color ( color_header-> m12 );

			if ( color && ( !only_color || ( color == only_color ))) {
				AppearsInMapVector &vec = map [color];

				for ( Q_UINT32 j = 0; j < color_header-> m20; j++, ptr++ ) {
					appears_in_record *color_entry = reinterpret_cast <appears_in_record *> ( ptr );

					uint qty = color_entry-> m12;    // qty
					uint index = color_entry-> m20;  // item index
					
					if ( qty && ( index < count ))
						vec. append ( QPair <int, const Item *> ( qty, items [index] ));
				}
			}
			else
				ptr += color_header-> m20; // skip
		}
	}

	return map;
}

void BrickLink::Item::setConsistsOf ( const InvItemList &items ) const
{
	delete [] m_consists_of;

	_qwords_for_consists += ( items. count ( ) + 1 );

	Q_UINT64 *ptr = new Q_UINT64 [items. count ( ) + 1];
	m_consists_of = ptr;

	*ptr++ = items. count ( );   // how many entries

	foreach ( const InvItem *item, items ) {
		consists_of_record *entry = reinterpret_cast <consists_of_record *> ( ptr );

		if ( item-> item ( ) && item-> color ( ) && item-> quantity ( )) {
			entry-> m_qty      = item-> quantity ( );
			entry-> m_index    = item-> item ( )-> m_index;
			entry-> m_color    = item-> color ( )-> id ( );
			entry-> m_extra    = ( item-> status ( ) == BrickLink::InvItem::Extra ) ? 1 : 0;
			entry-> m_isalt    = 0;
			entry-> m_altid    = 0;
			entry-> m_reserved = 0;
			ptr++;
		}
		else
			m_consists_of [0]--;
	}
}

BrickLink::InvItemList BrickLink::Item::consistsOf ( ) const
{
	InvItemList list;

	BrickLink::Item **items = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	if ( m_consists_of ) {
		Q_UINT64 *ptr = m_consists_of + 1;

		for ( uint i = 0; i < uint( m_consists_of [0] ); i++ ) {
			consists_of_record *entry = reinterpret_cast <consists_of_record *> ( ptr );
			ptr++;

			const BrickLink::Color *color = BrickLink::inst ( )-> color ( entry-> m_color );
			const BrickLink::Item *item = ( entry-> m_index < count ) ? items [entry-> m_index] : 0;

			if ( color && item ) {
				InvItem *ii = new InvItem ( color, item );
				ii-> setQuantity ( entry-> m_qty );
				if ( entry-> m_extra )
					ii-> setStatus ( BrickLink::InvItem::Extra );

				list. append ( ii );
			}
		}
	}

	return list;
}

QDataStream &operator << ( QDataStream &ds, const BrickLink::Item *item )
{
	ds << item-> m_id << item-> m_name << Q_UINT8( item-> m_item_type-> id ( ));
	
	Q_UINT32 catcount = 0;
	for ( const BrickLink::Category **catp = item-> m_categories; *catp; catp++ )
		catcount++;

	ds << catcount;
	for ( const BrickLink::Category **catp = item-> m_categories; *catp; catp++ )
		ds << Q_INT32(( *catp )-> id ( ));

	Q_INT32 colorid = item-> m_color ? Q_INT32( item-> m_color-> id ( )) : -1;
	ds << colorid << item-> m_inv_updated << item-> m_weight << Q_UINT32( item-> m_index ) << Q_UINT32( item-> m_year );

	if ( item-> m_appears_in && item-> m_appears_in [0] && item-> m_appears_in [1] ) {
		Q_UINT32 *ptr = item-> m_appears_in;

		for ( Q_UINT32 i = 0; i < item-> m_appears_in [1]; i++ )
			ds << *ptr++;
	}
	else
		ds << Q_UINT32( 0 );

	if ( item-> m_consists_of && item-> m_consists_of [0] ) {
		ds << Q_UINT32( item-> m_consists_of [0] );

		Q_UINT64 *ptr = item-> m_consists_of + 1;

		for ( Q_UINT32 i = 0; i < Q_UINT32( item-> m_consists_of [0] ); i++ )
			ds << *ptr++;
	}
	else
		ds << Q_UINT32( 0 );

	return ds;
}

QDataStream &operator >> ( QDataStream &ds, BrickLink::Item *item )
{
	delete [] item-> m_id;
	delete [] item-> m_name;
	delete [] item-> m_categories;
	delete item-> m_appears_in;

	Q_UINT8 ittid = 0;
	Q_UINT32 catcount = 0;

	ds >> item-> m_id >> item-> m_name >> ittid >> catcount;
	item-> m_item_type = BrickLink::inst ( )-> itemType ( ittid ); 

	item-> m_categories = new const BrickLink::Category * [catcount + 1];

	for ( Q_UINT32 i = 0; i < catcount; i++ ) {
		Q_INT32 id = 0;
		ds >> id;
		item-> m_categories [i] = BrickLink::inst ( )-> category ( id );
	}
	item-> m_categories [catcount] = 0;

	Q_INT32 colorid = 0;
	Q_UINT32 index = 0, year = 0;
	ds >> colorid >> item-> m_inv_updated >> item-> m_weight >> index >> year;
	item-> m_color = colorid == -1 ? 0 : BrickLink::inst ( )-> color ( colorid );
	item-> m_index = index;
	item-> m_year = year;
	
	Q_UINT32 appears = 0, appears_size = 0;
	ds >> appears;
	if ( appears )
		ds >> appears_size;

	if ( appears && appears_size ) {
		Q_UINT32 *ptr = new Q_UINT32 [appears_size];
		item-> m_appears_in = ptr;

		*ptr++ = appears;
		*ptr++ = appears_size;

		for ( Q_UINT32 i = 2; i < appears_size; i++ )
			ds >> *ptr++;
	}
	else
		item-> m_appears_in = 0;

	Q_UINT32 consists = 0;
	ds >> consists;

	if ( consists ) {
		Q_UINT64 *ptr = new Q_UINT64 [consists + 1];
		item-> m_consists_of = ptr;

		*ptr++ = consists;

		for ( Q_UINT32 i = 0; i < consists; i++ )
			ds >> *ptr++;
	}
	else
		item-> m_consists_of = 0;

	return ds;
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

BrickLink::InvItem::InvItem ( const BrickLink::InvItem &copy )
{
	m_incomplete = 0;
	m_custom_picture = 0;

	*this = copy;
}

BrickLink::InvItem &BrickLink::InvItem::operator = ( const InvItem &copy )
{
	delete m_incomplete;
	m_incomplete = 0;

	if ( m_custom_picture ) {
		m_custom_picture-> release ( );
		m_custom_picture = 0;
	}

	m_item           = copy. m_item;
	m_color          = copy. m_color;
	m_status         = copy. m_status;
	m_condition      = copy. m_condition;
	m_retain         = copy. m_retain;
	m_stockroom      = copy. m_stockroom;
	m_comments       = copy. m_comments;
	m_remarks        = copy. m_remarks;
	m_reserved       = copy. m_reserved;
	m_custom_picture_url = copy. m_custom_picture_url;
	m_quantity       = copy. m_quantity;
	m_bulk_quantity  = copy. m_bulk_quantity;
	m_tier_quantity [0] = copy. m_tier_quantity [0];
	m_tier_quantity [1] = copy. m_tier_quantity [1];
	m_tier_quantity [2] = copy. m_tier_quantity [2];
	m_sale           = copy. m_sale;
	m_price          = copy. m_price;
	m_tier_price [0] = copy. m_tier_price [0];
	m_tier_price [1] = copy. m_tier_price [1];
	m_tier_price [2] = copy. m_tier_price [2];
	m_weight         = copy. m_weight;
	m_lot_id         = copy. m_lot_id;
	m_orig_price     = copy. m_orig_price;
	m_orig_quantity  = copy. m_orig_quantity;

	if ( copy. m_incomplete ) {
		m_incomplete = new Incomplete;

		m_incomplete-> m_item_id       = copy. m_incomplete-> m_item_id;
		m_incomplete-> m_item_name     = copy. m_incomplete-> m_item_name;
		m_incomplete-> m_itemtype_id   = copy. m_incomplete-> m_itemtype_id;
		m_incomplete-> m_itemtype_name = copy. m_incomplete-> m_itemtype_name;
		m_incomplete-> m_category_id   = copy. m_incomplete-> m_category_id;
		m_incomplete-> m_category_name = copy. m_incomplete-> m_category_name;
		m_incomplete-> m_color_id      = copy. m_incomplete-> m_color_id;
		m_incomplete-> m_color_name    = copy. m_incomplete-> m_color_name;
	}

	if ( m_custom_picture )
		m_custom_picture-> addRef ( );

	return *this;
}

bool BrickLink::InvItem::operator == ( const InvItem &cmp ) const
{
	bool same = true;

	same &= ( m_incomplete         == cmp. m_incomplete );
	same &= ( m_custom_picture     == cmp. m_custom_picture );
	same &= ( m_item               == cmp. m_item );
	same &= ( m_color              == cmp. m_color );
	same &= ( m_status             == cmp. m_status );
	same &= ( m_condition          == cmp. m_condition );
	same &= ( m_retain             == cmp. m_retain );
	same &= ( m_stockroom          == cmp. m_stockroom );
	same &= ( m_comments           == cmp. m_comments );
	same &= ( m_remarks            == cmp. m_remarks );
	same &= ( m_reserved           == cmp. m_reserved );
	same &= ( m_custom_picture_url == cmp. m_custom_picture_url );
	same &= ( m_quantity           == cmp. m_quantity );
	same &= ( m_bulk_quantity      == cmp. m_bulk_quantity );
	same &= ( m_tier_quantity [0]  == cmp. m_tier_quantity [0] );
	same &= ( m_tier_quantity [1]  == cmp. m_tier_quantity [1] );
	same &= ( m_tier_quantity [2]  == cmp. m_tier_quantity [2] );
	same &= ( m_sale               == cmp. m_sale );
	same &= ( m_price              == cmp. m_price );
	same &= ( m_tier_price [0]     == cmp. m_tier_price [0] );
	same &= ( m_tier_price [1]     == cmp. m_tier_price [1] );
	same &= ( m_tier_price [2]     == cmp. m_tier_price [2] );
	same &= ( m_weight             == cmp. m_weight );
	same &= ( m_lot_id             == cmp. m_lot_id );
	same &= ( m_orig_price         == cmp. m_orig_price );
	same &= ( m_orig_quantity      == cmp. m_orig_quantity );

	return same;
}

BrickLink::InvItem::~InvItem ( )
{
	delete m_incomplete;

	if ( m_custom_picture )
		m_custom_picture-> release ( );
}

bool BrickLink::InvItem::mergeFrom ( const InvItem &from, bool prefer_from )
{
	if (( &from == this ) ||
	    ( from. item ( ) != item ( )) ||
	    ( from. color ( ) != color ( )) ||
	    ( from. condition ( ) != condition ( )))
		return false;

	if (( from. price ( ) != 0 ) && (( price ( ) == 0 ) || prefer_from ))
		setPrice ( from. price ( ));
	if (( from. bulkQuantity ( ) != 1 ) && (( bulkQuantity ( ) == 1 ) || prefer_from ))
		setBulkQuantity ( from. bulkQuantity ( ));
	if (( from. sale ( )) && ( !( sale ( )) || prefer_from ))
		setSale ( from. sale ( ));

	for ( int i = 0; i < 3; i++ ) {
		if (( from. tierPrice ( i ) != 0 ) && (( tierPrice ( i ) == 0 ) || prefer_from ))
			setTierPrice ( i, from. tierPrice ( i ));
		if (( from. tierQuantity ( i )) && ( !( tierQuantity ( i )) || prefer_from ))
			setTierQuantity ( i, from. tierQuantity ( i ));
	}
	
	if ( !from. remarks ( ). isEmpty ( ) && ( remarks ( ). isEmpty ( ) || prefer_from ))
		setRemarks ( from. remarks ( ));
	if ( !from. comments ( ). isEmpty ( ) && ( comments ( ). isEmpty ( ) || prefer_from ))
		setComments ( from. comments ( ));
	if ( !from. reserved ( ). isEmpty ( ) && ( reserved ( ). isEmpty ( ) || prefer_from ))
		setReserved ( from. reserved ( ));	
	if ( !from. customPictureUrl ( ). isEmpty ( ) && ( customPictureUrl ( ). isEmpty ( ) || prefer_from ))
		setCustomPictureUrl ( from. customPictureUrl ( ));
	
	if ( prefer_from ) {
		setStatus ( from. status ( ));
		setRetain ( from. retain ( ));
		setStockroom ( from. stockroom ( ));
	}

	setQuantity ( quantity ( ) + from. quantity ( ));
	setOrigQuantity ( origQuantity ( ) + from. origQuantity ( ));

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

BrickLink::InvItemDrag::InvItemDrag( const InvItemList &items, QWidget *dragsource, const char *name )
    : QDragObject( dragsource, name )
{
    setItems ( items );
}

BrickLink::InvItemDrag::InvItemDrag( QWidget *dragsource, const char *name )
    : QDragObject( dragsource, name )
{
	setItems ( InvItemList ( ));
}

const char *BrickLink::InvItemDrag::format ( int n ) const
{
	switch ( n ) {
		case  0: return s_mimetype;
		case  1: return "text/plain";
		default: return 0;
	}
}

void BrickLink::InvItemDrag::setItems ( const InvItemList &items )
{
	m_text. truncate ( 0 );
	m_data. truncate ( 0 );
	QDataStream ds ( m_data, IO_WriteOnly );
	
	ds << items. count ( );
	foreach ( const InvItem *ii, items ) {
		ds << *ii;
		if ( !m_text. isEmpty ( ))
			m_text. append ( "\n" );
		m_text. append ( ii-> item ( )-> id ( ));
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

bool BrickLink::InvItemDrag::decode ( QMimeSource *e, InvItemList &items )
{
    QByteArray data = e-> encodedData ( s_mimetype );
    QDataStream ds ( data, IO_ReadOnly );

    if ( !data. size ( ))
    	return false;

    Q_UINT32 count = 0;
    ds >> count;

	items. clear ( );
	
	for ( ; count && !ds. atEnd ( ); count-- ) {
		InvItem *ii = new InvItem ( );
	    ds >> *ii;
	    items. append ( ii );
	}
	
    return true;
}

BrickLink::Order::Order ( const QString &id, Type type )
	: m_id ( id ), m_type ( type )
{ }
