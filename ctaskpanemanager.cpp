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
#include <qicon.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qpixmapcache.h>
#include <qstyle.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qfont.h>
#include <qcursor.h>
#include <QMainWindow>
#include <QDockWidget>
#include <q3popupmenu.h>
#include <qaction.h>
#include <qstyleoption.h>
//Added by qt3to4:
#include <QContextMenuEvent>
#include <Q3ValueList>
#include <QFrame>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEvent>
#include <Q3ActionGroup>
#include <QPaintEvent>
#include <qDebug>
#include "ctaskpanemanager.h"
#include "cutility.h"


class CTaskPaneManagerPrivate {
public:
	CTaskPaneManager::Mode  m_mode;
    QMainWindow *m_mainwindow;
    QDockWidget *m_panedock;
	CTaskPane   *m_taskpane;

	struct Item {
        QDockWidget *m_itemdock;
		uint         m_taskpaneid;
		QWidget *    m_widget;
		QIcon        m_icon;
		QString      m_label;
		int          m_orig_framestyle;
		bool         m_expandible       : 1;
		bool         m_special          : 1;
		bool         m_visible          : 1;

		Item ( )
			: m_itemdock ( 0 ), m_taskpaneid ( 0 ),
            m_widget ( 0 ), m_orig_framestyle ( QFrame::NoFrame ), m_expandible ( false ),
			m_special ( false ), m_visible ( true )
		{ }

		Item ( const Item &copy )
			: m_itemdock ( copy. m_itemdock ), m_taskpaneid ( copy. m_taskpaneid ),
            m_widget ( copy. m_widget ), m_icon ( copy. m_icon ), m_label ( copy. m_label ),
			m_orig_framestyle ( copy. m_orig_framestyle ), m_expandible ( copy. m_expandible ),
			m_special ( copy. m_special ), m_visible ( copy. m_visible )
		{ }
	};

	Q3ValueList<Item>::iterator findItem ( QWidget *w );
	Q3ValueList<Item>::iterator noItem ( );

	Q3ValueList<Item> m_items;

	static QWidget *s_nullwidget;
};

QWidget *CTaskPaneManagerPrivate::s_nullwidget = 0;


CTaskPaneManager::CTaskPaneManager (QMainWindow *parent, const char *name )
	: QObject ( parent, name )
{
	d = new CTaskPaneManagerPrivate;
	d-> m_mainwindow = parent;
	d-> m_panedock = 0;
	d-> m_taskpane = 0;

#if defined( Q_WS_MACX )
	// MacOSX 10.4 Intel/Qt 3.3.6 crashes when reparenting to 0
	
	if ( !d-> s_nullwidget )
		d-> s_nullwidget = new QWidget ( 0 );
#endif

    d-> m_mode = Classic;
    setMode ( Modern );
}

CTaskPaneManager::~CTaskPaneManager ( )
{
	delete d;
}

Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator CTaskPaneManagerPrivate::findItem ( QWidget *w )
{
	Q3ValueList<Item>::iterator it = m_items. begin ( );

	while ( it != m_items. end ( )) {
		if ((*it). m_widget == w )
			break;
		++it;
	}
	return it;
}

Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator CTaskPaneManagerPrivate::noItem ( )
{
	return m_items. end ( );
}

void CTaskPaneManager::addItem ( QWidget *w, const QIcon &is, const QString &text )
{
	if ( w && ( d-> findItem ( w ) == d-> noItem ( ))) {
		CTaskPaneManagerPrivate::Item item;
		item. m_itemdock   = 0;
		item. m_widget     = w;
        item. m_icon       = is;
		item. m_label      = text;
		item. m_expandible = true;
		item. m_special    = false;
        item. m_orig_framestyle  = QFrame::NoFrame;

		kill ( );
		d-> m_items. append ( item );
		create ( );
	}
}

void CTaskPaneManager::removeItem ( QWidget *w, bool delete_widget )
{
	Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> findItem ( w );
	
	if ( it != d-> noItem ( )) {
		kill ( );

		d-> m_items. remove ( it );
		if ( delete_widget )
			delete w;

		create ( );
	}
}

void CTaskPaneManager::setItemText ( QWidget *w, const QString &txt )
{
	Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> findItem ( w );
	
	if ( it != d-> noItem ( )) {
		(*it). m_label = txt;
		
		kill ( );
		create ( );
	}
}


QString CTaskPaneManager::itemText ( QWidget *w ) const
{
	QString txt;
	Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> findItem ( w );
	
	if ( it != d-> noItem ( ))
		txt = (*it). m_label;

	return txt;
}


bool CTaskPaneManager::isItemVisible ( QWidget *w ) const
{
	Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> findItem ( w );
	
	if ( it != d-> noItem ( ))
		return (*it). m_visible;
	
	return false;
}

void CTaskPaneManager::setItemVisible ( QWidget *w, bool visible )
{
	Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> findItem ( w );
	
	if ( it != d-> noItem ( )) {
		if ( visible != (*it). m_visible ) {
			(*it). m_visible = visible;

			if ( d-> m_mode == Modern )
				d-> m_taskpane-> setExpanded ( w, visible );
			else
				(*it). m_itemdock-> setShown ( visible );
		}
	}
}

CTaskPaneManager::Mode CTaskPaneManager::mode ( ) const
{
	return d-> m_mode;
}

void CTaskPaneManager::setMode ( Mode m )
{
	if ( m == d-> m_mode )
		return;

	kill ( );
	d-> m_mode = m;
	create ( );
}

void CTaskPaneManager::kill ( )
{
	// kill existing window
	if ( d-> m_mode == Modern ) {
		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
			CTaskPaneManagerPrivate::Item &item = *it;

            if ( ::qobject_cast <QFrame *> ( item. m_widget )) {
                QFrame *f = static_cast <QFrame *> ( item. m_widget );
				f-> setFrameStyle ( item. m_orig_framestyle );
			}

			d-> m_taskpane-> removeItem ( item. m_widget, false );
		}
		delete d-> m_panedock;
		d-> m_panedock = 0;
		d-> m_taskpane = 0;
	}
	else {
		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
			CTaskPaneManagerPrivate::Item &item = *it;

			disconnect ( item. m_itemdock, SIGNAL( visibilityChanged ( bool )), this, SLOT( dockVisibilityChanged ( bool )));

			item. m_widget-> hide ( );
			item. m_widget-> reparent ( d-> s_nullwidget, QPoint ( 0, 0 ));

			delete item. m_itemdock;
			item. m_itemdock = 0;
		}
	}
}

void CTaskPaneManager::create ( )
{
	// create new window
	if ( d-> m_mode == Modern ) {
        d-> m_panedock = new QDockWidget("panedock", d-> m_mainwindow);

        d-> m_taskpane = new CTaskPane ( d-> m_panedock, "taskpane" );

        QWidget* titleWidget = new QWidget(d-> m_mainwindow);
        d-> m_panedock-> setTitleBarWidget( titleWidget );
        d-> m_panedock-> setFeatures(QDockWidget::NoDockWidgetFeatures);
        d-> m_panedock-> setWidget(d-> m_taskpane);

        d-> m_taskpane-> setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

        d-> m_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, d->m_panedock, Qt::Vertical);

		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
			CTaskPaneManagerPrivate::Item &item = *it;

            if ( ::qobject_cast <QFrame *> ( item. m_widget )) {
                QFrame *f = static_cast <QFrame *> ( item. m_widget );
				
				item. m_orig_framestyle = f-> frameStyle ( );
                f-> setFrameStyle ( QFrame::NoFrame );
            }

            d-> m_taskpane-> addItem ( item. m_widget, item. m_icon, item. m_label, item. m_expandible, item. m_special );
			item. m_widget-> show ( );
			d-> m_taskpane-> setExpanded ( item. m_widget, item. m_visible );
		}
		d-> m_taskpane-> show ( );
		d-> m_panedock-> show ( );

		connect ( d-> m_taskpane, SIGNAL( itemVisibilityChanged ( QWidget *, bool )), this, SLOT( itemVisibilityChanged ( QWidget *, bool )));
	}
	else {
		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
			CTaskPaneManagerPrivate::Item &item = *it;

            item. m_itemdock = new QDockWidget ( "itemdock", d-> m_mainwindow );
            d-> m_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, item.m_itemdock);
			item. m_itemdock-> setCaption ( item. m_label );

			item. m_widget-> reparent ( item. m_itemdock, QPoint ( 0, 0 ));
			item. m_widget-> show ( );

			if ( item. m_visible )
				item. m_itemdock-> show ( );
			else
				item. m_itemdock-> hide ( );

			connect ( item. m_itemdock, SIGNAL( visibilityChanged ( bool )), this, SLOT( dockVisibilityChanged ( bool )));
		}
	}
}

void CTaskPaneManager::dockVisibilityChanged ( bool b )
{
    QDockWidget *dock = ::qobject_cast <QDockWidget *> ( sender ( ));

	for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
		CTaskPaneManagerPrivate::Item &item = *it;

		if ( item. m_itemdock == dock ) {
			item. m_visible = b;
			break;
		}
	}	
}

void CTaskPaneManager::itemVisibilityChanged ( QWidget *w, bool b )
{
	for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it ) {
		CTaskPaneManagerPrivate::Item &item = *it;

		if ( item. m_widget == w ) {
			item. m_visible = b;
			break;
		}
	}	
}

QMenu *CTaskPaneManager::createItemVisibilityMenu ( ) const
{
    QMenu *m = new QMenu ( "taskpanemanager_itemmenu", d-> m_mainwindow );
	m-> setCheckable ( true );
	connect ( m, SIGNAL( aboutToShow ( )), this, SLOT( itemMenuAboutToShow ( )));
	return m;
}

void CTaskPaneManager::itemMenuAboutToShow ( )
{
    QMenu *m = ::qobject_cast <QMenu *> ( sender ( ));

	if ( m ) {
		m-> clear ( );

		bool allon = true;

		int index = 0;
		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it, index++ ) {
			int id = m-> insertItem ((*it). m_label, this, SLOT( itemMenuActivated ( int )));
			m-> setItemParameter ( id, index );

			bool vis = isItemVisible ((*it). m_widget );
			if ( d-> m_mode == Modern ) {
				bool pdvis = d-> m_panedock-> isVisible ( );
				vis &= pdvis;
				m-> setItemEnabled ( id, pdvis );
			}
			m-> setItemChecked ( id, vis );
			allon &= vis;
		}
		m-> insertSeparator ( );
		int allid = m-> insertItem ( tr( "All" ), this, SLOT( itemMenuActivated ( int )));

		if ( d-> m_mode == Modern ) 
			allon = d-> m_panedock-> isVisible ( );

		m-> setItemChecked ( allid, allon );
		m-> setItemParameter ( allid, allon ? -2 : -1 );

		m-> insertSeparator ( );

		int modernid = m-> insertItem ( tr( "Modern (fixed)" ), this, SLOT( itemMenuActivated ( int )));
		m-> setItemChecked ( modernid, d-> m_mode == Modern );
		m-> setItemParameter ( modernid, -3 );

		int classicid = m-> insertItem ( tr( "Classic (moveable)" ), this, SLOT( itemMenuActivated ( int )));
		m-> setItemChecked ( classicid, d-> m_mode != Modern );
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
	else if (( id < 0 ) && ( d-> m_mode == Modern )) {
		d-> m_panedock-> setShown ( id == -1 ? true : false );
	}
	else {
		int index = 0;
		for ( Q3ValueList<CTaskPaneManagerPrivate::Item>::iterator it = d-> m_items. begin ( ); it != d-> m_items. end ( ); ++it, index++ ) {
			CTaskPaneManagerPrivate::Item &item = *it;


			if (( id == index ) || ( id < 0 )) {
				bool onoff = !isItemVisible ( item. m_widget );

				if ( d-> m_mode == Classic ) {
					if ( id == -1 )
						onoff = true;
					else if ( id == -2 )
						onoff = false;
				}
				setItemVisible ( item. m_widget, onoff );
			}
		}
	}
}

class TaskPaneAction : public QMenu {
public:
    TaskPaneAction ( CTaskPaneManager *tpm, QWidget *parent, const char *name = 0 )
        : QMenu ( name, parent ), m_tpm ( tpm )
    {
        setObjectName( name );
//        return m_tpm-> createItemVisibilityMenu ( );
    }

private:
	CTaskPaneManager *m_tpm;
    QMap <QMenu *, int> m_update_menutexts;
};

QMenu *CTaskPaneManager::createItemVisibilityAction ( QWidget *parent, const char *name ) const
{
    QMenu* q = const_cast<CTaskPaneManager *> ( this )->createItemVisibilityMenu();
    q->setObjectName( name );
    return q;
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

	void setIcon ( const QIcon &is )
	{
		m_is = is;
		m_bar-> recalcLayout ( );
		update ( );
	}

	QIcon icon ( ) const
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
            emit m_bar-> itemVisibilityChanged ( m_widget, b );
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

	void mouseReleaseEvent ( QMouseEvent *e )
	{
		if ( e-> button ( ) == Qt::LeftButton ) {
			setExpanded ( !isExpanded ( ));
			e-> accept ( );
		}
	}

protected:
	bool m_expandible  : 1;
	bool m_expanded    : 1;
	bool m_special     : 1;
	bool m_hot         : 1;

	int m_leadin;
	int m_leadout;

	QWidget *m_widget;
	QIcon m_is;
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
    m_groups = new QList <CTaskGroup *>;
	//m_groups-> setAutoDelete ( true );

	fontChange ( font ( ));
	paletteChange ( palette ( ));

    setWindowFlags ( Qt::WNoAutoErase );
	setBackgroundMode ( Qt::NoBackground );
	setSizePolicy ( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
}

CTaskPane::~CTaskPane ( )
{ }

void CTaskPane::addItem ( QWidget *w, const QIcon &is, const QString &txt, bool expandible, bool special )
{
	CTaskGroup *g = new CTaskGroup ( this );
    g-> setIcon ( is );
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
    for ( int i = 0; i < m_groups-> count ( ); i++ ) {
		if ( w == m_groups-> at ( i )-> widget ( )) {
            m_groups-> removeAt ( i );

			if ( delete_widget )
				delete w;
			else
				w-> reparent ( CTaskPaneManagerPrivate::s_nullwidget, QPoint ( 0, 0 ));
		}
	}
}

bool CTaskPane::isExpanded ( QWidget *w ) const
{
	bool exp = false;

    QListIterator<CTaskGroup *> i(*m_groups);

    while (i.hasNext()) {
        CTaskGroup * tg = i.next();
        if (w == tg->widget()) {
            exp = tg->isExpanded();
            break;
        }
    }

	return exp;
}

void CTaskPane::setExpanded ( QWidget *w, bool exp )
{
    QListIterator<CTaskGroup *> i(*m_groups);

    while (i.hasNext()) {
        CTaskGroup * tg = i.next();
        if (w == tg->widget()) {
            tg->setExpanded(exp);
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

    if ( !g-> icon ( ). isNull ( )) {
        QPixmap pix = g-> icon ( ). pixmap ( QIcon::Large, hot ? QIcon::Active : QIcon::Normal, QIcon::On );

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
    if ( hasFocus ( )) {
        QStyleOption option;
        option.state = QStyle::State_None;
        style ( )-> drawPrimitive ((QStyle::PrimitiveElement) QStyle::CE_FocusFrame, &option, p);
    }
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

    QListIterator<CTaskGroup *> i(*m_groups);

    while (i.hasNext()) {
        CTaskGroup *g = i.next();

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

    QListIterator<CTaskGroup *> i(*m_groups);

    while (i.hasNext()) {
        CTaskGroup *g = i.next();
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
    if ( !g-> icon ( ). isNull ( ))
        icons = g-> icon ( ). pixmap ( ). size ( );
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

    QListIterator<CTaskGroup *> i(*m_groups);

    while (i.hasNext()) {
        CTaskGroup *g = i.next();
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
