#ifndef __CREPORTOBJECTS_H__
#define __CREPORTOBJECTS_H__

#include <qobject.h>
#include <qfont.h>
#include <qcolor.h>
#include <qvariant.h>
#include <qvaluelist.h>

class QPaintDevice;
class QPixmap;
class CReportPage;
class QPainter;


class CReportUtility : public QObject {
	Q_OBJECT
	Q_OVERRIDE( QCString name         SCRIPTABLE false)

public slots:
	QString translate ( const QString &context, const QString &text ) const;

	QMap<QString, QVariant> moneyFromDollar ( double d ) const;
	QMap<QString, QVariant> moneyFromLocal ( double d ) const;

	QString localDateString ( const QDateTime &dt ) const;
	QString localTimeString ( const QDateTime &dt ) const;

public:
	CReportUtility ( );
};

class CReportJob : public QObject {
	Q_OBJECT
	Q_OVERRIDE( QCString name         SCRIPTABLE false)
	
	Q_PROPERTY( uint     pageCount    READ pageCount)
//	Q_PROPERTY( int      paperFormat  READ paperFormat)
	Q_PROPERTY( QSize    paperSize    READ paperSize)
	Q_PROPERTY( double   scaling      READ scaling WRITE setScaling)
	
public slots:
	CReportPage *addPage ( );
	CReportPage *getPage ( uint i ) const;
	void abort ( );

public:
	uint pageCount ( ) const;
	QSize paperSize ( ) const;
	bool isAborted ( ) const;
	double scaling ( ) const;
	void setScaling ( double s );

public:
	CReportJob ( QPaintDevice *pd );
	~CReportJob ( );

	QPaintDevice *paintDevice ( ) const;
	
	bool print ( uint from, uint to );
	void dump ( );

private:
	QValueList <CReportPage *> m_pages;
	QPaintDevice *m_pd;
	bool m_aborted;
	double m_scaling;
};

class CReportPage : public QObject {
	Q_OBJECT 
	Q_OVERRIDE( QCString name SCRIPTABLE false)
	
	Q_ENUMS( LineStyle )
	Q_SETS( Alignment )
	
	Q_PROPERTY( int     number           READ pageNumber)
	Q_PROPERTY( QFont   font             READ font       WRITE setFont)
	Q_PROPERTY( QColor  color            READ color      WRITE setColor)
	Q_PROPERTY( QColor  backgroundColor  READ bgColor    WRITE setBgColor)
	Q_PROPERTY( int     lineStyle        READ lineStyle  WRITE setLineStyle)
	Q_PROPERTY( double  lineWidth        READ lineWidth  WRITE setLineWidth)
 
public:
	enum LineStyle {
		NoLine,
		SolidLine,
		DashLine,
		DotLine,
		DashDotLine,
		DashDotDotLine,
	};

	enum Alignment {
		AlignLeft     = Qt::AlignLeft,
		AlignHCenter  = Qt::AlignHCenter,
		AlignRight    = Qt::AlignRight,
		AlignTop      = Qt::AlignTop,
		AlignVCenter  = Qt::AlignVCenter,
		AlignBottom   = Qt::AlignBottom,
		AlignCenter   = Qt::AlignCenter
	};

public slots:
	QSize textSize ( const QString &text );
	
	void drawText ( double x, double y, const QString &text);
	void drawText ( double left, double top, double width, double height, Alignment align, const QString &text);
	void drawLine ( double x1, double y1, double x2, double y2 );
	void drawRect ( double left, double top, double width, double height );
	void drawEllipse ( double left, double top, double width, double height );
	void drawPixmap ( double left, double top, double width, double height, const QPixmap &pixmap );
	
public:
	int pageNumber ( ) const;
	QFont font ( ) const;
	QColor color ( ) const;
	QColor bgColor ( ) const;
	int lineStyle ( ) const;
	double lineWidth ( ) const;

	void setFont ( const QFont &f );
	void setColor ( const QColor &c );
	void setBgColor ( const QColor &c );
	void setLineStyle ( int linestyle );
	void setLineWidth ( double linewidth );
	
public:
	CReportPage ( const CReportJob *job );
 
	void dump ( );
	void print ( QPainter *p, double scale [2] );

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
		double  m_linewidth;
	};
	
	struct DrawCmd : public Cmd {
		double    m_x, m_y, m_w, m_h;
		QVariant  m_p1;
		QVariant  m_p2;
	};
	
	void attr_cmd ( );
	
private:
	QPtrList <Cmd> m_cmds;
	const CReportJob *m_job;
	AttrCmd m_attr;
};

#endif

