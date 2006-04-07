#ifndef __REPORT_OBJECTS_H__
#define __REPORT_OBJECTS_H__

#include <qobject.h>
#include <qfont.h>
#include <qcolor.h>
#include <qvariant.h>
#include <qvaluelist.h>

class QPaintDevice;
class QPixmap;
class ReportPage;


class ReportJob : public QObject {
  Q_OBJECT
  Q_OVERRIDE( QCString name         SCRIPTABLE false)
  
  Q_PROPERTY( uint     pageCount    READ pageCount)
//  Q_PROPERTY( int      paperFormat  READ paperFormat)
  Q_PROPERTY( QSize    paperSize    READ paperSize)
  
public slots:
  ReportPage *addPage ( );
  ReportPage *getPage ( uint i ) const;
  void abort ( );

public:
  uint pageCount ( ) const;
  QSize paperSize ( ) const;

public:
  ReportJob ( QPaintDevice *pd );
  
  QPaintDevice *paintDevice ( ) const;
  
private:
  QValueList <ReportPage *> m_pages;
  QPaintDevice *m_pd;
  bool m_aborted;
};

class ReportPage : public QObject {
  Q_OBJECT 
  Q_OVERRIDE( QCString name SCRIPTABLE false)
  
  Q_ENUMS( LineStyle )
  
  Q_PROPERTY( QFont   font             READ font       WRITE setFont)
  Q_PROPERTY( QColor  color            READ color      WRITE setColor)
  Q_PROPERTY( QColor  backgroundColor  READ bgColor    WRITE setBgColor)
  Q_PROPERTY( int     lineStyle        READ lineStyle  WRITE setLineStyle)
  Q_PROPERTY( uint    lineWidth        READ lineWidth  WRITE setLineWidth)
 
public:
  enum LineStyle {
    NoLine,
    SolidLine,
    DashLine,
    DotLine,
    DashDotLine,
    DashDotDotLine,
  };
 
public slots:
  QSize textSize ( const QString &text );
  
  void drawText ( int x, int y, const QString &text);
  void drawText ( int left, int top, int width, int height, int align, const QString &text);
  void drawLine ( int x1, int y1, int x2, int y2 );
  void drawRect ( int left, int top, int width, int height );
  void fillRect ( int left, int top, int width, int height, const QColor &fillcolor );
  void drawEllipse ( int left, int top, int width, int height );
  void fillEllipse ( int left, int top, int width, int height, const QColor &fillcolor );
  void drawPixmap ( int left, int top, int width, int height, const QPixmap &pixmap );  
  
public:
  QFont font ( ) const;
  QColor color ( ) const;
  QColor bgColor ( ) const;
  int lineStyle ( ) const;
  uint lineWidth ( ) const;

  void setFont ( const QFont &f );
  void setColor ( const QColor &c );
  void setBgColor ( const QColor &c );
  void setLineStyle ( int linestyle );
  void setLineWidth ( uint linewidth );
  
public:
  ReportPage ( const ReportJob *job );
 
private: 
  struct Cmd {
    enum {
      Attributes,
      Text,
      Line,
      Rect,
      Ellipse,
      Pixmap
    } m_cmd;    
  };
  
  struct AttrCmd : public Cmd {
    QFont  m_font;
    QColor m_color;
    QColor m_bgcolor;
    int    m_linestyle;
    uint   m_linewidth;
  };
  
  struct DrawCmd : public Cmd {
    int m_x, m_y, m_w, m_h;
    QVariant m_p1;
    QVariant m_p2;
  };
  
  void attr_cmd ( );
  
private:
  QValueList <Cmd> m_cmds;
  const ReportJob *m_job;
  AttrCmd m_attr;
};

#endif

