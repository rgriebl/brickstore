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
#include <string.h>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QLocale>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "bricklink.h"


namespace {

struct keyword {
	const char *m_match;
	int         m_id;
	int         m_start;
	int         m_stop;
};

static int find_keywords ( const char *str, int start, int stop, struct keyword *kw )
{
	if (( start >= stop ) || !kw || !kw-> m_match || !str )
		return 0;

	int matches = 0;
	const char *pos = str + start;
	const char *end = str + stop;
	struct keyword *lastkw = 0;

	for ( ; kw-> m_match; kw++ ) {
		kw-> m_start = kw-> m_stop = -1;

		int mlen = strlen ( kw-> m_match );

		if ( !mlen )
			continue;

		const char *testpos = pos;

		while (( testpos <= ( end - ( mlen - 1 ))) && ( testpos = (char *) memchr ( testpos, kw-> m_match [0], ( end - testpos + 1 ) - ( mlen - 1 )))) {
			if ( qstrncmp ( testpos, kw-> m_match, mlen ) == 0 )
				break;
			else
				testpos++;
		}

		if ( !testpos )
			continue;

		kw-> m_start = testpos - str;
		kw-> m_stop = stop;

		matches++;

		if ( lastkw )
			lastkw-> m_stop = testpos - str - 1;

		lastkw = kw;
		pos = testpos + mlen;
	}
	return matches;
}

} // namespace

// ---------------------------------------------------------------------------

BrickLink::PriceGuide *BrickLink::Core::priceGuide ( const BrickLink::Item *item, const BrickLink::Color *color, bool high_priority )
{
	if ( !item || !color )
		return 0;

	QString key;
	key. sprintf ( "%c@%d@%d", item-> itemType ( )-> id ( ), item-> index ( ), color-> id ( ));

	//qDebug ( "PG requested for %s", key. data ( ));
	PriceGuide *pg = m_price_guides. cache [key];

	if ( !pg ) {
		//qDebug ( "  ... not in cache" );
		pg = new PriceGuide ( item, color );
		m_price_guides. cache. insert ( key, pg );
	}

	if ( pg && ( !pg-> valid ( ) || updateNeeded ( pg-> lastUpdate ( ), m_price_guides. update_iv )))
		updatePriceGuide ( pg, high_priority );

	return pg;
}


BrickLink::PriceGuide::PriceGuide ( const BrickLink::Item *item, const BrickLink::Color *color )
{
	m_item = item;
	m_color = color;

	m_valid = false;
	m_update_status = Ok;

	memset ( m_quantities, 0, sizeof( m_quantities ));
	memset ( m_lots, 0, sizeof( m_lots ));
	memset ( m_prices, 0, sizeof( m_prices ));

	load_from_disk ( );
}


BrickLink::PriceGuide::~PriceGuide ( )
{
	//qDebug ( "Deleting PG %p for %c_%s_%d", this, m_item-> itemType ( )-> id ( ), m_item-> id ( ), m_color-> id ( ));
}

namespace {

static int safe_strtol ( const char *data, uint start, uint stop )
{
	uint i = start;
	
	while (( i <= stop ) && ( data [i] >= '0' && data [i] <= '9' ))
		i++;

	if ( i == start )
		return 0;
	
	// data [i] may NOT be writable
	return QString::fromAscii ( data + start, i - start ). toInt ( );
}

static money_t safe_strtomoney ( const char *data, uint start, uint stop )
{
	uint i = start;

	while (( i <= stop ) && ( data [i] == '.' || data [i] == ',' || ( data [i] >= '0' && data [i] <= '9' )))
		i++;

	if ( i == start )
		return .0;

	// data [i] may NOT be writable
	return money_t::fromCString ( QString::fromAscii ( data + start, i - start ));
}

} // namespace

bool BrickLink::PriceGuide::parse ( const char *data, uint size )
{
	if ( !data || !size )
		return false;

	memset ( m_quantities, 0, sizeof( m_quantities ));
	memset ( m_lots, 0, sizeof( m_lots ));
	memset ( m_prices, 0, sizeof( m_prices ));

	// parse data -- evil (but fast) hack :)

	struct keyword kw_times [] = {
		{ "All Time Sales",         AllTime, -1, -1 },
		{ "Past 6 Months Sales",    PastSix, -1, -1 },
		{ "Current Items for Sale", Current, -1, -1 },
		{ 0, 0, 0, 0 }
	};

	struct keyword kw_condition [] = {
		{ "New",  New,  -1, -1 },
		{ "Used", Used, -1, -1 },
		{ 0, 0, 0, 0 }
	};

	struct keyword kw_dummy [] = {
		{ "SIZE=\"2\">&nbsp;",  0,  -1, -1 },
		{ "SIZE=\"2\">&nbsp;",  1,  -1, -1 },
		{ "SIZE=\"2\">&nbsp;",  2,  -1, -1 },
		{ "SIZE=\"2\">&nbsp;",  3,  -1, -1 },
		{ "SIZE=\"2\">&nbsp;",  4,  -1, -1 },
		{ "SIZE=\"2\">&nbsp;",  5,  -1, -1 },
		{ 0, 0, 0, 0 }
	};

	find_keywords ( data, 0, size - 1, kw_times );

	for ( keyword *tkw = kw_times; tkw-> m_match; tkw++ ) {
		if ( tkw-> m_start < 0 )
			continue;

		find_keywords ( data, tkw-> m_start, tkw-> m_stop, kw_condition );

		for ( keyword *ckw = kw_condition; ckw-> m_match; ckw++ ) {
			if ( ckw-> m_start < 0 )
				continue;

			if ( find_keywords ( data, ckw-> m_start, ckw-> m_stop, kw_dummy ) != 6 )
				continue;

			for ( int i = 0; i < 6; i++ ) {
				uint offset = kw_dummy [i]. m_start + strlen ( kw_dummy [i]. m_match );

				switch ( i ) {
					case 0:
						m_lots [tkw-> m_id][ckw-> m_id]  = safe_strtol ( data, offset, size - 1 );
						break;
					case 1:
						m_quantities [tkw-> m_id][ckw-> m_id]  = safe_strtol ( data, offset, size - 1 );
						break;
					case 2:
					case 3:
					case 4:
					case 5: {
						offset++; // $
				        m_prices [tkw-> m_id][ckw-> m_id][i-2] = safe_strtomoney ( data, offset, size - 1 );
				        break;
					}
				}
			}
		}
	}


	m_valid = true;

	m_fetched = QDateTime::currentDateTime ( );
	save_to_disk ( );

	return m_valid;
}


void BrickLink::PriceGuide::save_to_disk ( )
{
	QString path = BrickLink::inst ( )-> dataPath ( m_item, m_color );

	if ( path. isEmpty ( ))
		return;
	path += "priceguide.txt";

	//qDebug ( "PG saving data to \"%s\"",  path. latin1 ( ));

	QFile f ( path );
	if ( f. open ( QIODevice::WriteOnly )) {
		QTextStream ts ( &f );

		ts << "# Price Guide for part #" << m_item-> id ( ) << " (" << m_item-> name ( ) << "), color #" << m_color-> id ( ) << " (" << m_color-> name ( ) << ")\n";
		ts << "# last update: " << m_fetched. toString ( ) << "\n#\n";

		for ( int t = 0; t < TimeCount; t++ ) {
			for ( int c = 0; c < ConditionCount; c++ ) {
				ts << ( t == AllTime ? 'A' : ( t == PastSix ? 'P' : 'C' )) << '\t' << ( c == New ? 'N' : 'U' ) << '\t';
				ts << m_quantities [t][c] << '\t'
				   << m_lots [t][c] << '\t'
				   << m_prices [t][c][Lowest]. toCString ( ) << '\t'
				   << m_prices [t][c][Average]. toCString ( ) << '\t'
				   << m_prices [t][c][WAverage]. toCString ( ) << '\t'
				   << m_prices [t][c][Highest]. toCString ( ) << '\n';
			}
		}
		//qDebug ( "done." );
	}
	else {
		//qDebug ( "failed." );
	}
}

void BrickLink::PriceGuide::load_from_disk ( )
{
	QString path = BrickLink::inst ( )-> dataPath ( m_item, m_color );

	if ( path. isEmpty ( ))
		return;
	path += "priceguide.txt";

	bool loaded [TimeCount][ConditionCount];
	::memset ( loaded, 0, sizeof( loaded ));

	//qDebug ( "PG loading data from \"%s\"", path. latin1 ( ));

	QFile f ( path );
	if ( f. open ( QIODevice::ReadOnly )) {
		QTextStream ts ( &f );
		QString line;

		while ( !( line = ts. readLine ( )). isNull ( )) {
			if ( !line. length ( ) || ( line [0] == '#' ) || ( line [0] == '\r' ))  // skip comments fast
				continue;

			QStringList sl = line. split ( '\t', QString::KeepEmptyParts );

			if (( sl. count ( ) != 8 ) || ( sl [0]. length ( ) != 1 ) || ( sl [1]. length ( ) != 1 )) { // sanity check
				continue;
			}

			int t = -1;
			int c = -1;

			switch ( sl [0][0]. toLatin1 ( )) {
				case 'A': t = AllTime; break;
				case 'P': t = PastSix; break;
				case 'C': t = Current; break;
			}

			switch ( sl [1][0]. toLatin1 ( )) {
				case 'N': c = New;  break;
				case 'U': c = Used; break;
			}

			if (( t != -1 ) && ( c != -1 )) {
				m_quantities [t][c]       = sl [2]. toInt ( );
				m_lots [t][c]             = sl [3]. toInt ( );
				m_prices [t][c][Lowest]   = money_t::fromCString ( sl [4] );
				m_prices [t][c][Average]  = money_t::fromCString ( sl [5] );
				m_prices [t][c][WAverage] = money_t::fromCString ( sl [6] );
				m_prices [t][c][Highest]  = money_t::fromCString ( sl [7] );

				loaded [t][c] = true;
			}
		}
		f. close ( );
	}

	m_valid = true;

	for ( int t = 0; t < TimeCount; t++ ) {
		for ( int c = 0; c < ConditionCount; c++ )
			m_valid &= loaded [t][c];
	}

	if ( m_valid ) {
		QFileInfo fi ( path );
		m_fetched = fi. lastModified ( );

		//qDebug ( "done." );
	}
	else {
		//qDebug ( "failed." );
	}
}


void BrickLink::PriceGuide::update ( bool high_priority )
{
	BrickLink::inst ( )-> updatePriceGuide ( this, high_priority );
}


void BrickLink::Core::updatePriceGuide ( BrickLink::PriceGuide *pg, bool high_priority )
{
	if ( !pg || ( pg-> m_update_status == Updating ))
		return;

	if ( !m_online ) {
		pg-> m_update_status = UpdateFailed;
		emit priceGuideUpdated ( pg );
		return;
	}

	pg-> m_update_status = Updating;
	pg-> addRef ( );

	QUrl url = "http://www.bricklink.com/priceGuide.asp"; // ?a=%c&viewType=N&colorID=%d&itemID=%s", tolower ( pg-> item ( )-> itemType ( )-> id ( )), pg-> color ( )-> id ( ), pg-> item ( )-> id ( ));

	url.addQueryItem("a",        QChar(pg->item()->itemType()->id()).toLower());
	url.addQueryItem("viewType", "N");
	url.addQueryItem("colorID",  QString::number(pg->color()->id()));
	url.addQueryItem("itemID",   pg->item()->id());
	url.addQueryItem("viewDec",  "3");

	//qDebug ( "PG request started for %s", (const char *) url );
    CTransferJob *job = CTransferJob::get(url);
    job->setUserData(pg);
	m_price_guides.transfer->retrieve(job, high_priority);
}


void BrickLink::Core::priceGuideJobFinished ( CTransferJob *j )
{
    if ( !j || !j-> data ( ) || !j-> userData<PriceGuide> ( ))
		return;

	PriceGuide *pg = j->userData<PriceGuide>();

	pg-> m_update_status = UpdateFailed;

	if (j->isCompleted()) {
		if ( pg-> parse ( j-> data ( )-> data ( ), j-> data ( )-> size ( )))
			pg-> m_update_status = Ok;
	}

	emit priceGuideUpdated ( pg );
	pg-> release ( );
}

