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
#include <qfile.h>
#include <qapplication.h>

#include "cconfig.h"
#include "cutility.h"

#include "crebuilddatabase.h"

#if defined( Q_OS_WIN32 )
#define printf   qWarning
#else
#include <stdio.h>
#endif

CRebuildDatabase::CRebuildDatabase ( const QString &output )
	: QObject ( 0, "rebuild_database" )
{
	m_output = output;
}


void CRebuildDatabase::error ( )
{
	if ( m_error. isEmpty ( ))
		printf ( " FAILED.\n" );
	else
		printf ( " FAILED: %s\n", m_error. ascii ( ));

	emit finished ( 2 );

	// wait for our death...
	while ( true )
		qApp-> processEvents ( );
}

void CRebuildDatabase::exec ( )
{
	BrickLink *bl = BrickLink::inst ( );

	bl-> setOnlineStatus ( CConfig::inst ( )-> onlineStatus ( ));
	bl-> setHttpProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
	bl-> setUpdateIntervals ( 0, 0 );
	
	printf ( "\n Rebuilding database " );
	printf ( "\n=====================\n" );

	printf ( "\nSTEP 1: Downloading (text) database files...\n" );
	if ( !download ( ))
		error ( );
	
	printf ( "\nSTEP 2: Parsing downloaded files...\n" );
	if ( !parse ( ))
		error ( );

	printf ( "\nSTEP 3: Retrieving missing/updated inventories...\n" );
	if ( !downloadInv ( ))
		error ( );

	printf ( "\nSTEP 4: Parsing inventories...\n" );
	if ( !parseInv ( ))
		error ( );

	printf ( "\nSTEP 5: Writing the new database to disk...\n" );
	if ( !writeDB ( ))
		error ( );

	printf ( "\nFINISHED.\n\n" );
	emit finished ( 0 );
}

bool CRebuildDatabase::parse ( )
{
	BrickLink::TextImport blti;

	if ( !blti. import ( BrickLink::inst ( )-> dataPath ( ))) {
		m_error = "failed to parse database files.";
		return false;
	}

	BrickLink::inst ( )-> setDatabase_Basics ( blti. colors ( ), blti. categories ( ), blti. itemTypes ( ), blti. items ( ));
	return true;
}

bool CRebuildDatabase::parseInv ( )
{
	BrickLink::inst ( )-> setDatabase_AppearsIn ( m_map );
	m_map. clear ( );

	return true;
}

bool CRebuildDatabase::writeDB ( )
{
	if ( !BrickLink::inst ( )-> writeDatabase ( m_output )) {
		m_error = "failed to write the database file.";
		return false;
	}
	return true;
}


namespace {

CKeyValueList itemQuery ( char item_type )
{
	CKeyValueList query;  //?a=a&viewType=0&itemType=X
	query << CKeyValue ( "a", "a" )
	      << CKeyValue ( "viewType", "0" )
	      << CKeyValue ( "itemType", QChar ( item_type ))
	      << CKeyValue ( "selItemColor", "Y" )  // special BrickStore flag to get default color - thanks Dan
	      << CKeyValue ( "selWeight", "Y" )
	      << CKeyValue ( "selYear", "Y" );

	return query;
}	

CKeyValueList dbQuery ( int which )
{
	CKeyValueList query; //?a=a&viewType=X
	query << CKeyValue ( "a", "a" )
	      << CKeyValue ( "viewType", QString::number ( which ));

	return query;
}

} // namespace

bool CRebuildDatabase::download ( )
{
	CTransfer *trans = new CTransfer ( );
	if ( !trans-> init ( )) {
		delete trans;
		m_error = "failed to init HTTP transfer.";
		return false;
	}
	trans-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
	connect ( trans, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( downloadJobFinished ( CTransfer::Job * )));

	QString path = BrickLink::inst ( )-> dataPath ( );

	struct {
		const char *m_url;
		const CKeyValueList m_query;
		const char *m_file;
	} * tptr, table [] = {
		{ "http://www.bricklink.com/catalogDownload.asp", dbQuery ( 1 ),     "itemtypes.txt"   },
		{ "http://www.bricklink.com/catalogDownload.asp", dbQuery ( 2 ),     "categories.txt"  },
		{ "http://www.bricklink.com/catalogDownload.asp", dbQuery ( 3 ),     "colors.txt"      },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'S' ), "items_S.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'P' ), "items_P.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'M' ), "items_M.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'B' ), "items_B.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'G' ), "items_G.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'C' ), "items_C.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'I' ), "items_I.txt"     },
		{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'O' ), "items_O.txt"     },
	//	{ "http://www.bricklink.com/catalogDownload.asp", itemQuery ( 'U' ), "items_U.txt"     }, // generates a 500 server error
		{ "http://www.bricklink.com/btinvlist.asp",       CKeyValueList ( ), "btinvlist.txt"   },
		{ "http://www.bricklink.com/catalogColors.asp",   CKeyValueList ( ), "colorguide.html" },

		{ "http://www.peeron.com/inv/colors",             CKeyValueList ( ), "peeron_colors.html" },

		{ 0, CKeyValueList ( ), 0 }
	};

	bool failed = false;
	m_downloads_in_progress = 0;
	m_downloads_failed = 0;

	{ // workaround for U type
		QFile uf ( path + "items_U.txt" );
		uf. open ( IO_WriteOnly );
	}

	for ( tptr = table; tptr-> m_url; tptr++ ) {
		QFile *f = new QFile ( path + tptr-> m_file + ".new" );

		if ( !f-> open ( IO_WriteOnly )) {
			m_error = QString ( "failed to write %1: %2" ). arg( tptr-> m_file ). arg( f-> errorString ( ));
			delete f;
			failed = true;
			break;
		}

		trans-> get ( tptr-> m_url, tptr-> m_query, f );
		m_downloads_in_progress++;
	}

	if ( failed ) {
		trans-> cancelAllJobs ( );
		return false;
	}

	while ( m_downloads_in_progress )
		qApp-> processEvents ( );

	if ( m_downloads_failed )
		return false;

	delete trans;
	return true;
}

void CRebuildDatabase::downloadJobFinished ( CTransfer::Job *job )
{
	if ( job ) {
		QFile *f = job-> file ( );

		m_downloads_in_progress--;

		if ( !job-> failed ( ) && f ) {
			f-> close ( );

			QString basepath = f-> name ( );
			basepath. truncate ( basepath. length ( ) - 4 );

			QString err = CUtility::safeRename ( basepath );

			if ( err. isNull ( )) {
				printf ( "  > %s", basepath. ascii ( ));
			}
			else {
				m_error = err;
				m_downloads_failed++;
			}
		}
		else {
			m_error = QString( "failed to download file: %s" ). arg( job-> url ( ));
			m_downloads_failed++;
		}
	}
}

bool CRebuildDatabase::downloadInv ( )
{
	const BrickLink::Item * const *itemp = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	connect ( BrickLink::inst ( ), SIGNAL( inventoryUpdated ( BrickLink::Inventory * )), this, SLOT( inventoryUpdated ( BrickLink::Inventory * )));

	m_processed = 0;
	m_ptotal = 2 * count;
	m_downloads_failed = 0;

	uint next_event_loop = count / 500;

	for ( uint i = 0; i < count; i++, itemp++ ) {
		const BrickLink::Item *item = *itemp;

		if (( i % next_event_loop ) == 0 )
			qApp-> processEvents ( );

		if ( item-> inventoryUpdated ( ). isValid ( )) {
			BrickLink::Inventory *inv = BrickLink::inst ( )-> inventory ( item );

			if ( inv ) {
				inv-> addRef ( );

				m_processed++;

				if ( inv-> updateStatus ( ) != BrickLink::Updating )
					inventoryUpdated ( inv );
			}
			else
				m_ptotal -= 2;
		}
		else
			m_ptotal -= 2;
	}

	while ( m_processed < m_ptotal )
		qApp-> processEvents ( );

	if ( m_downloads_failed > ( m_ptotal / 2 ) / 20 ) { // more than 5% errors
		m_error = "too many errors while download inventories";
		return false;
	}

	return true;
}

void CRebuildDatabase::inventoryUpdated ( BrickLink::Inventory *inv )
{
	if ( !inv )
		return;

	if ( inv-> updateStatus ( ) == BrickLink::UpdateFailed ) {
		m_downloads_failed++;

		printf ( "  > inventory failed: %s", inv-> item ( )-> id ( ));
	}
	else {
		if ( inv-> item ( )) {
			foreach ( const BrickLink::InvItem *ii, inv-> inventory ( )) {
				if ( !ii-> item ( ) || !ii-> color ( ) || !ii-> quantity ( ))
					continue;

				BrickLink::Item::AppearsInMapVector &vec = m_map [ii-> item ( )][ii-> color ( )];
				vec. append ( QPair<int, const BrickLink::Item *> ( ii-> quantity ( ), inv-> item ( )));
			}
		}
	}

	inv-> release ( );
	m_processed++;
}

