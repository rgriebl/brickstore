/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qfile.h>
#include <qfileinfo.h>
//Added by qt3to4:
#include <Q3TextStream>

#include "cutility.h"
#include "bricklink.h"


namespace {

static char *my_strdup ( const char *str )
{
	return str ? strcpy ( new char [strlen ( str ) + 1], str ) : 0;
}

static bool my_strncmp ( const char *s1, const char *s2, int len )
{
	while ( *s1 && ( len-- > 0 )) {
		if ( *s1++ != *s2++ )
			return false;
	}
	return ( len == 0 ) && ( *s1 == 0 );
}

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



const BrickLink::Category *BrickLink::TextImport::findCategoryByName ( const char *name, int len )
{
	if ( !name || !name [0] )
		return 0;
	if ( len < 0 )
		len = strlen ( name );

    for ( QHashIterator<int, Category *> it ( m_categories ); it. hasNext(); ) {
        it.next();
        if ( my_strncmp ( it. value ( )-> name ( ), name, len ))
            return it. value ( );
	}
	return 0;
}

const BrickLink::Item *BrickLink::TextImport::findItem ( char tid, const char *id )
{
	BrickLink::Item key;
	key. m_item_type = m_item_types [tid];
	key. m_id = const_cast <char *> ( id );

	BrickLink::Item *keyp = &key;

	Item **itp = (Item **) bsearch ( &keyp, m_items. data ( ), m_items. count ( ), sizeof( Item * ), (int (*) ( const void *, const void * )) Item::compare );

	key. m_id = 0;
	key. m_item_type = 0;

	return itp ? *itp : 0;
}

void BrickLink::TextImport::appendCategoryToItemType ( const Category *cat, ItemType *itt )
{
	bool found = false;
	Q_UINT32 catcount = 0;
	for ( const BrickLink::Category **catp = itt-> m_categories; catp && *catp; catp++ ) {
		if ( *catp == cat ) {
			found = true;
			break;
		}
		catcount++;
	}

	if ( found )
		return;

	const Category **catarr = new const Category * [catcount + 2];
	memcpy ( catarr, itt-> m_categories, catcount * sizeof( const Category * ));
	catarr [catcount] = cat;
	catarr [catcount + 1] = 0;

	delete [] itt-> m_categories;
    itt-> m_categories = catarr;
}

template <> BrickLink::Category *BrickLink::TextImport::parse<BrickLink::Category> ( uint count, const char **strs, BrickLink::Category * )
{
	if ( count < 2 )
		return 0;

	Category *cat = new Category ( );
	cat-> m_id   = strtol ( strs [0], 0, 10 );
	cat-> m_name = my_strdup ( strs [1] );
	return cat;
}

template <> BrickLink::Color *BrickLink::TextImport::parse<BrickLink::Color> ( uint count, const char **strs, BrickLink::Color * )
{
	if ( count < 2 )
		return 0;

	Color *col = new Color ( );
	col-> m_id          = strtol ( strs [0], 0, 10 );
	col-> m_name        = my_strdup ( strs [1] );
	col-> m_ldraw_id    = -1;
	col-> m_is_trans    = ( strstr ( strs [1], "Trans-"    ) != 0 );
	col-> m_is_glitter  = ( strstr ( strs [1], "Glitter "  ) != 0 );
	col-> m_is_speckle  = ( strstr ( strs [1], "Speckle "  ) != 0 );
	col-> m_is_metallic = ( strstr ( strs [1], "Metallic " ) != 0 );
	col-> m_is_chrome   = ( strstr ( strs [1], "Chrome "   ) != 0 );
	return col;
}

template <> BrickLink::ItemType *BrickLink::TextImport::parse<BrickLink::ItemType> ( uint count, const char **strs, BrickLink::ItemType * )
{
	if ( count < 2 )
		return 0;

	char c = strs [0][0];
	ItemType *itt = new ItemType ( );
	itt-> m_id              = c;
	itt-> m_picture_id      = ( c == 'I' ) ? 'S' : c;
	itt-> m_name            = my_strdup ( strs [1] );
	itt-> m_has_inventories = false;
	itt-> m_has_colors      = ( c == 'P' || c == 'G' );
	itt-> m_has_weight      = ( c == 'B' || c == 'P' || c == 'G' || c == 'S' || c == 'I' );
	itt-> m_has_year        = ( c == 'B' || c == 'C' || c == 'G' || c == 'S' || c == 'I' );

	return itt;
}

template <> BrickLink::Item *BrickLink::TextImport::parse<BrickLink::Item> ( uint count, const char **strs, BrickLink::Item * )
{
	if ( count < 4 )
		return 0;

	Item *item = new Item ( );
	item-> m_id        = my_strdup ( strs [2] );
	item-> m_name      = my_strdup ( strs [3] );
	item-> m_item_type = m_current_item_type;
		
	// only allocate one block for name, id and category to speed up things

	const char *pos = strs [1];
	const Category *maincat = m_categories [strtol ( strs [0], 0, 10 )];
	
	if ( maincat && pos [0] ) {
		pos += strlen ( maincat-> name ( ));
	
		if ( *pos )
			pos += 3;
	}
	const char *auxcat = pos;

	uint catcount = 1;
	const Category *catlist [256] = { maincat };

	while (( pos = strstr ( pos, " / " ))) {
		if ( auxcat [0] ) {
			const Category *cat = findCategoryByName ( auxcat, pos - auxcat );
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
		const Category *cat = findCategoryByName ( auxcat );
		if ( cat )
			catlist [catcount++] = cat;
		else
			qWarning ( "Invalid category name: %s", auxcat );
	}

	item-> m_categories = new const BrickLink::Category * [catcount + 1];
	memcpy ( item-> m_categories, catlist, catcount * sizeof( const BrickLink::Category * ));
	item-> m_categories [catcount] = 0;

	uint parsedfields = 4;

	if (( parsedfields < count ) && ( item-> m_item_type-> hasYearReleased ( ))) {
		int y = strtol ( strs [parsedfields], 0, 10 ) - 1900;
		item-> m_year = (( y > 0 ) && ( y < 255 )) ? y : 0; // we only have 8 bits for the year
		parsedfields++;
	}
	else
		item-> m_year = 0;

	if (( parsedfields < count ) && ( item-> m_item_type-> hasWeight ( ))) {
		item-> m_weight = QLocale::c ( ). toFloat ( strs [parsedfields] );
		parsedfields++;
	}
	else
		item-> m_weight = 0;

	if ( parsedfields < count )
		item-> m_color = m_colors [strtol ( strs [parsedfields], 0, 10 )];
	else
		item-> m_color = 0;

	return item;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

BrickLink::TextImport::TextImport ( )
{
    m_colors. reserve ( 151 );
    m_categories. reserve ( 503 );
    m_item_types. reserve ( 13 );

	m_current_item_type = 0;
}

bool BrickLink::TextImport::import ( const QString &path )
{
	bool ok = true;

	ok &= readDB <> ( path + "colors.txt",     m_colors );
	ok &= readDB <> ( path + "categories.txt", m_categories );
	ok &= readDB <> ( path + "itemtypes.txt",  m_item_types );

	ok &= readColorGuide ( path + "colorguide.html" );
	ok &= readPeeronColors ( path + "peeron_colors.html" );

	// hack to speed up loading (exactly 36759 items on 22.08.2005)
	m_items. resize ( 37000 );

    for ( QHashIterator<int, ItemType *> it ( m_item_types ); it. hasNext ( ); ) {
        it. next ( );
        m_current_item_type = it. value ( );
        ok &= readDB <> ( path + "items_" + char( it. value ( )-> m_id ) + ".txt", m_items );
	}
	m_current_item_type = 0;

	qsort ( m_items. data ( ), m_items. count ( ), sizeof( Item * ), (int (*) ( const void *, const void * )) Item::compare );

	Item **itp = m_items. data ( );

	for ( uint i = 0; i < m_items. count ( ); itp++, i++ ) {
		( *itp )-> m_index = i;

		ItemType *itt = const_cast <ItemType *> (( *itp )-> m_item_type );

		if ( itt ) {
			for ( const Category **catp = ( *itp )-> m_categories; *catp; catp++ )
				appendCategoryToItemType ( *catp, itt );
		}
	}

	btinvlist_dummy btinvlist_dummy;
	ok &= readDB <> ( path + "btinvlist.txt", btinvlist_dummy );

	if ( !ok )
		qWarning ( "Error importing databases!" );

	return ok;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine ( QHash<int, T *> &d, uint tokencount, const char **tokens )
{
	T *t = 0;
	t = parse<T> ( tokencount, (const char **) tokens, t );

	if ( t ) {
		d. insert ( t-> id ( ), t );
		return true;
	}
	else
		return false;
}

template <typename T> bool BrickLink::TextImport::readDB_processLine ( Q3PtrVector<T> &v, uint tokencount, const char **tokens )
{
	T *t = 0;
	t = parse<T> ( tokencount, (const char **) tokens, t );

	if ( t ) {
		if ( v. size ( ) == v. count ( ))
            v. resize ( v. size ( ) + QMAX( (uint)40, QMIN( (uint)320, v. size ( ))));
		v. insert ( v. count ( ), t );
		return true;
	}
	else
		return false;
}


bool BrickLink::TextImport::readDB_processLine ( btinvlist_dummy & /*dummy*/, uint count, const char **strs )
{
	if ( count < 2 || !strs [0][0] || !strs [1][0] )
		return false;

	const BrickLink::Item *itm = findItem ( strs [0][0], strs [1] );

	if ( itm ) {
		time_t t = time_t ( 0 ); // 1.1.1970 00:00

		if ( strs [2][0] ) {
			char ampm;
			int d, m, y, hh, mm, ss;

			if ( sscanf ( strs [2], "%d/%d/%d %d:%d:%d %cM", &m, &d, &y, &hh, &mm, &ss, &ampm ) == 7 ) {
				QDateTime dt;
				dt. setDate ( QDate ( y, m, d ));
				if ( ampm == 'P' )
					hh += ( hh == 12 ) ? -12 : 12;
				dt. setTime ( QTime ( hh, mm, ss ));

				// !!! These dates are in EST (-0500), not UTC !!!
				t = CUtility::toUTC ( dt, "EST5EDT" );
			}
		}

		const_cast <Item *> ( itm )-> m_last_inv_update = t;
		const_cast <ItemType *> ( itm-> m_item_type )-> m_has_inventories = true;
	}
	else
		qWarning ( "WARNING: parsing btinvlist: item %s [%c] doesn't exist!", strs [1], strs [0][0] );

	return true;
}


template <typename C> bool BrickLink::TextImport::readDB ( const QString &name, C &container )
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

			if ( !readDB_processLine ( container, tokencount, (const char **) tokens )) {
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

bool BrickLink::TextImport::readColorGuide ( const QString &name )
{
	QFile f ( name );
	if ( f. open ( QIODevice::ReadOnly )) {
		Q3TextStream ts ( &f );
		QString s = ts. read ( );
		f. close ( );

		QRegExp rxp ( ">([0-9]+)&nbsp;</TD><TD[ ]+BGCOLOR=\"#?([a-fA-F0-9]{6,6})\">" );

		int pos = 0;

		while (( pos = rxp. search ( s, pos )) != -1 ) {
			int id = rxp. cap ( 1 ). toInt ( );

			Color *colp = m_colors [id];
			if ( colp )
				colp-> m_color = QColor ( "#" + rxp. cap ( 2 ));

			pos += rxp. matchedLength ( );
		}
		return true;
	}
	return false;
}


bool BrickLink::TextImport::readPeeronColors ( const QString &name )
{
	QFile f ( name );
	if ( f. open ( QIODevice::ReadOnly )) {
		Q3TextStream in ( &f );
		
		int count = 0;

		QRegExp namepattern ( "<a href=[^>]+>(.+)</a>" );

        while (!in. atEnd ( )) {
            QString line = in. readLine ( );

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
						Color *colp = m_colors [id];
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

bool BrickLink::TextImport::importInventories ( const QString &path, Q3PtrVector<Item> &invs )
{
	BrickLink::Item **itemp = invs. data ( );
	for ( uint i = 0; i < invs. count ( ); i++ ) {
		BrickLink::Item *&item = itemp [i];

		if ( !item ) // already yanked
			continue;

		if ( !item-> hasInventory ( )) {
			item = 0;  // no inv at all -> yank it
			continue;
		}

		if ( readInventory ( path, item )) {
			item = 0;
		}
	}
	return true;
}

bool BrickLink::TextImport::readInventory ( const QString &path, Item *item )
{
	QString filename = QString ( "%1/%2/%3/inventory.xml" ). arg ( path ). arg ( QChar( item-> itemType ( )-> id ( ))). arg ( item-> id ( ));

	QFileInfo fi ( filename );
	if ( fi. exists ( ) && ( fi. lastModified ( ) < item-> inventoryUpdated ( )))
		return false;

	bool ok = false;

	QFile f ( filename );
	if ( f. open ( QIODevice::ReadOnly )) {
   		uint invalid_items = 0;
		InvItemList *items = 0;

		QString emsg;
		int eline = 0, ecol = 0;
		QDomDocument doc;

		if ( doc. setContent ( &f, &emsg, &eline, &ecol )) {
			QDomElement root = doc. documentElement ( );

			items = BrickLink::inst ( )-> parseItemListXML ( doc. documentElement ( ). toElement ( ), BrickLink::XMLHint_Inventory , &invalid_items );

			if ( items ) {
				if ( !invalid_items ) {
					foreach ( const BrickLink::InvItem *ii, *items ) {
						if ( !ii-> item ( ) || !ii-> color ( ) || !ii-> quantity ( ))
							continue;

						BrickLink::Item::AppearsInMapVector &vec = m_appears_in_map [ii-> item ( )][ii-> color ( )];
						vec. append ( QPair<int, const BrickLink::Item *> ( ii-> quantity ( ), item ));
					}
					m_consists_of_map. insert ( item, *items );
					ok = true;
				}
				delete items;
			}
		}
	}
	return ok;
}

void BrickLink::TextImport::exportTo ( BrickLink *bl )
{
	bl-> setDatabase_Basics ( m_colors, m_categories, m_item_types, m_items );
}

void BrickLink::TextImport::exportInventoriesTo ( BrickLink *bl )
{
	bl-> setDatabase_ConsistsOf ( m_consists_of_map );
	bl-> setDatabase_AppearsIn ( m_appears_in_map );
}
