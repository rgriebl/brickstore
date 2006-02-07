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
#include <qcolor.h>
#include <qpainter.h>
#include <qpaintdevicemetrics.h>
#include <qprinter.h>
#include <qdom.h>
#include <qfile.h>
#include <qdir.h>

#include <qlabel.h>
#include <qlineedit.h>
#include <qdatetimeedit.h>
#include <qlayout.h>
#include <qvalidator.h>
#include <qtextedit.h>

#include "cutility.h"
#include "cresource.h"
#include "cmoney.h"
#include "creport.h"
#include "creport_p.h"

#include "dlgreportuiimpl.h"

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

static QString getAttr ( const QDomNode &node, const QString &name )
{
	QString value;
	QDomNamedNodeMap map = node. attributes ( );
	
	for ( uint i = 0; i < map. count ( ); i++ ) {
		QDomAttr attr = map. item ( i ). toAttr ( );
	
		if ( attr. isNull ( ))
			continue;
	
		QString aname = attr. name ( ). stripWhiteSpace ( );
		QString avalue = attr. value ( ). stripWhiteSpace ( );

		if ( aname == name ) {
			value = avalue;
			break;
		}
	}
	return value;
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


static void insertVarsInto ( const CReportVariables &ins, CReportVariables &into )
{
	for ( CReportVariables::const_iterator it = ins. begin ( ); it != ins. end ( ); ++it ) 
		into. insert ( it. key ( ),it. data ( ), true );
}

static void removeVarsFrom ( const CReportVariables &rem, CReportVariables &from )
{
	for ( CReportVariables::const_iterator it = rem. begin ( ); it != rem. end ( ); ++it ) 
		from. remove ( it. key ( ));
}


static QString substitute ( QDomNode root, CReportVariables &vars )
{
	QString str;
	bool last_was_text = true;

	for ( QDomNode node = root. firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
		QString append;
		QString format;
		QStringList formatopt;

		if ( node. isText ( )) {
			append = node. nodeValue ( );
			last_was_text = true;
		}
		else if ( node. isElement ( )) {
			QDomElement e = node. toElement ( );
		
			if ( !last_was_text )
				str += " ";

			if ( e. nodeName ( ) == "var" ) {
				append = vars [getAttr ( e, "name" )];
				format = getAttr ( e, "format" );
			}
			else if ( e. nodeName ( ) == "setvar" ) {
				QString varname = getAttr ( e, "name" );

				if ( !varname. isEmpty ( ))
					vars [varname] = substitute ( node, vars );
			}
			else if ( e. nodeName ( ) == "func" ) {
				CReportFunction *f = CReportManager::inst ( )-> functions ( ) [getAttr ( e, "name" )];
				format = getAttr ( e, "format" );

				if ( f ) {
					QStringList argv;

					for ( QDomNode argnode = node. firstChild ( ); !argnode. isNull ( ); argnode = argnode. nextSibling ( )) {
						if ( argnode. isElement ( ) && ( argnode. toElement ( ). nodeName ( ) == "arg" )) {
							argv << substitute ( argnode, vars );
						}
					}

					while ( QABS( f-> argc ( )) < int( argv. count ( )))
						argv << QString ( );

					append = ( *f ) ( argv );
				}
			}

			formatopt = QStringList::split ( ':', format, true );
			format = formatopt [0];
			formatopt. pop_front ( );

			if ( format == "double" ) {
				double v = append. toDouble ( );

				if ( formatopt. count ( ) >= 1 )
					append = QString::number ( v, 'f', formatopt [0]. toInt ( ));
				else
					append = QString::number ( v );
			}
			else if ( format == "date" ) {
				QDate dt;
				dt. fromString ( append, Qt::TextDate );

				if (( formatopt. count ( ) >= 1 ) && ( formatopt [0] == "local" ))
					append = dt. toString ( Qt::LocalDate );
				else if ( formatopt. count ( ) >= 1 )
					append = dt. toString ( formatopt [0] );
				else
					append = dt. toString ( Qt::ISODate );
			}
#if 0 // using double and formating yourself seems to be a better idea
			else if ( format == "currency" ) {
				double v = append. toDouble ( );

				if (( formatopt. count ( ) >= 1 ) && ( formatopt [0] == "local" ))
					append = CMoney::inst ( )-> toString ( v );
				else
					append = "$" + QString::number ( v, 'f', 3 );
			}
#endif

			last_was_text = false;
		}
		str += append;
	}

	return str; //. simplifyWhiteSpace ( );
}

} // namespace


CReport::CReport ( )
{
	d = new CReportPrivate ( );
}

CReport::~CReport ( )
{
	delete d;
}

QString CReport::name ( ) const
{
	return d-> m_name;
}

QPrinter::PageSize CReport::pageSize ( ) const
{
	return d-> m_pagesize;
}

QSize CReport::pageSizePt ( ) const
{
	if ( d-> m_pagesize == QPrinter::A4 )
		return QSize ( mm2pt ( 210 ), mm2pt ( 297 ));
	else if ( d-> m_pagesize == QPrinter::Letter )
		return QSize ( mm2pt ( 216 ), mm2pt ( 279 ));
	else
		return QSize ( );
}

uint CReport::pageCount ( uint itemcount ) const
{
	uint pagecount = 0;
	int itemflip = 0;

	while ( itemcount ) {
		pagecount++;

		int ph = pageSizePt ( ). height ( ) - ( d-> m_margin_top + d-> m_margin_bottom );

		ph -= d-> m_page_header. height ( );
		ph -= d-> m_page_footer. height ( );

		if ( pagecount == 1 )
			ph -= d-> m_report_header. height ( );

		ph -= d-> m_list_header. height ( );
		ph -= d-> m_list_footer. height ( );
			
		while ( itemcount ) {
			int ih = d-> m_item [itemflip]. height ( );

			if (( ph - ih ) < 0 )
				break;
			ph -= ih;
			
			itemflip = 1 - itemflip;
			itemcount--;
		}

		if ( !itemcount && ( ph < d-> m_report_footer. height ( )))
			pagecount++;
		
		itemflip = 0;
	}
	return pagecount;
}

#define PRINT_THIS_PAGE         (( page >= from ) && ( page <= to ))

void CReport::render ( const BrickLink::InvItemList &items, const CReportVariables &add_vars, int from, int to, QPainter *p ) const
{
	if ( items. isEmpty ( ) || !p )
		return;
	if ( from < 1 )
		from = 1;
	if ( to < 1 )
		to = INT_MAX;

	QPrinter *printer = (( p-> device ( )-> devType ( ) & QInternal::DeviceTypeMask ) == QInternal::Printer ) ? (QPrinter *) p-> device ( ) : 0;

	bool skip_newpage = true;

	if ( !printer )
		to = from;

	int page = 0;
	int pagecount = pageCount ( items. count ( ));
	
	money_t price_total = 0, price_ptotal = 0;
	int lot_total = 0, lot_ptotal = 0;
	int qty_total = 0, qty_ptotal = 0;
	
	int itemflip = 0;
	int items_left = items. count ( );
	bool rfooter = false;

	CReportVariables vars = add_vars;
	vars ["date"] = QDate::currentDate ( ). toString ( Qt::TextDate );
	vars ["time"] = QTime::currentTime ( ). toString ( Qt::TextDate );

	vars ["currency-symbol"] = CMoney::inst ( )-> currencySymbol ( );
	vars ["currency-exchange-rate"] = QString::number ( CMoney::inst ( )-> factor ( ));

	vars ["pages"] = QString::number ( pagecount );

	foreach ( const BrickLink::InvItem *ii, items ) {
		qty_total += ii-> quantity ( );
		price_total += ( ii-> price ( ) * ii-> quantity ( ));
		lot_total++;
	}
	
	vars ["qty-grand-total"]   = QString::number ( qty_total );
	vars ["lot-grand-total"]   = QString::number ( lot_total );
	vars ["price-grand-total"] = price_total. toCString ( );

	if ( hasUI ( )) {
		DlgReportUIImpl dlg ( qApp-> mainWidget ( ), "ReportUI", true );
		dlg. setContent ( createUI ( vars ));
		dlg. setCaption ( name ( ));

		if ( dlg. exec ( ) != QDialog::Accepted )
			return;
			
		CReportVariables ui_vars = getValuesFromUI ( dlg. content ( ));

		insertVarsInto ( ui_vars, vars );
	}

	// scale for pt -> px conversions
	QPaintDeviceMetrics pdm ( p-> device ( ));
	
	QSize ps_pt = pageSizePt ( );  // page size in pt
	QSize ds_pt = QSize ( mm2pt ( pdm. widthMM ( )), mm2pt ( pdm. heightMM ( ))); // device size in pt
	
	QRect pagerect = QRect ( QPoint ( 0, 0 ), ps_pt );
	pagerect. addCoords ( d-> m_margin_left, d-> m_margin_top, -d-> m_margin_right, -d-> m_margin_bottom );

	// scale to device size PLUS convert from pt to pixel
	float s [2];
	s [0] = ( float( ds_pt. width ( )) / float( ps_pt. width ( ))) * float( pdm. logicalDpiX ( )) / 72.f,
	s [1] =	( float( ds_pt. height ( )) / float( ps_pt. height ( ))) * float( pdm. logicalDpiY ( )) / 72.f;

	
	BrickLink::InvItemList::const_iterator itemit = items. begin ( );

	while ( items_left || !rfooter ) {
		page++;

		vars ["page"] = QString::number ( page );

		QRect r = pagerect;
		
		if ( PRINT_THIS_PAGE ) {
			if ( printer && !skip_newpage )
				printer-> newPage ( );
			skip_newpage = false;
			d-> m_page_header. render ( p, QRect ( r. left ( ), r. top ( ), r. width ( ), d-> m_page_header. height ( )), s, vars );

			lot_ptotal = 0;
			price_ptotal = 0;
			qty_ptotal = 0;
		}

		r. rTop ( ) += d-> m_page_header. height ( );

		if ( page == 1 ) {
			if ( PRINT_THIS_PAGE )
				d-> m_report_header. render ( p, QRect ( r. left ( ), r. top ( ), r. width ( ), d-> m_report_header. height ( )), s, vars );
				
			r. rTop ( ) += d-> m_report_header. height ( );
		}

		if ( items_left ) {
			if ( PRINT_THIS_PAGE )
				d-> m_list_header. render ( p, QRect ( r. left ( ), r. top ( ) , r. width ( ), d-> m_list_header. height ( )), s, vars );
				
			r. rTop ( ) += d-> m_list_header. height ( );
		}

		int fh = d-> m_list_footer. height ( ) + d-> m_page_footer. height ( );
		bool items_on_page = false;
		
		while ( items_left ) {
			int ih = d-> m_item [itemflip]. height ( );

			if (( r. height ( ) - fh - ih ) < 0 )
				break;

			if ( PRINT_THIS_PAGE && ( itemit != items. end ( ))) {
				const BrickLink::InvItem *blitem = *itemit;

				// fill vars according to blitem

				CReportVariables itvars;
				itvars ["item-id"]    = blitem-> item ( )-> id ( );
				itvars ["item-name"]  = blitem-> item ( )-> name ( );
				itvars ["item-color-id"]   = blitem-> color ( )-> id ( );
				itvars ["item-color-name"] = blitem-> color ( )-> name ( );
				itvars ["item-color-rgb"] = blitem-> color ( )-> color ( ). name ( );
				itvars ["item-category-id"]  = blitem-> category ( )-> id ( );
				itvars ["item-category-name"] = blitem-> category ( )-> name ( );
				itvars ["item-type-id"] = QString ( QChar ( blitem-> itemType ( )-> id ( )));
				itvars ["item-type-name"] = blitem-> itemType ( )-> name ( );
				itvars ["item-qty"] = QString::number ( blitem-> quantity ( ));
				itvars ["item-price"] = blitem-> price ( ). toCString ( );
				itvars ["item-price-total"] = ( blitem-> price ( ) * blitem-> quantity ( )). toCString ( );
				itvars ["item-bulk"] = QString::number ( blitem-> sale ( ));
				itvars ["item-sale"] = QString::number ( blitem-> bulkQuantity ( ));
				switch ( blitem-> condition ( )) {
					case BrickLink::New : itvars ["item-condition-short"] = tr( "N" );
					                      itvars ["item-condition-long"]  = tr( "New" );
					                      break;
					case BrickLink::Used: itvars ["item-condition-short"] = tr( "U" );
					                      itvars ["item-condition-long"] = tr( "Used" );
					                      break;
					default             : itvars ["item-condition-short"] = "-";
					                      itvars ["item-condition-long"] = "-";
					                      break;
				}
				itvars ["item-comments"] = blitem-> comments ( );
				itvars ["item-remarks"] = blitem-> remarks ( );
				itvars ["item-tq1"] = QString::number ( blitem-> tierQuantity ( 0 ));
				itvars ["item-tq2"] = QString::number ( blitem-> tierQuantity ( 1 ));
				itvars ["item-tq3"] = QString::number ( blitem-> tierQuantity ( 2 ));
				itvars ["item-tp1"] = blitem-> tierPrice ( 0 ). toCString ( );
				itvars ["item-tp2"] = blitem-> tierPrice ( 1 ). toCString ( );
				itvars ["item-tp3"] = blitem-> tierPrice ( 2 ). toCString ( );
				
				QString image_key = "item-image-";

				const BrickLink::Picture *pic = BrickLink::inst ( )-> picture ( blitem-> item ( ), blitem-> color ( ), true );

				if ( pic && pic-> valid ( )) {
					image_key += pic-> key ( );

					QMimeSourceFactory::defaultFactory ( )-> setImage ( image_key, pic-> image ( ));
				}
				itvars ["item-image-mimesource"] = image_key;

				insertVarsInto ( itvars, vars );

				d-> m_item [itemflip]. render ( p, QRect ( r. left ( ), r. top ( ), r. width ( ), ih ), s, vars );

				removeVarsFrom ( itvars, vars );

				lot_ptotal ++;
				qty_ptotal += blitem-> quantity ( );
				price_ptotal += ( blitem-> price ( ) * blitem-> quantity ( ));
			}
			r. rTop ( ) += ih;
			
			itemflip = 1 - itemflip;
			items_left--;
			++itemit;
			items_on_page = true;
		}

		vars ["qty-page-total"]   = QString::number ( qty_ptotal );
		vars ["lot-page-total"]   = QString::number ( lot_ptotal );
		vars ["price-page-total"] = price_ptotal. toCString ( );

		if ( items_on_page ) {
			if ( PRINT_THIS_PAGE )
				d-> m_list_footer. render ( p, QRect ( r. left ( ), r. top ( ), r. width ( ), d-> m_list_footer. height ( )), s, vars );
				
			r. rTop ( ) += d-> m_list_footer. height ( );
		}

		if ( !items_left && !rfooter && ( r. height ( ) >= ( d-> m_report_footer. height ( ) + d-> m_page_footer. height ( )))) {
			if ( PRINT_THIS_PAGE )
				d-> m_report_footer. render ( p, QRect ( r. left ( ), r. top ( ), r. width ( ), d-> m_report_footer. height ( )), s, vars );
			rfooter = true;
		}
		
		if ( PRINT_THIS_PAGE )
			d-> m_page_footer. render ( p, QRect ( r. left ( ), r. bottom ( ) - d-> m_page_footer. height ( ), r. width ( ), d-> m_page_footer. height ( )), s, vars );

		itemflip = 0;
	}
}

#undef PRINT_THIS_PAGE


bool CReport::load ( const QString &filename )
{
	if ( d-> m_loaded )
		return false;

	QDomDocument doc;
	QFile file ( filename );

	if ( file. open ( IO_ReadOnly )) {
		QString err_str;
		int err_line = 0, err_col = 0;
	
		if ( doc. setContent ( &file, &err_str, &err_line, &err_col )) {
			QDomElement root = doc. documentElement ( );

			qDebug ( "FILE=%s ROOT=%s", filename.latin1(), root.nodeName().latin1());
			
			if ( root. isElement ( ) && root. nodeName ( ) == "report" ) {
				d-> parseAttr ( root. toElement ( ));
				
				for ( QDomNode node = root. firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
					if ( !node. isElement ( ))
						continue;
					QDomElement e = node. toElement ( );
					
					if ( e. nodeName ( ) == "default-style" ) {
						d-> m_default_style. parseAttr ( e );
					}
					else if ( e. nodeName ( ) == "ui" ) {
						d-> parseUI ( e );
					}
					else {
						int h = parseUnit ( getAttr ( e, "height" ));
						ReportPart *rp = 0;
						bool item_part = false;
						
						if ( e. nodeName ( ) == "page-header" )
							rp = &d-> m_page_header;
						else if ( e. nodeName ( ) == "page-footer" )
							rp = &d-> m_page_footer;
						else if ( e. nodeName ( ) == "report-header" )
							rp = &d-> m_report_header;
						else if ( e. nodeName ( ) == "report-footer" )
							rp = &d-> m_report_footer;
						else if ( e. nodeName ( ) == "list-header" )
							rp = &d-> m_list_header;
						else if ( e. nodeName ( ) == "list-footer" )
							rp = &d-> m_list_footer;
						else if ( e. nodeName ( ) == "item" ) {
							rp = ( getAttr ( node, "even-odd" ) == "even" ) ? &d-> m_item [1] : &d-> m_item [0];
							item_part = true;
						}
							
						if ( rp ) {
							rp-> setDefaultStyle ( &d-> m_default_style );
							rp-> setHeight ( h );
							rp-> parseItem ( e, item_part );
						}
					}
				}

				d-> m_loaded = true;
				return true;
			}
			else
				qDebug ( "CReport::load() - root tag is not <report>" );
		}
		else
			qDebug ( "CReport::load() - error reading template: %s in line %d, column %d", err_str. latin1 ( ), err_line, err_col );
	}
	else
		qDebug ( "CReport::load() - failed to open %s", filename. latin1 ( ));

	return false;
}


bool CReport::hasUI ( ) const
{
	return !d-> m_ui. isEmpty ( );
}

void CReportPrivate::parseUI ( QDomElement root )
{
	m_ui. clear ( );

	for ( QDomNode node = root. firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
		if ( !node. isElement ( ))
			continue;
		QDomElement e = node. toElement ( );
		
		if ( e. nodeName ( ) == "getvar" ) {
			ReportUIElement *uie = new ReportUIElement ( );

			uie-> m_name = getAttr ( e, "name" );
			uie-> m_description = getAttr ( e, "description" );
			QString type = getAttr ( e, "type" );

			if ( type == "string" )
				uie-> m_type = ReportUIElement::String;
			else if ( type == "multistring" )
				uie-> m_type = ReportUIElement::MultiString;
			else if ( type == "date" )
				uie-> m_type = ReportUIElement::Date;
			else if ( type == "double" )
				uie-> m_type = ReportUIElement::Double;
			else if ( type == "int" )
				uie-> m_type = ReportUIElement::Int;

			if (( uie-> m_type != ReportUIElement::Invalid ) && !uie-> m_name. isEmpty ( ) && !uie-> m_description. isEmpty ( )) {
				uie-> m_default = e. cloneNode ( ). toElement ( );

				m_ui. append ( uie );
			}
			else
				delete uie;
		}
	}
}

QWidget *CReport::createUI ( CReportVariables &vars ) const
{
	QWidget *ui = new QWidget ( 0, name ( ). utf8 ( ));
	QGridLayout *grid = new QGridLayout ( ui, d-> m_ui. count ( ), 2, 0, 6 );

	int row = 0;

	for ( QPtrListIterator <ReportUIElement> it ( d-> m_ui ); it. current ( ); ++it ) {
		ReportUIElement *uie = it. current ( );

		QWidget *edit;
		QString def = substitute ( uie-> m_default, vars );
		QCString name = uie-> m_name. utf8 ( );

		switch ( uie-> m_type ) {
			case ReportUIElement::String: 
				edit = new QLineEdit ( def, ui, name );
				break;

			case ReportUIElement::MultiString: 
				edit = new QTextEdit ( def, QString::null, ui, name );
				static_cast <QTextEdit *> ( edit )-> setWordWrap ( QTextEdit::NoWrap );
				static_cast <QTextEdit *> ( edit )-> setTextFormat ( QTextEdit::PlainText );
				break;

			case ReportUIElement::Double: 
				edit = new QLineEdit ( QString::number ( def. toDouble ( )), ui, name );
				static_cast <QLineEdit *> ( edit )-> setValidator ( new QDoubleValidator ( edit ));
				break;

			case ReportUIElement::Int:
				edit = new QLineEdit ( QString::number ( def. toInt ( )), ui, name );
				static_cast <QLineEdit *> ( edit )-> setValidator ( new QIntValidator ( edit ));
				break;

			case ReportUIElement::Date: {
				edit = new QDateEdit ( ui, name );
				/*QStringList sl = QStringList::split ( QChar ( '/' ), def, false );
				if ( sl. count ( ) == 3 )
					static_cast <QDateEdit *> ( edit )-> setDate ( QDate ( sl [2]. toInt ( ), sl [0]. toInt ( ), sl [1]. toInt ( )));
				*/
				if ( !def. isEmpty ( ))
					static_cast <QDateEdit *> ( edit )-> setDate ( QDate::fromString ( def, Qt::TextDate ));
				break;							
			}
			
			default:
				edit = 0;
				break;
		}

		if ( edit ) {
			QLabel *label = new QLabel ( edit, uie-> m_description, ui );

			grid-> addWidget ( label, row, 0 );
			grid-> addWidget ( edit, row, 1 );

			row++;
		}
	}

	return ui;
}

CReportVariables CReport::getValuesFromUI ( QWidget *ui ) const
{
	CReportVariables vars;

	if ( ui-> name ( ) != name ( ). utf8 ( ))
		return vars;

	for ( QPtrListIterator <ReportUIElement> it ( d-> m_ui ); it. current ( ); ++it ) {
		ReportUIElement *uie = it. current ( );

		QString key = uie-> m_name;
		QCString name = uie-> m_name. utf8 ( );
		QObject *o = ui-> child ( name );

		if ( !o )
			continue;

		switch ( uie-> m_type ) {
			case ReportUIElement::String: 
			case ReportUIElement::Double: 
			case ReportUIElement::Int:
				vars [key] = static_cast <QLineEdit *> ( o )-> text ( );
				break;

			case ReportUIElement::MultiString: 
				vars [key] = static_cast <QTextEdit *> ( o )-> text ( );
				qWarning ( "%s", static_cast <QTextEdit *> ( o )-> text ( ). latin1( ));
				break;

			case ReportUIElement::Date:
				vars [key] = static_cast <QDateEdit *> ( o )-> date ( ). toString ( Qt::TextDate );
				break;
				
			default:
				break;
		}
	}

	return vars;
}

CReportPrivate::CReportPrivate ( )
{
	m_pagesize = QPrinter::A4;
	m_loaded = false;

	m_margin_left = m_margin_top = m_margin_right = m_margin_bottom = 0;

	m_ui. setAutoDelete ( true );
}

void CReportPrivate::parseAttr ( const QDomElement &node )
{
	QDomNamedNodeMap map = node. attributes ( );
	
	for ( uint i = 0; i < map. count ( ); i++ ) {
		QDomAttr attr = map. item ( i ). toAttr ( );
	
		if ( attr. isNull ( ))
			continue;
	
		QString aname = attr. name ( ). stripWhiteSpace ( );
		QString avalue = attr. value ( ). stripWhiteSpace ( );

		if ( aname == "name" ) {
			m_name = avalue;
		}
		else if ( aname == "page-size" ) {
			if ( avalue == "A4" )
				m_pagesize = QPrinter::A4;
			else if ( avalue == "Letter" )
				m_pagesize = QPrinter::Letter;
		}
		else if ( aname == "margin-left" ) {
			m_margin_left = parseUnit ( avalue );
		}
		else if ( aname == "margin-top" ) {
			m_margin_top = parseUnit ( avalue );
		}
		else if ( aname == "margin-right" ) {
			m_margin_right = parseUnit ( avalue );
		}
		else if ( aname == "margin-bottom" ) {
			m_margin_bottom = parseUnit ( avalue );
		}
	}
}





/*




${filename}
${date}
${page}
${pages}
${filter}


<report name="..." page-size="A4">
  <default [css] />
  <page>
    <text x=0 y=1cm width=20cm height=2cm>BrickStore report for <filename /></text>
    <text align="center">Page <page />/<pages /></text>
	<img src="" />
	<line x= y= width= height= linetype=solid|dotted|dashed linewidth=1pt />

    <items x=2cm y=3cm width=16cm height=24cm />
  </page>
  <item even-odd=any|even|odd>
    <text>lfkdkf</text>
    <status type=image|text />
	...
	<part-image />
	<conditon type=short|long />
	<color type=text|color-text|color-rect|text-in-color-rect|text-besides-color-rect />
	<price format="..." />
  </item>
</report>

PROPERTIES:
 top     = 1pt|mm|in
 left    = 1pt|mm|in
 width   = 1pt|mm|in
 height  = 1pt|mm|in
 
 background-color = #rrggbb|none
(background-image = "URL")  // SCALE!!!
(background-repeat= repeat|repeat-x|repeat-y|no-repeat)

 color           = #rrggbb
 font-family     = serif|sans-serif|monospace|<fontname>
 font-size       = 1pt|mm|in
 font-weight     = bold|normal|100-900
 font-style      = italic|oblique|normal
 text-decoration = underline|none

 text-align      = left|center|right
 white-space     = normal|no-wrap
 vertical-align  = top|middle|bottom
 
report name=<string> page-size=A4|Letter margin-top=<unit> margin-left=<unit> margin-right=<unit> margin-bottom=<unit>
  default [css]
  
  page-header height=<unit>
    text  [pos] [css]
    image [pos] [css] src=<url>
    line  [pos] [css] linetype=solid|dashed|dotted linewidth=<unit>

  page-footer height=<unit>
    text  [pos] [css]
  
  list-header height=<unit>

  list-footer height=<unit>

  report-header height=<unit>
  report-footer height=<unit>
  
  item even-odd=any|even|odd height=<unit>
    <all> [pos] [css]

	status 
    color type=text|color-text|color-rect|text-in-color-rect|text-besides-color-rect
	


PROPERTIES:
 top     = 1pt|mm|in
 left    = 1pt|mm|in
 width   = 1pt|mm|in
 height  = 1pt|mm|in
 
 background-color = #rrggbb|none
(background-image = "URL")  // SCALE!!!
(background-repeat= repeat|repeat-x|repeat-y|no-repeat)

 color           = #rrggbb
 font-family     = serif|sans-serif|monospace|<fontname>
 font-size       = 1pt|mm|in
 font-weight     = bold|normal|100-900
 font-style      = italic|oblique|normal
 text-decoration = underline|none

 text-align      = left|center|right
 white-space     = normal|no-wrap
 vertical-align  = top|middle|bottom
*/

CReportManager::CReportManager ( )
{
	m_reports. setAutoDelete ( true );
	m_printer = 0;

	m_functions. setAutoDelete ( true );
	CReportFunction *f;

	f = new ReportFunction_Add ( );
	m_functions. insert ( f-> name ( ), f );
	f = new ReportFunction_Sub ( );
	m_functions. insert ( f-> name ( ), f );
	f = new ReportFunction_Mul ( );
	m_functions. insert ( f-> name ( ), f );
	f = new ReportFunction_Div ( );
	m_functions. insert ( f-> name ( ), f );
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

		QStringList files = d. entryList ( "*.xml" );

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

const CReportFunctions &CReportManager::functions ( ) const
{
	return m_functions;
}

QPrinter *CReportManager::printer ( ) const
{
	if ( !m_printer )
		m_printer = new QPrinter ( QPrinter::HighResolution );
		
	return m_printer;
}


//------------------------------------------------------------------------
// ReportStyle
//------------------------------------------------------------------------

ReportStyle::ReportStyle ( ReportStyle *def )
{
	m_flags = RS_NONE;
	m_font_stylehint = QFont::AnyStyle;
	m_font_size = 10;
	m_font_italic = m_font_bold = m_font_underline = false;
	m_white_space = Qt::WordBreak;
	m_halign = Qt::AlignLeft;
	m_valign = Qt::AlignVCenter;

	m_default = def;
}

ReportStyle::~ReportStyle ( )
{ }

ReportStyle *ReportStyle::defaultStyle ( ) const
{
	return m_default;
}

void ReportStyle::setDefaultStyle ( ReportStyle *def )
{
	m_default = def;
}

QColor ReportStyle::color ( ) const
{
	QColor c;

	if ( m_default )
		c = m_default-> color ( );
	if ( m_flags & RS_COLOR )
		c = m_color;
	return c;
}

QColor ReportStyle::backgroundColor ( ) const
{
	QColor c;

	if ( m_default )
		c = m_default-> backgroundColor ( );
	if ( m_flags & RS_BGCOLOR )
		c = m_bgcolor;
	return c;
}

QFont ReportStyle::font ( ) const
{
	QFont f;

	if ( m_default )
		f = m_default-> font ( );

	if ( m_flags & RS_FONT_FAMILY )
		f. setFamily ( m_font_family );
	if ( m_flags & RS_FONT_STYLEHINT )
		f. setStyleHint ( m_font_stylehint );
	if ( m_flags & RS_FONT_SIZE )
		f. setPointSize ( m_font_size );
	if ( m_flags & RS_FONT_BOLD )
		f. setBold ( m_font_bold );
	if ( m_flags & RS_FONT_ITALIC )
		f. setItalic ( m_font_italic );
	if ( m_flags & RS_FONT_UNDERLINE )
		f. setUnderline ( m_font_underline );
		
	return f;
}

int ReportStyle::alignment ( ) const
{
	int al = 0;

	if ( m_default )
		al = m_default-> alignment ( );
	
	if ( m_flags & RS_WHITE_SPACE )
		al = ( al & ~( Qt::SingleLine | Qt::WordBreak )) | m_white_space;
	if ( m_flags & RS_HALIGN )
		al = ( al & ~Qt::AlignHorizontal_Mask ) | m_halign;
	if ( m_flags & RS_VALIGN )
		al = ( al & ~Qt::AlignVertical_Mask ) | m_valign;
		
	return al;
}


void ReportStyle::parseAttr ( const QDomNode &node )
{
	QDomNamedNodeMap map = node. attributes ( );

	for ( uint i = 0; i < map. count ( ); i++ ) {
		QDomAttr attr = map. item ( i ). toAttr ( );

		if ( attr. isNull ( ))
			continue;

		QString aname = attr. name ( ). stripWhiteSpace ( );
		QString avalue = attr. value ( ). stripWhiteSpace ( );
 
		if ( aname == "background-color" ) {
			m_bgcolor = QColor ( avalue );
			if ( m_bgcolor. isValid ( ))
				m_flags |= RS_BGCOLOR;
		}
		
		else if ( aname == "color" ) {
			m_color = QColor ( avalue );
			if ( m_color. isValid ( ))
				m_flags |= RS_COLOR;
		}
		
		else if ( aname == "font-family" ) {
			m_flags |= RS_FONT_STYLEHINT;
				
			if ( avalue == "sans-serif" )
				m_font_stylehint = QFont::SansSerif;
			else if ( avalue == "serif" )
				m_font_stylehint = QFont::Serif;
			else if ( avalue == "monospace" )
				m_font_stylehint = QFont::TypeWriter;
			else {
				m_font_family = avalue;
				m_flags |= RS_FONT_FAMILY;
				m_flags &= ~RS_FONT_STYLEHINT;
			}
		}
		else if ( aname == "font-size" ) {
			m_font_size = parseUnit ( avalue );
			m_flags |= RS_FONT_SIZE;
		}
		else if ( aname == "font-weight" ) {
			m_flags |= RS_FONT_BOLD;
				
			if ( avalue == "normal" )
				m_font_bold = false;
			else if ( avalue == "bold" )
				m_font_bold = true;
			else
				m_flags &= ~RS_FONT_BOLD;
		}
		else if ( aname == "font-style" ) {
			m_flags |= RS_FONT_ITALIC;
				
			if ( avalue == "normal" )
				m_font_italic = false;
			else if ( avalue == "italic" || avalue == "oblique" )
				m_font_italic = true;
			else
				m_flags &= ~RS_FONT_ITALIC;
		}
		else if ( aname == "text-decoration" ) {
			m_flags |= RS_FONT_UNDERLINE;
				
			if ( avalue == "none" )
				m_font_underline = false;
			else if ( avalue == "underline" )
				m_font_underline = true;
			else
				m_flags &= ~RS_FONT_UNDERLINE;
		}
		
		else if ( aname == "text-align" ) {
			m_flags |= RS_HALIGN;
			
			if ( avalue == "left" )
				m_halign = Qt::AlignLeft;
			else if ( avalue == "center" )
				m_halign = Qt::AlignHCenter;
			else if ( avalue == "right" )
				m_halign = Qt::AlignRight;
			else
				m_flags &= ~RS_HALIGN;
		}
		else if ( aname == "white-space" ) {
			m_flags |= RS_WHITE_SPACE;
			
			if ( avalue == "normal" )
				m_white_space = Qt::WordBreak;
			else if ( avalue == "nowrap" )
				m_white_space = Qt::SingleLine;
			else if ( avalue == "pre" )
				m_white_space = 0;
			else
				m_flags &= ~RS_WHITE_SPACE;
		}
		else if ( aname == "vertical-align" ) {
			m_flags |= RS_VALIGN;

			if ( avalue == "top" )
				m_valign = Qt::AlignTop;
			else if ( avalue == "middle" )
				m_valign = Qt::AlignVCenter;
			else if ( avalue == "bottom" )
				m_valign = Qt::AlignBottom;
			else
				m_flags &= ~RS_VALIGN;
		}
	}
}



//------------------------------------------------------------------------
// ReportPart
//------------------------------------------------------------------------

ReportPart::ReportPart ( )
{
	m_height = 0;
	setAutoDelete ( true );
}

ReportStyle *ReportPart::defaultStyle ( ) const
{
	return m_default;
}

void ReportPart::setDefaultStyle ( ReportStyle *def )
{
	m_default = def;
}

void ReportPart::setHeight ( int h )
{
	m_height = h;
}

int ReportPart::height ( ) const
{
	return m_height;
}

void ReportPart::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	for ( QPtrListIterator<ReportItem> it ( *this ); it. current ( ); ++it ) {
		ReportItem *ri = it. current ( );

		p-> save ( );
		ri-> setup ( p, vars );

		QRect ir = ri-> rect ( );
		ir. moveBy ( r. left ( ), r. top ( ));
		ir &= r;

		ri-> render ( p, ir, s, vars );
		p-> restore ( );
	}
}
bool ReportPart::parseItem ( const QDomElement &root, bool item_part )
{
	for ( QDomNode node = root. firstChild ( ); !node. isNull ( ); node = node. nextSibling ( )) {
		if ( !node. isElement ( ))
			continue;
		QDomElement e = node. toElement ( );
		
		ReportItem *ri = 0;

		if ( e. nodeName ( ) == "text" )
			ri = new ReportText ( );
		else if ( e. nodeName ( ) == "line" )
			ri = new ReportLine ( );
		else if ( e. nodeName ( ) == "image" )
			ri = new ReportImage ( );
		else if ( e. nodeName ( ) == "part-image" && item_part )
			ri = new ReportPartImage ( );
		else if ( e. nodeName ( ) == "color" && item_part )
			ri = new ReportColor ( e. text ( ));
		else if ( e. nodeName ( ) == "status" && item_part )
			ri = new ReportStatus ( );

		if ( ri ) {
			ri-> setDefaultStyle ( defaultStyle ( ));

			if ( ri-> parseItem ( e ))
				append ( ri );
			else
				delete ri;
		}
	}
	return true;
}



//------------------------------------------------------------------------
// Abstract ReportItem
//------------------------------------------------------------------------

ReportItem::ReportItem ( )
{ }

ReportItem::~ReportItem ( )
{ }

QRect ReportItem::rect ( ) const
{
	return m_rect;
}

void ReportItem::setup ( QPainter *p, CReportVariables &/*vars*/ )
{
	if ( !p )
		return;

	QColor bg = backgroundColor ( );
	if ( bg. isValid ( )) {
		p-> setBackgroundMode ( Qt::OpaqueMode );
		p-> setBackgroundColor ( bg );
	}
	else {
		p-> setBackgroundMode ( Qt::TransparentMode );
	}

	QColor fg = color ( );
	if ( fg. isValid ( ))
		p-> setPen ( fg );
	p-> setFont ( font ( ));
}

void ReportItem::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables & /*vars*/ )
{
	if ( !p )
		return;

	if ( p-> backgroundMode ( ) == Qt::OpaqueMode )
		p-> fillRect ( pt2px( r, s ), p-> backgroundColor ( ));
}

bool ReportItem::parseItem ( const QDomElement &node )
{
	parseAttr ( node );

	QDomNamedNodeMap map = node. attributes ( );
	
	int x = 0, y = 0, w = 0, h = 0;

	for ( uint i = 0; i < map. count ( ); i++ ) {
		QDomAttr attr = map. item ( i ). toAttr ( );
	
		if ( attr. isNull ( ))
			continue;
	
		QString aname = attr. name ( ). stripWhiteSpace ( );
		QString avalue = attr. value ( ). stripWhiteSpace ( );
	
		if ( aname == "left" )
			x = parseUnit ( avalue );
		else if ( aname == "top" )
			y = parseUnit ( avalue );
		else if ( aname == "width" )
			w = parseUnit ( avalue );
		else if ( aname == "height" )
			h = parseUnit ( avalue );
	}
	m_rect. setRect ( x, y, w, h );

	return true;
}



//------------------------------------------------------------------------
// Real ReportItems (Text, Line, Image, Color)
//------------------------------------------------------------------------

ReportLine::ReportLine ( )
{
	m_linewidth = 1;
	m_linestyle = Qt::SolidLine;
}

void ReportLine::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	ReportItem::render ( p, r, s, vars );
	
	p-> setPen ( QPen ( p-> pen ( ). color ( ), pt2px( m_linewidth, ( s [0] + s [1] ) / 2 ), m_linestyle ));

	QRect r2 = pt2px( r, s );
	p-> drawLine ( r2. topLeft ( ), r2. bottomRight ( ));
}

bool ReportLine::parseItem ( const QDomElement &node )
{
	ReportItem::parseItem ( node );

	QString lt = getAttr ( node, "linetype" );
	QString lw = getAttr ( node, "linewidth" );

	if ( !lw. isNull ( ))
		m_linewidth = parseUnit ( lw );
	if ( lt == "solid" )
		m_linestyle = Qt::SolidLine;
	else if ( lt == "dashed" )
		m_linestyle = Qt::DashLine;
	else if ( lt == "dotted" )
		m_linestyle = Qt::DotLine;

	return true;
}



ReportText::ReportText ( )
{ }


void ReportText::setText ( const QString &str )
{
	QDomText dt;
	dt. setData ( str );

	m_textnode = dt. cloneNode ( );
}


void ReportText::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	ReportItem::render ( p, r, s, vars );

	QString str = substitute ( m_textnode, vars );

	if ( !str. isEmpty ( ))
		p-> drawText ( pt2px( r, s ), alignment ( ), str );
}

bool ReportText::parseItem ( const QDomElement &node )
{
	ReportItem::parseItem ( node );

	m_textnode = node. cloneNode ( );

	return true;
}


ReportImage::ReportImage ( )
{
	m_aspect = false;
}

void ReportImage::setImage ( const QImage &img )
{
	m_image = img;
}

void ReportImage::setAspect ( bool a )
{
	m_aspect = a;
}

void ReportImage::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	ReportItem::render ( p, r, s, vars );

	if ( !m_image. isNull ( )) {
		QRect dr = pt2px( r, s );

		if ( m_aspect ) {
			QSize oldsize = dr. size ( );
			QSize newsize = m_image. size ( );
			newsize. scale ( oldsize, QSize::ScaleMin );

			dr. setSize ( newsize );
			dr. moveBy (( oldsize. width ( ) - newsize. width ( )) / 2,
			            ( oldsize. height ( ) - newsize. height ( )) / 2 );
		}
		p-> drawImage ( dr, m_image );
	}
}

bool ReportImage::parseItem ( const QDomElement &node )
{
	ReportItem::parseItem ( node );

	QString src = getAttr ( node, "src" );

	if ( !src. isNull ( )) {
		m_image = QImage ( src ); //TODO: May be should either switch dir or use QMimeSource here...
	}

	QString scale = getAttr ( node, "scale" );
	setAspect ( scale == "aspect" );

	return true;
}



ReportColor::ReportColor ( const QString &str )
	: ReportText ( ), m_type ( ColorText )
{ 
	setText ( str );
}


void ReportColor::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	QRect r2 = r;
	QColor color ( vars ["item-color-rgb"] );

	switch ( m_type ) {
		case ColorText:
			p-> setPen ( color );
			break;

		case TextInColorRect:
			p-> setPen ( qGray ( color. rgb ( )) < 128 ? Qt::white : Qt::black );
			p-> setBackgroundColor ( color );
			p-> setBackgroundMode ( Qt::OpaqueMode );
			break;

		case TextBesidesColorRect:
			if ( r. height ( ) < r. width ( )) {
				r2. setWidth ( r2. height ( ));
			
				p-> fillRect ( pt2px( r2, s ), color );
				
				r2 = r;
				r2. rLeft ( ) += r2. height ( );
				int dx = px2pt( p-> fontMetrics ( ). width ( "x" ), s [0] );
				if ( r2. width ( ) >= dx )
					r2. rLeft ( ) += dx;
			}
			break;
		
	}
	ReportText::render ( p, r2, s, vars );
}

bool ReportColor::parseItem ( const QDomElement &node )
{
	ReportText::parseItem ( node );

	QString t = getAttr ( node, "type" );

	if ( t == "color-text" )
		m_type = ColorText;
	else if ( t == "text-in-color-rect" )
		m_type = TextInColorRect;
	else if ( t == "text-besides-color-rect" )
		m_type = TextBesidesColorRect;

	return true;
}



ReportStatus::ReportStatus ( )
	: ReportImage ( )
{ }

void ReportStatus::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	QString st = vars ["item-status"];

	//TODO: setImage ( f ( st ));

	ReportImage::render ( p, r, s, vars );
}

bool ReportStatus::parseItem ( const QDomElement &node )
{
	// explicity NO ReportImage parsing here!
	bool res = ReportItem::parseItem ( node );

	setAspect ( true );
	return res;
}


ReportPartImage::ReportPartImage ( )
	: ReportImage ( )
{ }

void ReportPartImage::render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars )
{
	setImage ( QImage ( ));

	QString ms_name = vars ["item-image-mimesource"];

	const QMimeSource *ms = QMimeSourceFactory::defaultFactory ( )-> data ( ms_name );

	if ( ms && QImageDrag::canDecode ( ms )) {
		QImage img;

		if ( QImageDrag::decode ( ms, img ))
			setImage ( img );
	}

	ReportImage::render ( p, r, s, vars );
}

bool ReportPartImage::parseItem ( const QDomElement &node )
{
	// explicity NO ReportImage parsing here!
	bool res = ReportItem::parseItem ( node );

	setAspect ( true );
	return res;
}


