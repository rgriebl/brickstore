#include <qpixmap.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>
#include <qfontmetrics.h>

#include "reportobjects.h"

ReportJob::ReportJob ( QPaintDevice *pd )
  : QObject ( 0, "ReportJob" ), m_pd ( pd ), m_aborted ( false )
{
}

QPaintDevice *ReportJob::paintDevice ( ) const
{
  return m_pd;
}

ReportPage *ReportJob::addPage ( )
{
  ReportPage *page = new ReportPage ( this );
  m_pages. append ( page );
  return page;
}

ReportPage *ReportJob::getPage ( uint i ) const
{
  if ( i < m_pages. count ( ))
    return m_pages [i];
  else
    return 0;
}

void ReportJob::abort ( )
{
  m_aborted = true;
}

uint ReportJob::pageCount ( ) const
{
  return m_pages. count ( );
}

QSize ReportJob::paperSize ( ) const
{
  QPaintDeviceMetrics pdm ( m_pd );
  return QSize ( pdm. widthMM ( ), pdm. heightMM ( ));
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

ReportPage::ReportPage ( const ReportJob *job )
  : QObject ( const_cast <ReportJob *> ( job )), m_job ( job )
{
  setName ( QString( "Page%1" ). arg( job-> pageCount ( ) + 1 ). ascii ( ));

  m_attr. m_font = QFont ( "Arial", 12 );
  m_attr. m_color = Qt::black;
  m_attr. m_bgcolor = Qt::white;
  m_attr. m_linewidth = 100; // 0.1mm
  m_attr. m_linestyle = SolidLine;
}

QFont ReportPage::font ( ) const
{
  return m_attr. m_font;
}

QColor ReportPage::color ( ) const
{
  return m_attr. m_color;
}

QColor ReportPage::bgColor ( ) const
{
  return m_attr. m_bgcolor;
}

int ReportPage::lineStyle ( ) const
{
  return m_attr. m_linestyle;
}

uint ReportPage::lineWidth ( ) const
{
  return m_attr. m_linewidth;
}

void ReportPage::attr_cmd ( )
{  
  AttrCmd ac = m_attr;
  ac. m_cmd = Cmd::Attributes;
  m_cmds. append ( ac );
}

void ReportPage::setFont ( const QFont &f )
{
  m_attr. m_font = f;
  attr_cmd ( );
}
  
void ReportPage::setColor ( const QColor &c )
{
  m_attr. m_color = c;
  attr_cmd ( );
}

void ReportPage::setBgColor ( const QColor &c )
{
  m_attr. m_bgcolor = c;
  attr_cmd ( );
}

void ReportPage::setLineStyle ( int linestyle )
{
  m_attr. m_linestyle = linestyle;
  attr_cmd ( );
}

void ReportPage::setLineWidth ( uint linewidth )
{
  m_attr. m_linewidth = linewidth;
  attr_cmd ( );
}


QSize ReportPage::textSize ( const QString &text )
{
  QFontMetrics fm ( m_attr. m_font );
  QPaintDeviceMetrics pdm ( m_job-> paintDevice ( ));
  QSize s = fm. size ( 0, text );
  return QSize ( s. width ( ) * pdm. widthMM ( ) / pdm. width ( ),
                 s. height ( ) * pdm. heightMM ( ) / pdm. height ( ));
}  

void ReportPage::drawText ( int x, int y, const QString &text)
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Text;
  dc. m_x = x;
  dc. m_y = y;
  dc. m_w = dc. m_h = -1;
  dc. m_p2 = text;
  m_cmds. append ( dc );
}

void ReportPage::drawText ( int left, int top, int width, int height, int align, const QString &text)
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Text;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  dc. m_p1 = align;
  dc. m_p2 = text;
  m_cmds. append ( dc );
}

void ReportPage::drawLine ( int x1, int y1, int x2, int y2 )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Line;
  dc. m_x = x1;
  dc. m_y = y1;
  dc. m_w = x2;
  dc. m_h = y2;
  m_cmds. append ( dc );
}

void ReportPage::drawRect ( int left, int top, int width, int height )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Rect;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  m_cmds. append ( dc );
}

void ReportPage::fillRect ( int left, int top, int width, int height, const QColor &fillcolor )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Rect;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  dc. m_p1 = fillcolor;
  m_cmds. append ( dc );
}

void ReportPage::drawEllipse ( int left, int top, int width, int height )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Ellipse;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  m_cmds. append ( dc );
}

void ReportPage::fillEllipse ( int left, int top, int width, int height, const QColor &fillcolor )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Ellipse;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  dc. m_p1 = fillcolor;
  m_cmds. append ( dc );
}

void ReportPage::drawPixmap ( int left, int top, int width, int height, const QPixmap &pixmap )
{
  DrawCmd dc;
  dc. m_cmd = Cmd::Pixmap;
  dc. m_x = left;
  dc. m_y = top;
  dc. m_w = width;
  dc. m_h = height;
  dc. m_p1 = pixmap;
  m_cmds. append ( dc );
}



///////////////////////////////////////////////////////////////////////////////////////////////
