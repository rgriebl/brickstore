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
#include <qmainwindow.h>
#include <qdockarea.h>
#include <qpopupmenu.h>
#include <qaction.h>

#include "ctaskpanemanager.h"
#include "cutility.h"

class CTaskPane : public QWidget {
public:
	CTaskPane ( QWidget *parent, const char *name = 0 );
	virtual ~CTaskPane ( );

	void addItem ( QWidget *w, const QIconSet &is, const QString &txt, bool expandible = true, bool special = false );
	void removeItem ( QWidget *w, bool delete_widget = true );

	bool isExpanded ( QWidget *w ) const;
	void setExpanded ( QWidget *w, bool exp );

	virtual QSize sizeHint ( ) const;

protected:
	friend class CTaskGroup;

	void drawHeaderBackground ( QPainter *p, CTaskGroup *g, bool /*hot*/ );
	void drawHeaderContents ( QPainter *p, CTaskGroup *g, bool hot );
	void recalcLayout ( );
	QSize sizeForGroup ( const CTaskGroup *g ) const;

	virtual void resizeEvent ( QResizeEvent *re );
	virtual void paintEvent ( QPaintEvent *pe );
	virtual void contextMenuEvent( QContextMenuEvent *cme );
	virtual void paletteChange ( const QPalette &oldpal );
	virtual void fontChange ( const QFont &oldfont );

private:
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

CTaskPaneManager::CTaskPaneManager ( QMainWindow *parent, const char *name )
	: QObject ( parent, name )
{
	m_idcounter = 0;
	m_mainwindow = parent;
	m_panedock = 0;
	m_taskpane = 0;

	m_mode = Classic;
	setMode ( Modern );
}

CTaskPaneManager::~CTaskPaneManager ( )
{
}

int CTaskPaneManager::addItem ( QWidget *w, const QIconSet &is, const QString &text )
{
	if ( w ) {
		Item item;
		item. m_id         = m_idcounter++;
		item. m_itemdock   = 0;
		item. m_widget     = w;
		item. m_iconset    = is;
		item. m_label      = text;
		item. m_expandible = true;
		item. m_special    = false;
		item. m_expanded   = false;
		item. m_orig_framestyle  = QFrame::NoFrame;

		kill ( );
		m_items. append ( item );
		create ( );

		return item. m_id;
	}
	return -1;
}

void CTaskPaneManager::removeItem ( int id, bool delete_widget )
{
	for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		Item &item = *it;

		if ( item. m_id == id ) {
			kill ( );

			QWidget *delptr = delete_widget ? item. m_widget : 0;
			m_items. remove ( it );
			delete delptr;

			create ( );
			break;
		}
	}
}

QWidget *CTaskPaneManager::widget ( int id ) const
{
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		if ((*it). m_id == id )
			return (*it). m_widget;
	}
	return 0;
}

int CTaskPaneManager::id ( QWidget *widget ) const
{
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		if ((*it). m_widget == widget )
			return (*it). m_id;
	}
	return -1;
}

QValueList <int> CTaskPaneManager::allItems ( ) const
{
	QValueList <int> il;
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it )
		il << (*it). m_id;
	return il;
}

QString CTaskPaneManager::label ( int id ) const
{
	QString l;
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		if ((*it). m_id == id ) {
			l = (*it). m_label;
			break;
		}
	}
	return l;
}

QIconSet CTaskPaneManager::iconSet ( int id ) const
{
	QIconSet is;
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		if ((*it). m_id == id ) {
			is = (*it). m_iconset;
			break;
		}
	}
	return is;

}

bool CTaskPaneManager::isItemVisible ( int id ) const
{
	bool vis = false;
	for ( QValueList<Item>::const_iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		const Item &item = *it;
		if ( item. m_id == id ) {
			if ( m_mode == Modern )
				vis = m_taskpane-> isExpanded ( item. m_widget );
			else
				vis = item. m_itemdock-> isVisible ( );
			break;
		}
	}
	return vis;
}

void CTaskPaneManager::setItemVisible ( int id, bool visible )
{
	for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
		Item &item = *it;

		if ( item. m_id == id ) {
			if ( visible != item. m_visible ) {
				item. m_visible = visible;

				if ( m_mode == Modern )
					m_taskpane-> setExpanded ( item. m_widget, visible );
				else
					item. m_itemdock-> setShown ( visible );
			}
			break;
		}
	}
}


void CTaskPaneManager::setMode ( Mode m )
{
	if ( m == m_mode )
		return;

	kill ( );
	m_mode = m;
	create ( );
}

void CTaskPaneManager::kill ( )
{
	// kill existing window
	if ( m_mode == Modern ) {
		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			Item &item = *it;

			if ( ::qt_cast <QFrame *> ( item. m_widget )) {
				QFrame *f = static_cast <QFrame *> ( item. m_widget );
				f-> setFrameStyle ( item. m_orig_framestyle );
			}

			m_taskpane-> removeItem ( item. m_widget, false );
		}
		delete m_panedock;
		m_panedock = 0;
		m_taskpane = 0;
	}
	else {
		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			Item &item = *it;

			item. m_widget-> hide ( );
			item. m_widget-> reparent ( 0, QPoint ( 0, 0 ));

			delete item. m_itemdock;
			item. m_itemdock = 0;
		}
	}
}

void CTaskPaneManager::create ( )
{
	// create new window
	if ( m_mode == Modern ) {
		m_panedock = new QDockWindow ( QDockWindow::InDock, m_mainwindow-> leftDock ( ), "panedock" );

		m_panedock-> setMovingEnabled ( false );
		m_panedock-> setCloseMode ( QDockWindow::Never );
		m_panedock-> setVerticallyStretchable ( true );
		
		m_taskpane = new CTaskPane ( m_panedock, "taskpane" );

		m_panedock-> boxLayout ( )-> setMargin ( 0 );
		m_panedock-> boxLayout ( )-> setSpacing ( 0 );
		m_panedock-> boxLayout ( )-> addWidget ( m_taskpane );

		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			Item &item = *it;

			if ( ::qt_cast <QFrame *> ( item. m_widget )) {
				QFrame *f = static_cast <QFrame *> ( item. m_widget );
				
				item. m_orig_framestyle = f-> frameStyle ( );
				f-> setFrameStyle ( QFrame::NoFrame );
			}

			m_taskpane-> addItem ( item. m_widget, item. m_iconset, item. m_label, item. m_expandible, item. m_special );
			item. m_widget-> show ( );
		//	if ( !item-> m_expanded )
		//		m_taskpane-> setExpanded ( item-> m_taskpaneid, false );
		}
		m_taskpane-> show ( );
		m_panedock-> show ( );
	}
	else {
		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			Item &item = *it;

			item. m_itemdock = new QDockWindow ( QDockWindow::InDock, m_mainwindow-> leftDock ( ), "itemdock" );
			item. m_itemdock-> setMovingEnabled ( true );
			item. m_itemdock-> setResizeEnabled ( true );
			item. m_itemdock-> setVerticallyStretchable ( false );
			item. m_itemdock-> setHorizontallyStretchable ( false );
			item. m_itemdock-> setCloseMode ( QDockWindow::Always );
			item. m_itemdock-> setCaption ( item. m_label );

			item. m_widget-> reparent ( item. m_itemdock, QPoint ( 0, 0 ));
			item. m_itemdock-> boxLayout ( )-> setMargin ( 1 );
			item. m_itemdock-> boxLayout ( )-> addWidget ( item. m_widget );

			item. m_widget-> show ( );
			item. m_itemdock-> show ( );			
		}
	}
}

QPopupMenu *CTaskPaneManager::createItemMenu ( ) const
{
	QPopupMenu *m = new QPopupMenu ( m_mainwindow, "taskpanemanager_itemmenu" );
	m-> setCheckable ( true );
	connect ( m, SIGNAL( aboutToShow ( )), this, SLOT( itemMenuAboutToShow ( )));
	return m;
}

void CTaskPaneManager::itemMenuAboutToShow ( )
{
	QPopupMenu *m = ::qt_cast <QPopupMenu *> ( sender ( ));

	if ( m ) {
		m-> clear ( );

		bool allon = true;

		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			int id = m-> insertItem ((*it). m_label, this, SLOT( itemMenuActivated ( int )));
			m-> setItemParameter ( id, (*it). m_id );

			bool vis = isItemVisible ((*it). m_id );
			if ( m_mode == Modern ) {
				bool pdvis = m_panedock-> isVisible ( );
				vis &= pdvis;
				m-> setItemEnabled ( id, pdvis );
			}
			m-> setItemChecked ( id, vis );
			allon &= vis;
		}
		m-> insertSeparator ( );
		int allid = m-> insertItem ( tr( "All" ), this, SLOT( itemMenuActivated ( int )));

		if ( m_mode == Modern ) 
			allon = m_panedock-> isVisible ( );

		m-> setItemChecked ( allid, allon );
		m-> setItemParameter ( allid, allon ? -2 : -1 );

		m-> insertSeparator ( );

		int modernid = m-> insertItem ( tr( "Modern (fixed)" ), this, SLOT( itemMenuActivated ( int )));
		m-> setItemChecked ( modernid, m_mode == Modern );
		m-> setItemParameter ( modernid, -3 );

		int classicid = m-> insertItem ( tr( "Classic (moveable)" ), this, SLOT( itemMenuActivated ( int )));
		m-> setItemChecked ( classicid, m_mode != Modern );
		m-> setItemParameter ( classicid, -4 );
	}
}

void CTaskPaneManager::itemMenuActivated ( int id )
{
	if ( id == -4 ) {
		setMode ( Classic );
	}
	else if ( id == -3 ) {
		setMode ( Modern );
	}
	else if (( id < 0 ) && ( m_mode == Modern )) {
		m_panedock-> setShown ( id == -1 ? true : false );
	}
	else {
		for ( QValueList<Item>::iterator it = m_items. begin ( ); it != m_items. end ( ); ++it ) {
			Item &item = *it;

			if (( item. m_id == id ) || ( id < 0 )) {
				if ( m_mode == Modern ) {
					m_taskpane-> setExpanded ( item. m_widget, !m_taskpane-> isExpanded ( item. m_widget ));
				}
				else {
					bool onoff;

					if ( id == -1 )
						onoff = true;
					else if ( id == -2 )
						onoff = false;
					else
						onoff = !item. m_itemdock-> isVisible ( );

					item. m_itemdock-> setShown ( onoff );
				}
			}
		}
	}
}

class TaskPaneAction : public QActionGroup {
public:
	TaskPaneAction ( CTaskPaneManager *tpm, QObject *parent, const char *name = 0 )
		: QActionGroup ( parent, name, false ), m_tpm ( tpm )
	{ }

	virtual bool addTo ( QWidget *w )
	{
		if ( w-> inherits ( "QPopupMenu" )) {
			static_cast <QPopupMenu *> ( w )-> insertItem ( menuText ( ), m_tpm-> createItemMenu ( ));
			return true;
		}
		return false;
	}

private:
	CTaskPaneManager *m_tpm;
};

QAction *CTaskPaneManager::createItemAction ( QObject *parent, const char *name ) const
{
	return new TaskPaneAction ( const_cast<CTaskPaneManager *> ( this ), parent, name );
}


// vvv PRIVATE vvv

class CTaskGroup : public QWidget {
private:
	friend class CTaskPane;

	CTaskGroup ( CTaskPane *parent )
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

	CTaskPane *m_bar;
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


CTaskPane::CTaskPane ( QWidget *parent, const char *name )
	: QWidget ( parent, name )
{
	m_margins = QRect ( 6, 6, 1, 1 );
	m_condensed = false;
	m_groups = new QPtrList <CTaskGroup>;
	//m_groups-> setAutoDelete ( true );

	fontChange ( font ( ));
	paletteChange ( palette ( ));

	setWFlags ( Qt::WNoAutoErase );
	setBackgroundMode ( Qt::NoBackground );
	setSizePolicy ( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
}

CTaskPane::~CTaskPane ( )
{
}

void CTaskPane::addItem ( QWidget *w, const QIconSet &is, const QString &txt, bool expandible, bool special )
{
	CTaskGroup *g = new CTaskGroup ( this );
	g-> setIconSet ( is );
	g-> setText ( txt );
	g-> setSpecial ( special );
	g-> setExpandible ( expandible );
	g-> setWidget ( w );
	g-> show ( );

	m_groups-> append ( g );
	recalcLayout ( );
}

void CTaskPane::removeItem ( QWidget *w, bool delete_widget )
{
	for ( uint i = 0; i < m_groups-> count ( ); i++ ) {
		if ( w == m_groups-> at ( i )-> widget ( )) {
			m_groups-> remove ( i );

			if ( delete_widget )
				delete w;
			else
				w-> reparent ( 0, QPoint ( 0, 0 ));
		}
	}
}

bool CTaskPane::isExpanded ( QWidget *w ) const
{
	bool exp = false;

	for ( QPtrListIterator <CTaskGroup> it ( *m_groups ); it. current ( ); ++it ) {
		if ( w == it. current ( )-> widget ( )) {
			exp = it. current ( )-> isExpanded ( );
			break;
		}
	}
	return exp;
}

void CTaskPane::setExpanded ( QWidget *w, bool exp )
{
	for ( QPtrListIterator <CTaskGroup> it ( *m_groups ); it. current ( ); ++it ) {
		if ( w == it. current ( )-> widget ( )) {
			it. current ( )-> setExpanded ( exp );
			break;
		}
	}
}

void CTaskPane::drawHeaderBackground ( QPainter *p, CTaskGroup *g, bool /*hot*/ )
{
	const QColorGroup &cg = palette ( ). active ( );
	QRect r = g-> rect ( );

	int offy = r. height ( ) - ( m_vspace + m_xh + m_vspace );

	if ( offy > 0 )
		p-> fillRect ( r. x ( ), r. y ( ), r. width ( ), offy, m_mcol );

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
	p-> setPen ( m_mcol );
	p-> drawPoint ( r. x ( ), r. y ( ) + offy );
	p-> drawPoint ( r. right ( ), r. y ( ) + offy );
}

void CTaskPane::drawHeaderContents ( QPainter *p, CTaskGroup *g, bool hot )
{
	const QColorGroup &cg = palette ( ). active ( );

	int x, y, w, h;
	g-> rect ( ). rect ( &x, &y, &w, &h );

	int offx = m_hspace;

	if ( !g-> iconSet ( ). isNull ( )) {
		QPixmap pix = g-> iconSet ( ). pixmap ( QIconSet::Large, hot ? QIconSet::Active : QIconSet::Normal, QIconSet::On );

		if ( !pix. isNull ( )) {
			p-> drawPixmap ( x + offx, y + ( h > pix. height ( ) ? ( h - pix. height ( )) / 2 : 0 ), pix );
			offx += ( pix. width ( ) + m_hspace );
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
	p-> setFont ( m_font );

	QRect tr ( x + offx, y + h - ( 2 * m_vspace + m_xh ), w - ( offx + m_hspace ), 2 * m_vspace + m_xh );
	p-> drawText ( tr, Qt::AlignLeft | Qt::AlignVCenter, g-> text ( ));

	if ( g-> isExpandible ( )) {
		p-> setPen ( stc ); 

		QRect ar ( tr. right ( ) - m_arrow, tr. top ( ) + ( tr. height ( ) - m_arrow ) / 2, m_arrow, m_arrow );

		for ( int i = 0; i < m_arrow; i++ ) {
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

QSize CTaskPane::sizeHint ( ) const
{
	int w = 0, h = 0;

	w += ( m_margins. left ( ) + m_margins. right ( ));
	h += ( m_margins. top ( ) + m_margins. bottom ( ));

	if ( m_condensed ) {
		w += 2;
		h += 2;
	}

	int iw = 0;

	bool first = true;

	for ( CTaskGroup *g = m_groups-> first ( ); g; g = m_groups-> next ( )) {
		if ( !first && !m_condensed )
			h += m_xh / 2;
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

void CTaskPane::recalcLayout ( )
{
	updateGeometry ( );

	int offx = m_margins. left ( );
	int offy = m_margins. top ( );
	int w = width ( ) - offx - m_margins. right ( );
	int h = height ( ) - offy - m_margins. bottom ( );

	if ( w <= 0 || h <= 0 )
		return;

	if ( m_condensed ) {
		offx++; offy++;
		w -= 2; h -= 2;
	}

	bool first = true;

	for ( CTaskGroup *g = m_groups-> first ( ); g; g = m_groups-> next ( )) {
		if ( !first && !m_condensed )
			offy += m_xh / 2;
		first = false;

		int gh = g-> sizeHint ( ). height ( );

		g-> setGeometry ( offx, offy, w, gh );
		offy += gh;
		offy++;

		QWidget *content = g-> widget ( );

		if ( content ) {
			if ( g-> isExpandible ( ) && g-> isExpanded ( )) { 
				QSize cs = content-> minimumSizeHint ( );

				if ( cs. isEmpty ( ))
					cs = content-> minimumSize ( );

				int ch = cs. height ( );

				content-> setGeometry ( offx + ( m_condensed ? 0 : 1 ), offy, w - ( m_condensed ? 0 : 2 ), ch );
				offy += ch;
				offy++;
				content-> show ( );
			}
			else
				content-> hide ( );
		}
	}
}

QSize CTaskPane::sizeForGroup ( const CTaskGroup *g ) const
{
	QSize s;
	QFontMetrics fm ( m_font );
	
	QSize icons ( 0, 0 );
	if ( !g-> iconSet ( ). isNull ( ))
		icons = g-> iconSet ( ). pixmap ( ). size ( );
	int tw = fm. width ( g-> text ( ));
	int th = 2 * m_vspace + m_xh;

	s. setWidth ( m_hspace + icons. width ( ) + ( icons. width ( ) ? m_hspace : 0 ) + tw + ( tw ? m_hspace : 0 ) + ( g-> isExpandible ( ) ? m_arrow + m_hspace : 0 ));
	s. setHeight ( QMAX( icons. height ( ), th ));

	return s;
}

void CTaskPane::resizeEvent ( QResizeEvent *re )
{
	QWidget::resizeEvent ( re );
	recalcLayout ( );
}

void CTaskPane::contextMenuEvent( QContextMenuEvent *cme )
{
	cme-> accept ( );
}

void CTaskPane::paletteChange ( const QPalette &oldpal )
{
	const QColorGroup &cg = palette ( ). active ( );
	const QColor &colh = cg. highlight ( );
	const QColor &colb = cg. background ( );
	QSize ls ( m_margins. left ( ), 8 );
	QSize rs ( m_margins. right ( ), 8 );

	QColor colx = CUtility::gradientColor ( colh, colb, .2f  );

	m_lgrad. convertFromImage ( CUtility::createGradient ( ls, Qt::Horizontal, colx, colb, 130 ));
	m_rgrad. convertFromImage ( CUtility::createGradient ( rs, Qt::Horizontal, colx, colb, -130 ));
	m_mcol = colx;

	QPixmapCache::clear ( );
	QWidget::paletteChange ( oldpal );
	update ( );
}

void CTaskPane::fontChange ( const QFont &oldfont )
{
	m_font = font ( );
	m_font. setBold ( true );

	QFontMetrics fm ( m_font );

	m_xw = fm. width ( "x" );
	m_xh = fm. height ( );

	m_arrow = ( m_xh / 2 ) | 1;
	m_hspace = m_xw;
	m_vspace = m_xh / 6;

	QWidget::fontChange ( oldfont );
	recalcLayout ( );
	update ( );
}

void CTaskPane::paintEvent ( QPaintEvent *e )
{
	const QColorGroup &cg = palette ( ). active ( );
	QPainter p ( this );

	int w = width ( );
	int h = height ( );

	int ml = m_margins. left ( );
	int mr = m_margins. right ( );

	QRect rl = QRect ( 0, 0, ml, h ) & e-> rect ( );
	QPoint pl ( rl. left ( ), 0 );

	QRect rr = QRect ( w - mr, 0, mr, h ) & e-> rect ( );
	QPoint pr ( rr. left ( ) - ( w - mr ), 0 );

	QRect rm = QRect ( ml, 0, w - ml - mr, h ) & e-> rect ( );

	if ( !rl. isEmpty ( ))
		p. drawTiledPixmap ( rl, m_lgrad, pl );
	if ( !rr. isEmpty ( ))
		p. drawTiledPixmap ( rr, m_rgrad, pr );
	if ( !rm. isEmpty ( ))
		p. fillRect        ( rm, m_mcol  );

	p. setPen ( CUtility::gradientColor ( cg. base ( ), cg. highlight ( ), .2f ));

	for ( CTaskGroup *g = m_groups-> first ( ); g; g = m_groups-> next ( )) {
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

