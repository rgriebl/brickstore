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
*//#ifndef __CREPORTOBJECTS_H__
//#define __CREPORTOBJECTS_H__

//#include <qobject.h>
//#include <qfont.h>
//#include <qcolor.h>
//#include <qvariant.h>
//#include <q3valuelist.h>
////Added by qt3to4:
//#include <Q3CString>
//#include <QPixmap>
//#include <Q3PtrList>

//class QPaintDevice;
//class QPixmap;
//class CReportPage;
//class QPainter;
//class QSInterpreter;


//class CReportUtility : public QObject {
//    Q_OBJECT
//    Q_OVERRIDE( Q3CString name         SCRIPTABLE false)

//public slots:
//    QString translate ( const QString &context, const QString &text ) const;

//    QString localDateString ( const QDateTime &dt ) const;
//    QString localTimeString ( const QDateTime &dt ) const;

//public:
//    CReportUtility ( );
//};


//class CReportMoneyStatic : public QObject {
//    Q_OBJECT
//    Q_OVERRIDE( Q3CString name SCRIPTABLE false)

//public:
//    CReportMoneyStatic ( QSInterpreter *ip );

//public slots:
//    double fromValue ( double d );
//    double fromLocalValue ( double d );

//    double value ( double d ) const;
//    double localValue ( double d ) const;

//    QString localCurrencySymbol ( ) const;

//    QString toString ( double d, bool with_currency_symbol = false, int precision = 3 );
//    QString toLocalString ( double d, bool with_currency_symbol = false, int precision = 3 );

//private:
//    QSInterpreter *m_ip;
//};


//class CReportJob : public QObject {
//    Q_OBJECT
//    Q_OVERRIDE( Q3CString name         SCRIPTABLE false)
	
//    Q_PROPERTY( uint     pageCount    READ pageCount)
////	Q_PROPERTY( int      paperFormat  READ paperFormat)
//    Q_PROPERTY( QSize    paperSize    READ paperSize)
//    Q_PROPERTY( double   scaling      READ scaling WRITE setScaling)
	
//public slots:
//    CReportPage *addPage ( );
//    CReportPage *getPage ( uint i ) const;
//    void abort ( );

//public:
//    uint pageCount ( ) const;
//    QSize paperSize ( ) const;
//    bool isAborted ( ) const;
//    double scaling ( ) const;
//    void setScaling ( double s );

//public:
//    CReportJob ( QPaintDevice *pd );
//    ~CReportJob ( );

//    QPaintDevice *paintDevice ( ) const;
	
//    bool print ( uint from, uint to );
//    void dump ( );

//private:
//    Q3ValueList <CReportPage *> m_pages;
//    QPaintDevice *m_pd;
//    bool m_aborted;
//    double m_scaling;
//};

//class CReportPage : public QObject {
//    Q_OBJECT
//    Q_OVERRIDE( Q3CString name SCRIPTABLE false)
	
//    Q_ENUMS( LineStyle )
//    Q_SETS( Alignment )
	
//    Q_PROPERTY( int     number           READ pageNumber)
//    Q_PROPERTY( QFont   font             READ font       WRITE setFont)
//    Q_PROPERTY( QColor  color            READ color      WRITE setColor)
//    Q_PROPERTY( QColor  backgroundColor  READ bgColor    WRITE setBgColor)
//    Q_PROPERTY( int     lineStyle        READ lineStyle  WRITE setLineStyle)
//    Q_PROPERTY( double  lineWidth        READ lineWidth  WRITE setLineWidth)
 
//public:
//    enum LineStyle {
//        NoLine,
//        SolidLine,
//        DashLine,
//        DotLine,
//        DashDotLine,
//        DashDotDotLine,
//    };

//    enum Alignment {
//        AlignLeft     = Qt::AlignLeft,
//        AlignHCenter  = Qt::AlignHCenter,
//        AlignRight    = Qt::AlignRight,
//        AlignTop      = Qt::AlignTop,
//        AlignVCenter  = Qt::AlignVCenter,
//        AlignBottom   = Qt::AlignBottom,
//        AlignCenter   = Qt::AlignCenter
//    };

//public slots:
//    QSize textSize ( const QString &text );
	
//    void drawText ( double x, double y, const QString &text);
//    void drawText ( double left, double top, double width, double height, Alignment align, const QString &text);
//    void drawLine ( double x1, double y1, double x2, double y2 );
//    void drawRect ( double left, double top, double width, double height );
//    void drawEllipse ( double left, double top, double width, double height );
//    void drawPixmap ( double left, double top, double width, double height, const QPixmap &pixmap );
	
//public:
//    int pageNumber ( ) const;
//    QFont font ( ) const;
//    QColor color ( ) const;
//    QColor bgColor ( ) const;
//    int lineStyle ( ) const;
//    double lineWidth ( ) const;

//    void setFont ( const QFont &f );
//    void setColor ( const QColor &c );
//    void setBgColor ( const QColor &c );
//    void setLineStyle ( int linestyle );
//    void setLineWidth ( double linewidth );
	
//public:
//    CReportPage ( const CReportJob *job );
 
//    void dump ( );
//    void print ( QPainter *p, double scale [2] );

//private:
//    struct Cmd {
//        enum {
//            Attributes,
//            Text,
//            Line,
//            Rect,
//            Ellipse,
//            Pixmap
//        } m_cmd;
//    };
	
//    struct AttrCmd : public Cmd {
//        QFont  m_font;
//        QColor m_color;
//        QColor m_bgcolor;
//        int    m_linestyle;
//        double  m_linewidth;
//    };
	
//    struct DrawCmd : public Cmd {
//        double    m_x, m_y, m_w, m_h;
//        QVariant  m_p1;
//        QVariant  m_p2;
//    };
	
//    void attr_cmd ( );
	
//private:
//    Q3PtrList <Cmd> m_cmds;
//    const CReportJob *m_job;
//    AttrCmd m_attr;
//};

//#endif

