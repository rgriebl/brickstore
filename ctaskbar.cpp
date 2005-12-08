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
#include <qiconset.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qpixmapcache.h>
#include <qstyle.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qfont.h>
#include <qcursor.h>


#include "ctaskbar.h"
#include "cutility.h"

class CTaskBarPrivate {
public:
	QPixmap            m_lgrad;
	QPixmap            m_rgrad;
	QColor             m_mcol;

	QRect m_margins;
	QFont m_font;
	int m_xh, m_xw;
	int m_vspace, m_hspace;
	int m_arrow;
	bool m_condensed : 1;
	QPtrList <CTaskGroup> *m_groups;
};


// vvv PRIVATE vvv

class CTaskGroup : public QWidget {
private:
	friend class CTaskBar;

	CTaskGroup ( CTaskBar *parent )
		: QWidget ( parent )
	{
		m_bar = parent;
		m_expanded = true;
		m_expandible = true;
		m_special = m_hot = false;
		m_widget = 0;

		setBackgroundOrigin ( QWidget::AncestorOrigin );
		setSizePolicy ( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
		setCursor ( Qt::PointingHandCursor );
	}

public:
	void setWidget ( QWidget *w )
	{
		m_widget = w;
		w-> reparent ( m_bar, 0, QPoint ( 0, 0 ), false );
		m_bar-> recalcLayout ( );
	}

	QWidget *widget ( ) const
	{
		return m_widget;
	}

	void setText ( const QString &txt )
	{
		if ( txt != m_text ) {
			m_text = txt;
			m_bar-> recalcLayout ( );
			update ( );
		}
	}

	QString text ( ) const
	{
		return m_text;
	}

	void setIconSet ( const QIconSet &is )
	{
		m_is = is;
		m_bar-> recalcLayout ( );
		update ( );
	}

	QIconSet iconSet ( ) const
	{
		return m_is;
	}

	void setSpecial ( bool b )
	{
		if ( m_special != b ) {
			m_special = b;
			m_bar-> recalcLayout ( );
			update ( );
		}
	}

	bool isSpecial ( ) const 
	{
		return m_special;
	}

	void setExpandible ( bool b )
	{
		if ( m_expandible != b ) {
			m_expandible = b;
			m_bar-> recalcLayout ( );
			update ( );
		}
	}

	bool isExpandible ( ) const
	{
		return m_expandible;
	}

	void setExpanded ( bool b ) 
	{
		if ( m_expanded != b ) {
			m_expanded = b;
			m_bar-> recalcLayout ( );
			update ( );
			m_bar-> update ( );
		}
	}

	bool isExpanded ( ) const
	{
		return m_expanded;
	}

	void paintEvent ( QPaintEvent * /*e*/ )
	{
		QPainter p ( this );

		m_bar-> drawHeaderBackground ( &p, this, m_hot );
		m_bar-> drawHeaderContents ( &p, this, m_hot );
	}

	virtual QSize sizeHint ( ) const
	{
		return m_bar-> sizeForGroup ( this );
	}

protected:
	virtual void enterEvent ( QEvent * )
	{
		m_hot = true;
		update ( );
	}

	virtual void leaveEvent ( QEvent * )
	{
		m_hot = false;
		update ( );
	}

	void mouseReleaseEvent ( QMouseEvent * /*e*/ )
	{
		setExpanded ( !isExpanded ( ));
	}

protected:
	bool m_expandible  : 1;
	bool m_expanded    : 1;
	bool m_special     : 1;
	bool m_hot         : 1;

	int m_leadin;
	int m_leadout;

	QWidget *m_widget;
	QIconSet m_is;
	QString m_text;

	CTaskBar *m_bar;
};



//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################
//###########################################################################################


CTaskBar::CTaskBar ( QWidget *parent, const char *name )
	: QWidget ( parent, name )
{
	d = new CTaskBarPrivate ( );

	d-> m_margins = QRect ( 6, 6, 1, 1 );
	d-> m_condensed = false;
	d-> m_groups = new QPtrList <CTaskGroup>;
	d-> m_groups-> setAutoDelete ( true );

	fontChange ( font ( ));
	paletteChange ( palette ( ));

	setWFlags ( Qt::WNoAutoErase );
	setBackgroundMode ( Qt::NoBackground );
	setSizePolicy ( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
}

CTaskBar::~CTaskBar ( )
{
	delete d;
}

uint CTaskBar::addItem ( QWidget *w, const QIconSet &is, const QString &txt, bool expandible, bool special )
{
	CTaskGroup *g = new CTaskGroup ( this );
	g-> setIconSet ( is );
	g-> setText ( txt );
	g-> setSpecial ( special );
	g-> setExpandible ( expandible );
	g-> setWidget ( w );
	g-> show ( );

	d-> m_groups-> append ( g );
	recalcLayout ( );

	return d-> m_groups-> count ( ) - 1;
}

void CTaskBar::removeItem ( uint i, bool delete_widget )
{
	if ( i < d-> m_groups-> count ( )) {
		QWidget *w = d-> m_groups-> at ( i )-> widget ( );
		d-> m_groups-> remove ( i );

		if ( delete_widget )
			delete w;
	}
}

void CTaskBar::drawHeaderBackground ( QPainter *p, CTaskGroup *g, bool /*hot*/ )
{
	const QColorGroup &cg = palette ( ). active ( );
	QRect r = g-> rect ( );

	int offy = r. height ( ) - ( d-> m_vspace + d-> m_xh + d-> m_vspace );

	if ( offy > 0 )
		p-> fillRect ( r. x ( ), r. y ( ), r. width ( ), offy, d-> m_mcol );

	if ( g-> isSpecial ( )) {
		p-> fillRect ( r. x ( ), r. y ( ) + offy, r. width ( ), r. height ( ) - offy, cg. highlight ( ). dark ( 150 ));
	}
	else {
		QPixmap gradient;
		QString key;

		key. sprintf ( "ctg#%d", r. width ( ));

		if ( !QPixmapCache::find ( key, gradient )) {
			gradient. resize ( r. width ( ), 8 );

			gradient. convertFromImage ( CUtility::createGradient ( gradient. size ( ), Qt::Horizontal, cg. base ( ), cg. highlight ( ), -100 ));

			QPixmapCache::insert ( key, gradient );
		}

		p-> drawTiledPixmap ( r. x ( ), r. y ( ) + offy, r. width ( ), r. height ( ) - offy, gradient );
	}
	p-> setPen ( d-> m_mcol );
	p-> drawPoint ( r. x ( ), r. y ( ) + offy );
	p-> drawPoint ( r. right ( ), r. y ( ) + offy );
}

void CTaskBar::drawHeaderContents ( QPainter *p, CTaskGroup *g, bool hot )
{
	const QColorGroup &cg = palette ( ). active ( );

	int x, y, w, h;
	g-> rect ( ). rect ( &x, &y, &w, &h );

	int offx = d-> m_hspace;

	if ( !g-> iconSet ( ). isNull ( )) {
		QPixmap pix = g-> iconSet ( ). pixmap ( QIconSet::Large, hot ? QIconSet::Active : QIconSet::Normal, QIconSet::On );

		if ( !pix. isNull ( )) {
			p-> drawPixmap ( x + offx, y + ( h > pix. height ( ) ? ( h - pix. height ( )) / 2 : 0 ), pix );
			offx += ( pix. width ( ) + d-> m_hspace );
		}
	}

	QColor stc = cg. highlightedText ( );
	QColor sbc = cg. highlight ( );
	QColor tc = cg. highlight ( );
	QColor bc = cg. base ( );
	if ( hot ) {
		stc = CUtility::gradientColor ( stc, sbc, .5f );
		tc = CUtility::gradientColor ( tc, bc, .5f );
	}
	p-> setPen ( g-> isSpecial ( ) ? stc : tc );
	p-> setFont ( d-> m_font );

	QRect tr ( x + offx, y + h - ( 2 * d-> m_vspace + d-> m_xh ), w - ( offx + d-> m_hspace ), 2 * d-> m_vspace + d-> m_xh );
	p-> drawText ( tr, Qt::AlignLeft | Qt::AlignVCenter, g-> text ( ));

	if ( g-> isExpandible ( )) {
		p-> setPen ( stc ); 

		QRect ar ( tr. right ( ) - d-> m_arrow, tr. top ( ) + ( tr. height ( ) - d-> m_arrow ) / 2, d-> m_arrow, d-> m_arrow );

		for ( int i = 0; i < d-> m_arrow; i++ ) {
			int off = i / 2;

			if ( g-> isExpanded ( ))
				p-> drawLine ( ar. left ( ) + off, ar. top ( ) + i, ar. right ( ) - off, ar. top ( ) + i );
			else
				p-> drawLine ( ar. left ( ) + i, ar. top ( ) + off, ar. left ( ) + i, ar. bottom ( ) - off );
		}
	}
	if ( hasFocus ( ))
		style ( ). drawPrimitive ( QStyle::PE_FocusRect, p, tr, cg, QStyle::Style_Default );

}

QSize CTaskBar::sizeHint ( ) const
{
	int w = 0, h = 0;

	w += ( d-> m_margins. left ( ) + d-> m_margins. right ( ));
	h += ( d-> m_margins. top ( ) + d-> m_margins. bottom ( ));

	if ( d-> m_condensed ) {
		w += 2;
		h += 2;
	}

	int iw = 0;

	bool first = true;

	for ( CTaskGroup *g = d-> m_groups-> first ( ); g; g = d-> m_groups-> next ( )) {
		if ( !first && !d-> m_condensed )
			h += d-> m_xh / 2;
		first = false;

		QSize gs = g-> sizeHint ( );

		if ( gs. width ( ) > iw )
			iw = gs. width ( );

		h += gs. height ( );
		h++;

		QWidget *content = g-> widget ( );

		if ( content && g-> isExpanded ( )) { 
			QSize cs = content-> sizeHint ( );

			if ( cs. isEmpty ( ))
				cs = content-> minimumSize ( );

			if (( cs. width ( ) + 2 ) > iw )
				iw = cs. width ( ) + 2;

			h += cs. height ( );
			h++;
		}
	}
	w += iw;

	return QSize ( w, h );
}

void CTaskBar::recalcLayout ( )
{
	updateGeometry ( );

	int offx = d-> m_margins. left ( );
	int offy = d-> m_margins. top ( );
	int w = width ( ) - offx - d-> m_margins. right ( );
	int h = height ( ) - offy - d-> m_margins. bottom ( );

	if ( w <= 0 || h <= 0 )
		return;

	if ( d-> m_condensed ) {
		offx++; offy++;
		w -= 2; h -= 2;
	}

	bool first = true;

	for ( CTaskGroup *g = d-> m_groups-> first ( ); g; g = d-> m_groups-> next ( )) {
		if ( !first && !d-> m_condensed )
			offy += d-> m_xh / 2;
		first = false;

		int gh = g-> sizeHint ( ). height ( );

		g-> setGeometry ( offx, offy, w, gh );
		offy += gh;
		offy++;

		QWidget *content = g-> widget ( );

		if ( content ) {
			if ( g-> isExpandible ( ) && g-> isExpanded ( )) { 
				QSize cs = content-> sizeHint ( );

				if ( cs. isEmpty ( ))
					cs = content-> minimumSize ( );

				int ch = cs. height ( );

				content-> setGeometry ( offx + ( d-> m_condensed ? 0 : 1 ), offy, w - ( d-> m_condensed ? 0 : 2 ), ch );
				offy += ch;
				offy++;
				content-> show ( );
			}
			else
				content-> hide ( );
		}
	}
}

QSize CTaskBar::sizeForGroup ( const CTaskGroup *g ) const
{
	QSize s;
	QFontMetrics fm ( d-> m_font );
	
	QSize icons ( 0, 0 );
	if ( !g-> iconSet ( ). isNull ( ))
		icons = g-> iconSet ( ). pixmap ( ). size ( );
	int tw = fm. width ( g-> text ( ));
	int th = 2 * d-> m_vspace + d-> m_xh;

	s. setWidth ( d-> m_hspace + icons. width ( ) + ( icons. width ( ) ? d-> m_hspace : 0 ) + tw + ( tw ? d-> m_hspace : 0 ) + ( g-> isExpandible ( ) ? d-> m_arrow + d-> m_hspace : 0 ));
	s. setHeight ( QMAX( icons. height ( ), th ));

	return s;
}

void CTaskBar::resizeEvent ( QResizeEvent *re )
{
	QWidget::resizeEvent ( re );
	recalcLayout ( );
}

void CTaskBar::paletteChange ( const QPalette &oldpal )
{
	const QColorGroup &cg = palette ( ). active ( );
	const QColor &colh = cg. highlight ( );
	const QColor &colb = cg. background ( );
	QSize ls ( d-> m_margins. left ( ), 8 );
	QSize rs ( d-> m_margins. right ( ), 8 );

	QColor colx = CUtility::gradientColor ( colh, colb, .2f  );

	d-> m_lgrad. convertFromImage ( CUtility::createGradient ( ls, Qt::Horizontal, colx, colb, 130 ));
	d-> m_rgrad. convertFromImage ( CUtility::createGradient ( rs, Qt::Horizontal, colx, colb, -130 ));
	d-> m_mcol = colx;

	QPixmapCache::clear ( );
	QWidget::paletteChange ( oldpal );
	update ( );
}

void CTaskBar::fontChange ( const QFont &oldfont )
{
	d-> m_font = font ( );
	d-> m_font. setBold ( true );

	QFontMetrics fm ( d-> m_font );

	d-> m_xw = fm. width ( "x" );
	d-> m_xh = fm. height ( );

	d-> m_arrow = ( d-> m_xh / 2 ) | 1;
	d-> m_hspace = d-> m_xw;
	d-> m_vspace = d-> m_xh / 6;

	QWidget::fontChange ( oldfont );
	recalcLayout ( );
	update ( );
}

void CTaskBar::paintEvent ( QPaintEvent *e )
{
	const QColorGroup &cg = palette ( ). active ( );
	QPainter p ( this );

	int w = width ( );
	int h = height ( );

	int ml = d-> m_margins. left ( );
	int mr = d-> m_margins. right ( );

	QRect rl = QRect ( 0, 0, ml, h ) & e-> rect ( );
	QPoint pl ( rl. left ( ), 0 );

	QRect rr = QRect ( w - mr, 0, mr, h ) & e-> rect ( );
	QPoint pr ( rr. left ( ) - ( w - mr ), 0 );

	QRect rm = QRect ( ml, 0, w - ml - mr, h ) & e-> rect ( );

	if ( !rl. isEmpty ( ))
		p. drawTiledPixmap ( rl, d-> m_lgrad, pl );
	if ( !rr. isEmpty ( ))
		p. drawTiledPixmap ( rr, d-> m_rgrad, pr );
	if ( !rm. isEmpty ( ))
		p. fillRect        ( rm, d-> m_mcol  );

	p. setPen ( CUtility::gradientColor ( cg. base ( ), cg. highlight ( ), .2f ));

	for ( CTaskGroup *g = d-> m_groups-> first ( ); g; g = d-> m_groups-> next ( )) {
		if ( g-> widget ( ) && g-> isExpanded ( )) {
			QRect wr = g-> widget ( )-> geometry ( );
			wr. addCoords ( -1, -1, 1, 1 );
			p. drawRect ( wr );
		}
		else {
			QRect gr = g-> geometry ( );
			p. drawLine ( gr. x ( ), gr. bottom ( ) + 1, gr. right ( ), gr. bottom ( ) + 1 );
		}
	}

	p. end ( );
}

