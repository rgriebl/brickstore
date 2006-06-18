#include <qapplication.h>
#include <qpixmap.h>
#include <qprinter.h>
#include <qpaintdevicemetrics.h>
#include <qfontmetrics.h>
#include <qpainter.h>
#include <qdatetime.h>

#include "cmoney.h"
#include "cutility.h"

#include "creportobjects.h"


CReportUtility::CReportUtility ( )
	: QObject ( 0, "Utility" )
{ }

QString CReportUtility::translate ( const QString &context, const QString &text ) const
{
	return qApp-> translate ( context. latin1 ( ), text. latin1 ( ));
}

QMap<QString, QVariant> CReportUtility::moneyFromDollar ( double d ) const
{
	money_t m ( d );
	return m. toScriptObject ( );
}

QMap<QString, QVariant> CReportUtility::moneyFromLocal ( double d ) const
{
	money_t m ( d );
	m /= CMoney::inst ( )-> factor ( );
	return m. toScriptObject ( );
}

QString CReportUtility::localDateString ( const QDateTime &dt ) const
{
	return dt. date ( ). toString ( Qt::LocalDate );
}

QString CReportUtility::localTimeString ( const QDateTime &dt ) const
{
	return dt. time ( ). toString ( Qt::LocalDate );
}


CReportJob::CReportJob ( QPaintDevice *pd )
	: QObject ( 0, "Job" ), m_pd ( pd ), m_aborted ( false ), m_scaling ( 1.0f )
{
}

CReportJob::~CReportJob ( )
{
	qDeleteAll ( m_pages );
}

QPaintDevice *CReportJob::paintDevice ( ) const
{
	return m_pd;
}

CReportPage *CReportJob::addPage ( )
{
	CReportPage *page = new CReportPage ( this );
	m_pages. append ( page );
	return page;
}

CReportPage *CReportJob::getPage ( uint i ) const
{
	if ( i < m_pages. count ( ))
		return m_pages [i];
	else
		return 0;
}

void CReportJob::abort ( )
{
	m_aborted = true;
}

bool CReportJob::isAborted ( ) const
{
	return m_aborted;
}

double CReportJob::scaling ( ) const
{
	return m_scaling;
}

void CReportJob::setScaling ( double s )
{
	m_scaling = s;
}

uint CReportJob::pageCount ( ) const
{
	return m_pages. count ( );
}

QSize CReportJob::paperSize ( ) const
{
	QPaintDeviceMetrics pdm ( m_pd );
	return QSize ( pdm. widthMM ( ), pdm. heightMM ( ));
}

void CReportJob::dump ( )
{
	qDebug( "Print Job Dump" );
	qDebug( " # of pages: %d", int( m_pages. count ( )));
	qDebug( " " );

	for ( uint i = 0; i < m_pages. count ( ); i++ ) {
		qDebug ( "Page #%d", i );
		m_pages [i]-> dump ( );
	}
}

bool CReportJob::print ( uint from, uint to )
{
	if ( !m_pages. count ( ) || ( from > to ) || ( to >= m_pages. count ( )))
		return false;

	QPainter p;
	if ( !p. begin ( m_pd ))
		return false;

	QPrinter *prt = (( m_pd-> devType ( ) & QInternal::DeviceTypeMask ) == QInternal::Printer ) ? static_cast<QPrinter *> ( m_pd ) : 0;

	QPaintDeviceMetrics pdm ( m_pd );
	double scaling [2];
	scaling [0] = m_scaling * double( pdm. logicalDpiX ( )) / 25.4f;
	scaling [1] = m_scaling * double( pdm. logicalDpiY ( )) / 25.4f;
	bool no_new_page = true;

	for ( uint i = from; i <= to; i++ ) {
		CReportPage *page = m_pages [i];

		if ( !no_new_page && prt )
			prt-> newPage ( );

		page-> print ( &p, scaling );
		no_new_page = false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

CReportPage::CReportPage ( const CReportJob *job )
	: QObject ( const_cast <CReportJob *> ( job )), m_job ( job )
{
	setName ( QString( "Page%1" ). arg( job-> pageCount ( ) + 1 ). ascii ( ));

	m_attr. m_font = QFont ( "Arial", 10 );
	m_attr. m_color = Qt::black;
	m_attr. m_bgcolor = Qt::white;
	m_attr. m_linewidth = 0.1;
	m_attr. m_linestyle = SolidLine;
}

int CReportPage::pageNumber ( ) const
{
	int res = -1;

	for ( uint i = 0; i < m_job-> pageCount ( ); i++ ) {
		if ( m_job-> getPage ( i ) == this ) {
			res = int( i );
			break;
		}
	}
	return res;
}

void CReportPage::dump ( )
{
	qDebug( " # of commands: %d", int( m_cmds. count ( )));
	qDebug( " " );

	for ( uint i = 0; i < m_cmds. count ( ); i++ ) {
		switch ( m_cmds. at ( i )-> m_cmd ) {
			case Cmd::Attributes: {
				AttrCmd *ac = static_cast<AttrCmd *> ( m_cmds. at ( i ));

				qDebug ( " [%d] Attributes (Font: %s | Color: %s | BgColor: %s | Line: %f | LineStyle: %d", i, ac-> m_font. toString ( ). latin1 ( ), ac-> m_color. name ( ). latin1 ( ), ac-> m_bgcolor. name ( ). latin1 ( ), ac-> m_linewidth, ac-> m_linestyle );
				break;
			}
			case Cmd::Text:  {
				DrawCmd *dc = static_cast<DrawCmd *> ( m_cmds. at ( i ));

				if ( dc-> m_w == -1 && dc-> m_h == -1 )
					qDebug ( " [%d] Text (%f,%f), \"%s\"", i, dc-> m_x, dc-> m_y, dc-> m_p2. toString ( ). latin1 ( ));
				else
					qDebug ( " [%d] Text (%f,%f - %fx%f), align: %d, \"%s\"", i, dc-> m_x, dc-> m_y, dc-> m_w, dc-> m_h, dc-> m_p1. toInt ( ), dc-> m_p2. toString ( ). latin1 ( ));

				break;
			}
			case Cmd::Line: {
				DrawCmd *dc = static_cast<DrawCmd *> ( m_cmds. at ( i ));

				qDebug ( " [%d] Line (%f,%f - %f,%f)", i, dc-> m_x, dc-> m_y, dc-> m_w, dc-> m_h );
				break;
			}
			case Cmd::Rect: {
				DrawCmd *dc = static_cast<DrawCmd *> ( m_cmds. at ( i ));

				qDebug ( " [%d] Rectangle (%f,%f - %fx%f)", i, dc-> m_x, dc-> m_y, dc-> m_w, dc-> m_h );
				break;
			}
			case Cmd::Ellipse: {
				DrawCmd *dc = static_cast<DrawCmd *> ( m_cmds. at ( i ));

				qDebug ( " [%d] Ellipse (%f,%f - %fx%f)", i, dc-> m_x, dc-> m_y, dc-> m_w, dc-> m_h );
				break;
			}
			case Cmd::Pixmap: {
				DrawCmd *dc = static_cast<DrawCmd *> ( m_cmds. at ( i ));

				qDebug ( " [%d] Pixmap (%f,%f - %fx%f)", i, dc-> m_x, dc-> m_y, dc-> m_w, dc-> m_h );
				break;
			}
		}
	}
}

void CReportPage::print ( QPainter *p, double scale [2] )
{
	for ( QPtrListIterator<Cmd> it ( m_cmds ); it. current ( ); ++it ) {
		if ( it. current ( )-> m_cmd == Cmd::Attributes ) {
			AttrCmd *ac = static_cast<AttrCmd *> ( it. current ( ));

		//	QFont f = ac-> m_font;
		//	f. setPointSizeFloat ( f. pointSizeFloat ( ) * scale [1] );
			p-> setFont ( ac-> m_font );

			if ( ac-> m_color. isValid ( ))
				p-> setPen ( QPen ( ac-> m_color, int( ac-> m_linewidth * ( scale [0] + scale [1] ) / 2.f ), Qt::PenStyle( ac-> m_linestyle )));
			else
				p-> setPen ( QPen ( Qt::NoPen ));
			
			if ( ac-> m_bgcolor. isValid ( ))
				p-> setBrush ( QBrush ( ac-> m_bgcolor ));
			else
				p-> setBrush ( QBrush ( Qt::NoBrush ));
		}
		else {
			DrawCmd *dc = static_cast<DrawCmd *> ( it. current ( ));

			int x = int( dc-> m_x * scale [0] );
			int y = int( dc-> m_y * scale [1] );
			int w = int( dc-> m_w * scale [0] );
			int h = int( dc-> m_h * scale [1] );

			switch ( it. current ( )-> m_cmd ) {
				case Cmd::Text:
					if ( w < 0 && h < 0 )
						p-> drawText ( x, y, dc-> m_p2. toString ( ));
					else
						p-> drawText ( x, y, w, h, dc-> m_p1. toInt ( ), dc-> m_p2. toString ( ));
					break;

				case Cmd::Line:
					p-> drawLine ( x, y, w, h );
					break;

				case Cmd::Rect:
					p-> drawRect ( x, y, w, h );
					break;

				case Cmd::Ellipse:
					p-> drawEllipse ( x, y, w, h );
					break;

				case Cmd::Pixmap: {
					QPixmap pix = dc-> m_p1. toPixmap ( );
					
					if ( !pix. isNull ( )) {
						QRect dr = QRect ( x, y, w, h );

						QSize oldsize = dr. size ( );
						QSize newsize = pix. size ( );
						newsize. scale ( oldsize, QSize::ScaleMin );

						dr. setSize ( newsize );
						dr. moveBy (( oldsize. width ( ) - newsize. width ( )) / 2,
						            ( oldsize. height ( ) - newsize. height ( )) / 2 );

						p-> drawPixmap ( dr, pix );
					}
					break;
				}

				case Cmd::Attributes:
					break;
			}
		}
	}
}


QFont CReportPage::font ( ) const
{
	return m_attr. m_font;
}

QColor CReportPage::color ( ) const
{
	return m_attr. m_color;
}

QColor CReportPage::bgColor ( ) const
{
	return m_attr. m_bgcolor;
}

int CReportPage::lineStyle ( ) const
{
	return m_attr. m_linestyle;
}

double CReportPage::lineWidth ( ) const
{
	return m_attr. m_linewidth;
}

void CReportPage::attr_cmd ( )
{  
	AttrCmd *ac = new AttrCmd ( );
	*ac = m_attr;
	ac-> m_cmd = Cmd::Attributes;
	m_cmds. append ( ac );
}

void CReportPage::setFont ( const QFont &f )
{
	m_attr. m_font = f;
	attr_cmd ( );
}
	
void CReportPage::setColor ( const QColor &c )
{
	m_attr. m_color = c;
	attr_cmd ( );
}

void CReportPage::setBgColor ( const QColor &c )
{
	m_attr. m_bgcolor = c;
	attr_cmd ( );
}

void CReportPage::setLineStyle ( int linestyle )
{
	m_attr. m_linestyle = linestyle;
	attr_cmd ( );
}

void CReportPage::setLineWidth ( double linewidth )
{
	m_attr. m_linewidth = linewidth;
	attr_cmd ( );
}


QSize CReportPage::textSize ( const QString &text )
{
	QFontMetrics fm ( m_attr. m_font );
	QPaintDeviceMetrics pdm ( m_job-> paintDevice ( ));
	QSize s = fm. size ( 0, text );
	return QSize ( s. width ( ) * pdm. widthMM ( ) / pdm. width ( ),
	               s. height ( ) * pdm. heightMM ( ) / pdm. height ( ));
}  

void CReportPage::drawText ( double x, double y, const QString &text)
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Text;
	dc-> m_x = x;
	dc-> m_y = y;
	dc-> m_w = dc-> m_h = -1;
	dc-> m_p2 = text;
	m_cmds. append ( dc );
}

void CReportPage::drawText ( double left, double top, double width, double height, Alignment align, const QString &text)
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Text;
	dc-> m_x = left;
	dc-> m_y = top;
	dc-> m_w = width;
	dc-> m_h = height;
	dc-> m_p1 = align;
	dc-> m_p2 = text;
	m_cmds. append ( dc );
}

void CReportPage::drawLine ( double x1, double y1, double x2, double y2 )
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Line;
	dc-> m_x = x1;
	dc-> m_y = y1;
	dc-> m_w = x2;
	dc-> m_h = y2;
	m_cmds. append ( dc );
}

void CReportPage::drawRect ( double left, double top, double width, double height )
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Rect;
	dc-> m_x = left;
	dc-> m_y = top;
	dc-> m_w = width;
	dc-> m_h = height;
	m_cmds. append ( dc );
}

void CReportPage::drawEllipse ( double left, double top, double width, double height )
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Ellipse;
	dc-> m_x = left;
	dc-> m_y = top;
	dc-> m_w = width;
	dc-> m_h = height;
	m_cmds. append ( dc );
}

void CReportPage::drawPixmap ( double left, double top, double width, double height, const QPixmap &pixmap )
{
	DrawCmd *dc = new DrawCmd ( );
	dc-> m_cmd = Cmd::Pixmap;
	dc-> m_x = left;
	dc-> m_y = top;
	dc-> m_w = width;
	dc-> m_h = height;
	dc-> m_p1 = pixmap;
	m_cmds. append ( dc );
}



///////////////////////////////////////////////////////////////////////////////////////////////
