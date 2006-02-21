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


CRebuildDatabase::CRebuildDatabase ( const QString &output )
	: QObject ( 0, "rebuild_database" )
{
	m_output = output;
	m_trans = 0;
	
#if defined( Q_OS_WIN32 )
	AllocConsole ( );
	const char *title = "BrickStore - Rebuilding Database";
	QT_WA({ SetConsoleTitleW ( QString( title ). ucs2 ( )); }, 
	      { SetConsoleTitleA ( title ); })
	freopen ( "CONIN$", "r", stdin );
	freopen ( "CONOUT$", "w", stdout );
#endif
}

CRebuildDatabase::~CRebuildDatabase ( )
{
#if defined( Q_OS_WIN32 )
	printf ( "\n\nPress RETURN to quit...\n\n" );
	getchar ( );
#endif
	delete m_trans;
}

int CRebuildDatabase::error ( const QString &error )
{
	if ( error. isEmpty ( ))
		printf ( " FAILED.\n" );
	else
		printf ( " FAILED: %s\n", error. ascii ( ));

	return 2;
}

int CRebuildDatabase::exec ( )
{
	m_trans = new CTransfer ( );
	if ( !m_trans-> init ( )) {
		delete m_trans;
		return error ( "failed to init HTTP transfer." );
	}
	m_trans-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
	connect ( m_trans, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( downloadJobFinished ( CTransfer::Job * )));

	BrickLink *bl = BrickLink::inst ( );
	bl-> setOnlineStatus ( false );

	BrickLink::TextImport blti;



	printf ( "\n Rebuilding database " );
	printf ( "\n=====================\n" );

	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 1: Downloading (text) database files...\n" );

	if ( !download ( ))
		return error ( m_error );
	
	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 2: Parsing downloaded files...\n" );

	if ( !blti. import ( bl-> dataPath ( )))
		return error ( "failed to parse database files." );

	blti. exportTo ( bl );

	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 3: Parsing inventories (part I)...\n" );

	QPtrVector<BrickLink::Item> invs = blti. items ( );
	blti. importInventories ( bl-> dataPath ( ), invs );

	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 4: Downloading missing/updated inventories...\n" );
	
	downloadInventories ( invs );
	
	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 5: Parsing inventories (part II)...\n" );

	blti. importInventories ( bl-> dataPath ( ), invs );
	
	if (( invs. size ( ) - invs. containsRef ( 0 )) > ( blti. items ( ). count ( ) / 50 )) // more than 2% have failed
		return error ( "more than 2% of all inventories had errors." );

	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 6: Computing the database...\n" );
	
	blti. exportInventoriesTo ( bl );

	extern uint _dwords_for_appears, _qwords_for_consists;
                        
	printf ( "  > appears-in : %11u bytes\n", _dwords_for_appears * 4 );
	printf ( "  > consists-of: %11u bytes\n", _qwords_for_consists * 8 );
                                                                        

	/////////////////////////////////////////////////////////////////////////////////
	printf ( "\nSTEP 7: Writing the new database to disk...\n" );
	if ( !bl-> writeDatabase ( m_output ))
		return error ( "failed to write the database file." );

	printf ( "\nFINISHED.\n\n" );
	return 0;
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

		m_trans-> get ( tptr-> m_url, tptr-> m_query, f );
		m_downloads_in_progress++;
	}

	if ( failed ) {
		m_trans-> cancelAllJobs ( );
		return false;
	}

	while ( m_downloads_in_progress )
		qApp-> processEvents ( );

	if ( m_downloads_failed )
		return false;

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
				printf ( "  > %s\n", basepath. ascii ( ));
			}
			else {
				m_error = err;
				m_downloads_failed++;
			}
		}
		else {
			m_error = QString( "failed to download file: %1" ). arg( job-> url ( ));
			m_downloads_failed++;
		}
	}
}


bool CRebuildDatabase::downloadInventories ( QPtrVector<BrickLink::Item> &invs )
{
	QString path = BrickLink::inst ( )-> dataPath ( );

	bool failed = false;
	m_downloads_in_progress = 0;
	m_downloads_failed = 0;

	QCString url = "http://www.bricklink.com/catalogDownload.asp";

	BrickLink::Item **itemp = invs. data ( );
	for ( uint i = 0; i < invs. count ( ); i++ ) {
		BrickLink::Item *&item = itemp [i];

		if ( item ) {
			QFile *f = new QFile ( BrickLink::inst ( )-> dataPath ( item ) + "inventory.xml.new" );

			if ( !f-> open ( IO_WriteOnly )) {
				m_error = QString ( "failed to write %1: %2" ). arg( f-> name ( )). arg( f-> errorString ( ));
				delete f;
				failed = true;
				break;
			}

			CKeyValueList query;
			query << CKeyValue ( "a",            "a" )
			      << CKeyValue ( "viewType",     "4" )
			      << CKeyValue ( "itemTypeInv",  QChar ( item-> itemType ( )-> id ( )))
			      << CKeyValue ( "itemNo",       item-> id ( ))
			      << CKeyValue ( "downloadType", "X" );

			m_trans-> get ( url, query, f );
			m_downloads_in_progress++;
		}
	}

	if ( failed ) {
		m_trans-> cancelAllJobs ( );
		return false;
	}

	while ( m_downloads_in_progress )
		qApp-> processEvents ( );

	return true;
}
