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
#include <limits.h>

#include <qapplication.h>
#include <qfile.h>
#include <qdir.h>
#include <qpainter.h>

#include <qsinterpreter.h>
#include <qsargument.h>
#include <qsinputdialogfactory.h>

#include "cutility.h"
#include "cresource.h"
#include "cmoney.h"
#include "creport.h"
#include "creport_p.h"

#include "reportobjects.h"


namespace {

static int mm2pt ( int mm )
{
	return int( mm * 72.0 / 25.4 );
}

static int parseUnit ( const QString &str )
{
	float scale = 1.f;

	QString unit = str. right ( 2 );
	bool use_unit = true;

	if ( unit == "pt" )
		scale = 1.f;
	else if ( unit == "cm" )
		scale = 72.f * 1.f / 2.54f;
	else if ( unit == "mm" )
		scale = 72.f * .1f * ( 1.f / 2.54f );
	else if ( unit == "in" )
		scale = 72.f;
	else
		use_unit = false;

	return int( scale * ( use_unit ? str. left ( str. length ( ) - 2 ) : str ). toFloat ( ));
}

static QRect pt2px ( const QRect &r, const float s [2] )
{
	return QRect ( int( r. x ( ) * s [0] ), int( r. y ( ) * s [1] ), int( r. width ( ) * s [0] ), int( r. height ( ) * s [1] ));
}

static int pt2px ( int p, float s )
{
	return int( p * s );
}

static int px2pt ( int p, float s )
{
	return int( p / s );
}

#if 0
QSize CReport::pageSizePt ( ) const
{
	if ( d-> m_pagesize == QPrinter::A4 )
		return QSize ( mm2pt ( 210 ), mm2pt ( 297 ));
	else if ( d-> m_pagesize == QPrinter::Letter )
		return QSize ( mm2pt ( 216 ), mm2pt ( 279 ));
	else
		return QSize ( );
}
#endif


} // namespace


CReport::CReport ( )
{
	d = new CReportPrivate ( );
	d-> m_loaded = -1;
	d-> m_interpreter = 0;
}

CReport::~CReport ( )
{
	delete d-> m_interpreter;
	delete d;
}

QString CReport::name ( ) const
{
	return d-> m_name;
}

bool CReport::load ( const QString &filename )
{
	QFileInfo fi ( filename );

	// deleted or updated?
	if (( d-> m_loaded >= 0 ) && (( !fi. exists ( )) || ( d-> m_loaded < (time_t) fi. lastModified ( ). toTime_t ( )))) {
		// unload
		delete d-> m_interpreter;
		d-> m_interpreter = 0;
		d-> m_code = QString ( );
		d-> m_loaded = -1;
	}

	if ( d-> m_loaded >= 0 )
		return false;

	QFile f ( filename );
	if ( f. open ( IO_ReadOnly )) {
		QTextStream ts ( &f );
		d-> m_code = ts. read ( );

		d-> m_interpreter = new QSInterpreter ( this, "report_interpreter" );
		d-> m_interpreter-> evaluate ( d-> m_code, 0, filename );

		if ( !d-> m_interpreter-> hadError ( )) {
			d-> m_name = fi. baseName ( );

			if ( d-> m_interpreter-> hasFunction ( "load" ) && d-> m_interpreter-> hasFunction ( "print" )) {
				QSArgument res = d-> m_interpreter-> call ( "load" );

				if ( !d-> m_interpreter-> hadError ( )) {	
					d-> m_interpreter-> addObjectFactory ( new QSInputDialogFactory ( ));

					d-> m_loaded = fi. lastModified ( ). toTime_t ( );
				}
			}
		}
	}
	else
		qDebug ( "CReport::load() - failed to open %s", filename. latin1 ( ));

	return ( d-> m_loaded >= 0 );
}

void CReport::print ( QPainter *p, const CDocument *doc, const CDocument::ItemList &items ) const
{
	CDocument::Statistics stat = doc-> statistics ( items );

	QMap<QString, QVariant> docmap;
	docmap ["filename"] = doc-> fileName ( );
	docmap ["title"]    = doc-> title ( );

	QMap<QString, QVariant> statmap;
	statmap ["errors"]   = stat. errors ( );
	statmap ["items"]    = stat. items ( );
	statmap ["lots"]     = stat. lots ( );
	statmap ["minvalue"] = stat. minValue ( ). toCString ( );
	statmap ["value"]    = stat. value ( ). toCString ( );
	statmap ["weight"]   = stat. weight ( );

	docmap ["statistics"] = statmap;



	if ( doc-> order ( )) {
		const BrickLink::Order *order = doc-> order ( );
		QMap<QString, QVariant> ordermap;

		ordermap ["id"]           = order-> id ( );
		ordermap ["type"]         = ( order-> type ( ) == BrickLink::Order::Placed ? "P" : "R" );
		ordermap ["date"]         = order-> date ( ). toString ( Qt::TextDate );
		ordermap ["statuschange"] = order-> statusChange ( );
		ordermap ["buyer"]        = order-> buyer ( );
		ordermap ["shipping"]     = order-> shipping ( ). toCString ( );
		ordermap ["insurance"]    = order-> insurance ( ). toCString ( );
		ordermap ["delivery"]     = order-> delivery ( ). toCString ( );
		ordermap ["credit"]       = order-> credit ( ). toCString ( );
		ordermap ["grandtotal"]   = order-> grandTotal ( ). toCString ( );
		ordermap ["status"]       = order-> status ( );
		ordermap ["payment"]      = order-> payment ( );
		ordermap ["remarks"]      = order-> remarks ( );

		docmap ["order"] = ordermap;
	}

	QValueList<QVariant> itemslist;

	foreach ( CDocument::Item *item, items ) {
		QMap<QString, QVariant> imap;

		imap ["id"]       = item-> item ( )-> id ( );
		imap ["name"]     = item-> item ( )-> name ( );
		imap ["quantity"] = item-> quantity ( );

		QMap<QString, QVariant> colormap;
		colormap ["id"]   = item-> color ( ) ? (int) item-> color ( )-> id ( ) : -1;
		colormap ["name"] = item-> color ( ) ? item-> color ( )-> name ( ) : "";
		colormap ["rgb"]  = item-> color ( ) ? item-> color ( )-> color ( ) : QColor ( );
		imap ["color"] = colormap;

		itemslist << imap;
	}

	ReportJob *job = new ReportJob ( p-> device ( ));
	d-> m_interpreter-> addTransientObject ( job );

	QSArgumentList args;
	args << QSArgument( docmap ) << QSArgument( itemslist );
	d-> m_interpreter-> call ( "print", args );

	if ( !d-> m_interpreter-> hadError ( )) {
		// check abort 


	}
	d-> m_interpreter-> clear ( );
	d-> m_interpreter-> evaluate ( d-> m_code, 0, d-> m_name );
	
	delete job;
}


CReportManager::CReportManager ( )
{
	m_reports. setAutoDelete ( true );
	m_printer = 0;
}

CReportManager::~CReportManager ( )
{
	delete m_printer;
	s_inst = 0;
}

CReportManager *CReportManager::s_inst = 0;

CReportManager *CReportManager::inst ( )
{
	if ( !s_inst )
		s_inst = new CReportManager ( );
	return s_inst;
}

bool CReportManager::reload ( )
{
	m_reports. clear ( );

	QString dir = CResource::inst ( )-> locate ( "reports", CResource::LocateDir );

	if ( !dir. isNull ( )) {
		QDir d ( dir );

		QStringList files = d. entryList ( "*.qs" );

		for ( QStringList::iterator it = files. begin ( ); it != files. end ( ); ++it ) {
			CReport *rep = new CReport ( );

			if ( rep-> load ( d. absFilePath ( *it )))
				m_reports. append ( rep );
			else
				delete rep;
		}
	}
	return false;
}

const QPtrList <CReport> &CReportManager::reports ( ) const
{
	return m_reports;
}

QPrinter *CReportManager::printer ( ) const
{
	if ( !m_printer )
		m_printer = new QPrinter ( QPrinter::HighResolution );
		
	return m_printer;
}

