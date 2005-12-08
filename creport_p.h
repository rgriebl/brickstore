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
#ifndef __CREPORT_P_H__
#define __CREPORT_P_H__

class ReportStyle {
public:
	ReportStyle ( ReportStyle *def = 0 );
	virtual ~ReportStyle ( );

	ReportStyle *defaultStyle ( ) const;
	void setDefaultStyle ( ReportStyle *def );

	void parseAttr ( const QDomNode &node );
	
	virtual QColor color ( ) const;
	virtual QColor backgroundColor ( ) const;
	virtual QFont font ( ) const;
	virtual int alignment ( ) const;
	
protected:
	enum {
		RS_NONE           = 0x0000,
		
		RS_BGCOLOR        = 0x0010,
		RS_COLOR          = 0x0020,
		RS_FONT_FAMILY    = 0x0040,
		RS_FONT_STYLEHINT = 0x0080,
		RS_FONT_SIZE      = 0x0100,
		RS_FONT_BOLD      = 0x0200,
		RS_FONT_ITALIC    = 0x0400,
		RS_FONT_UNDERLINE = 0x0800,
		RS_WHITE_SPACE    = 0x1000,
		RS_HALIGN         = 0x2000,
		RS_VALIGN         = 0x4000
	};
	
	uint             m_flags;

	QColor           m_bgcolor;
	QColor           m_color;
	
	QString          m_font_family;
	QFont::StyleHint m_font_stylehint;
	int              m_font_size;
	bool             m_font_bold;
	bool             m_font_italic;
	bool             m_font_underline;
	
	int              m_white_space;
	int              m_halign;
	int              m_valign;

	ReportStyle *    m_default;
};

class ReportItem : public ReportStyle {
public:
	ReportItem ( );
	virtual ~ReportItem ( );

	QRect rect ( ) const;

	virtual void setup ( QPainter *p, CReportVariables &/*vars*/ );
	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables & /*vars*/ );
	virtual bool parseItem ( const QDomElement &node );

private:
	QRect         m_rect;
};

class ReportPart : public QPtrList<ReportItem> {
public:
	ReportPart ( );

	ReportStyle *defaultStyle ( ) const;
	void setDefaultStyle ( ReportStyle *def );

	void setHeight ( int h );
	int height ( ) const;
	
	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &root, bool item_part = false );
	
private:
	int m_height;
	ReportStyle *m_default;
};

class ReportLine : public ReportItem {
public:
	ReportLine ( );
	
	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );
	
protected:
	int          m_linewidth;
	Qt::PenStyle m_linestyle;
};

class ReportText : public ReportItem {
public:
	ReportText ( );

	void setText ( const QString &str );
	
	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );

protected:
	QDomNode m_textnode;
};

class ReportImage : public ReportItem {
public:
	ReportImage ( );

	void setImage ( const QImage &img );
	void setAspect ( bool a );
	
	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );
	
protected:
	QImage m_image;
	bool m_aspect;
};
	
class ReportColor : public ReportText {
public:
	ReportColor ( const QString &str = QString::null );

	enum Type { ColorText, TextInColorRect, TextBesidesColorRect };

	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );
	
private:
	Type    m_type;
};

class ReportStatus : public ReportImage {
public:
	ReportStatus ( );

	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );
};

class ReportPartImage : public ReportImage {
public:
	ReportPartImage ( );

	virtual void render ( QPainter *p, const QRect &r, const float s [2], CReportVariables &vars );
	virtual bool parseItem ( const QDomElement &node );
};



class ReportUIElement {
public:
	enum Type { Invalid, String, MultiString, Double, Int, Date };

	QString m_name;
	QString m_description;
	Type m_type;

	QDomElement m_default;

	ReportUIElement ( )
		: m_type ( Invalid )
	{ }
};


class CReportPrivate {
public:
	CReportPrivate ( );

	void parseAttr ( const QDomElement &node );
	void parseUI ( QDomElement e );

	bool                m_loaded;
	
	QString             m_name;
	QPrinter::PageSize  m_pagesize;

	int                 m_margin_left;
	int                 m_margin_top;
	int                 m_margin_right;
	int                 m_margin_bottom;
	
	ReportStyle         m_default_style;

	ReportPart          m_page_header;
	ReportPart          m_page_footer;
	ReportPart          m_report_header;
	ReportPart          m_report_footer;
	ReportPart          m_list_header;
	ReportPart          m_list_footer;
	
	enum { Even, Odd };
	ReportPart          m_item [2];

	QPtrList <ReportUIElement> m_ui;
};


class ReportFunction_Add : public CReportFunction {
public:
	virtual ~ReportFunction_Add ( )
	{ }
	
	virtual QString name ( ) const 
	{ return "+"; }

	virtual int argc ( ) const
	{ return -2; }

	virtual QString operator ( ) ( const QStringList &argv )
	{
		double v = 0.;
		for ( QStringList::const_iterator it = argv. begin ( ); it != argv. end ( ); ++it )
			v += (*it). toDouble ( );
		return QString::number ( v );
	}
};

class ReportFunction_Sub : public CReportFunction {
public:
	virtual ~ReportFunction_Sub ( )
	{ }
	
	virtual QString name ( ) const 
	{ return "-"; }

	virtual int argc ( ) const
	{ return +2; }

	virtual QString operator ( ) ( const QStringList &argv )
	{ return QString::number ( argv [0]. toDouble ( ) - argv [1]. toDouble ( )); }
};

class ReportFunction_Mul : public CReportFunction {
public:
	virtual ~ReportFunction_Mul ( )
	{ }
	
	virtual QString name ( ) const 
	{ return "*"; }

	virtual int argc ( ) const
	{ return -2; }

	virtual QString operator ( ) ( const QStringList &argv )
	{ 
		double v = 1.0;
		for ( QStringList::const_iterator it = argv. begin ( ); it != argv. end ( ); ++it )
			v *= (*it). toDouble ( );
		return QString::number ( v );
	}
};

class ReportFunction_Div : public CReportFunction {
public:
	virtual ~ReportFunction_Div ( )
	{ }
	
	virtual QString name ( ) const 
	{ return "/"; }

	virtual int argc ( ) const
	{ return +2; }

	virtual QString operator ( ) ( const QStringList &argv )
	{ return QString::number ( argv [0]. toDouble ( ) / argv [1]. toDouble ( )); }
};

#endif
