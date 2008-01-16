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
#include <locale.h>

#include <qapplication.h>
#include <qfile.h>
#include <qdir.h>
#include <qpainter.h>

#include <qsinterpreter.h>
#include <qsargument.h>
#include <qsinputdialogfactory.h>
#include <qsobjectfactory.h>
#include <qsutilfactory.h>

#include "cutility.h"
#include "cresource.h"
#include "cmoney.h"
#include "creport.h"
#include "creport_p.h"
#include "cconfig.h"

#include "creportobjects.h"

namespace {

static inline QSArgument evaluate ( QSInterpreter *interp, const QString &code, QObject *context = 0, const QString &scriptName = QString::null )
{
	QCString oldloc = ::setlocale ( LC_ALL, 0 );
	::setlocale ( LC_ALL, "C" );
	QSArgument res = interp-> evaluate ( code, context, scriptName );
	::setlocale ( LC_ALL, oldloc );
	return res;
}


class CReportFactory : public QSObjectFactory {
public:
    CReportFactory ( )
	{
		registerClass ( "Money", new CReportMoneyStatic ( interpreter ( )));
	}

    QObject *create ( const QString &, const QSArgumentList &, QObject * )
	{
		return 0;
	}
};

}


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
	return d-> m_label. isEmpty ( ) ? d-> m_name : d-> m_label;
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
		d-> m_interpreter-> addObjectFactory ( new QSInputDialogFactory ( ));
		d-> m_interpreter-> addObjectFactory ( new QSUtilFactory ( ));
		d-> m_interpreter-> addObjectFactory ( new CReportFactory ( ));

		evaluate ( d-> m_interpreter, d-> m_code, 0, fi. baseName ( ));

		if ( !d-> m_interpreter-> hadError ( )) {
			d-> m_name = fi. baseName ( );

			if ( d-> m_interpreter-> hasFunction ( "load" ) && d-> m_interpreter-> hasFunction ( "print" )) {
				QSArgument res = d-> m_interpreter-> call ( "load" );

				if ( !d-> m_interpreter-> hadError ( )) {	
					d-> m_loaded = fi. lastModified ( ). toTime_t ( );

					if ( res. type ( ) == QSArgument::Variant && res. variant ( ). type ( ) == QVariant::Map )
						d-> m_label = res. variant ( ). asMap ( ) ["name"]. toString ( );
				}
			}
		}
	}
	else
		qDebug ( "CReport::load() - failed to open %s", filename. latin1 ( ));

	return ( d-> m_loaded >= 0 );
}

void CReport::print ( QPaintDevice *pd, const CDocument *doc, const CDocument::ItemList &items ) const
{
	CDocument::Statistics stat = doc-> statistics ( items );

	QMap<QString, QVariant> docmap;
	docmap ["fileName"] = doc-> fileName ( );
	docmap ["title"]    = doc-> title ( );

	QMap<QString, QVariant> statmap;
	statmap ["errors"]   = stat. errors ( );
	statmap ["items"]    = stat. items ( );
	statmap ["lots"]     = stat. lots ( );
	statmap ["minValue"] = stat. minValue ( ). toDouble ( );
	statmap ["value"]    = stat. value ( ). toDouble ( );
	statmap ["weight"]   = stat. weight ( );

	docmap ["statistics"] = statmap;

	if ( doc-> order ( )) {
		const BrickLink::Order *order = doc-> order ( );
		QMap<QString, QVariant> ordermap;

		ordermap ["id"]           = order-> id ( );
		ordermap ["type"]         = ( order-> type ( ) == BrickLink::Order::Placed ? "P" : "R" );
		ordermap ["date"]         = order-> date ( ). toString ( Qt::TextDate );
		ordermap ["statusChange"] = order-> statusChange ( );
		ordermap ["other"]        = order-> other ( );
        ordermap ["buyer"]        = ( order-> type ( ) == BrickLink::Order::Placed ? CConfig::inst()->blLoginUsername() : order-> other ( ));
        ordermap ["seller"]       = ( order-> type ( ) == BrickLink::Order::Placed ? order-> other ( ) : CConfig::inst()->blLoginUsername());
		ordermap ["shipping"]     = order-> shipping ( ). toDouble ( );
		ordermap ["insurance"]    = order-> insurance ( ). toDouble ( );
		ordermap ["delivery"]     = order-> delivery ( ). toDouble ( );
		ordermap ["credit"]       = order-> credit ( ). toDouble ( );
		ordermap ["grandTotal"]   = order-> grandTotal ( ). toDouble ( );
		ordermap ["status"]       = order-> status ( );
		ordermap ["payment"]      = order-> payment ( );
		ordermap ["remarks"]      = order-> remarks ( );
		ordermap ["address"]      = order-> address ( );

		docmap ["order"] = ordermap;
	}

	QValueList<QVariant> itemslist;

	foreach ( CDocument::Item *item, items ) {
		QMap<QString, QVariant> imap;

		imap ["id"]        = item-> item ( )-> id ( );
		imap ["name"]      = item-> item ( )-> name ( );

		BrickLink::Picture *pic = BrickLink::inst ( )-> picture ( item-> item ( ), item-> color ( ), true );
		imap ["picture"]   = pic ? pic-> pixmap ( ) : QPixmap ( );

		QMap<QString, QVariant> status;
		status ["include"] = ( item-> status ( ) == BrickLink::InvItem::Include );
		status ["exclude"] = ( item-> status ( ) == BrickLink::InvItem::Exclude );
		status ["extra"]   = ( item-> status ( ) == BrickLink::InvItem::Extra );
		imap ["status"]    = status;

		imap ["quantity"]  = item-> quantity ( );

		QMap<QString, QVariant> color;
		color ["id"]       = item-> color ( ) ? (int) item-> color ( )-> id ( ) : -1;
		color ["name"]     = item-> color ( ) ? item-> color ( )-> name ( ) : "";
		color ["rgb"]      = item-> color ( ) ? item-> color ( )-> color ( ) : QColor ( );
		QPixmap colorpix;
		colorpix. convertFromImage ( BrickLink::inst ( )-> colorImage ( item-> color ( ), 20, 20 ));
		color ["picture"]  = colorpix;
		imap ["color"]     = color;

		QMap<QString, QVariant> cond;
		cond ["new"]       = ( item-> condition ( ) == BrickLink::New );
		cond ["used"]      = ( item-> condition ( ) == BrickLink::Used );
		imap ["condition"] = cond;

		imap ["price"]     = item-> price ( ). toDouble ( );
		imap ["total"]     = item-> total ( ). toDouble ( );
		imap ["bulkQuantity"] = item-> bulkQuantity ( );
		imap ["sale"]      = item-> sale ( );
		imap ["remarks"]   = item-> remarks ( );
		imap ["comments"]  = item-> comments ( );

		QMap<QString, QVariant> category;
		category ["id"]    = item-> category ( )-> id ( );
		category ["name"]  = item-> category ( )-> name ( );
		imap ["category"]  = category;

		QMap<QString, QVariant> itemtype;
		itemtype ["id"]    = item-> itemType ( )-> id ( );
		itemtype ["name"]  = item-> itemType ( )-> name ( );
		imap ["itemType"]  = itemtype;

		QValueList<QVariant> tq;
		tq << item-> tierQuantity ( 0 ) << item-> tierQuantity ( 1 ) << item-> tierQuantity ( 2 );
		imap ["tierQuantity"] = tq;

		QValueList<QVariant> tp;
		tp << item-> tierPrice ( 0 ). toDouble ( ) << item-> tierPrice ( 1 ). toDouble ( ) << item-> tierPrice ( 2 ). toDouble ( );
		imap ["tierPrice"] = tp;

		imap ["lotId"]     = item-> lotId ( );
		imap ["retain"]    = item-> retain ( );
		imap ["stockroom"] = item-> stockroom ( );
		imap ["reserved"]  = item-> reserved ( );

		itemslist << imap;
	}
	docmap ["items"] = itemslist;


	CReportJob *job = new CReportJob ( pd );
	d-> m_interpreter-> addTransientObject ( new CReportUtility ( ));
	d-> m_interpreter-> addTransientObject ( job );
	d-> m_interpreter-> addTransientVariable ( "Document", QSArgument (  docmap ));

	d-> m_interpreter-> call ( "print" );

	if ( !d-> m_interpreter-> hadError ( )) {
		if ( !job-> isAborted ( )) {
			job-> dump ( );

			job-> print ( 0, job-> pageCount ( ) - 1 );
		}
	}
	d-> m_interpreter-> clear ( );
	evaluate ( d-> m_interpreter, d-> m_code, 0, d-> m_name );
	
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

	QString dir = CResource::inst ( )-> locate ( "print-templates", CResource::LocateDir );

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

