#include <qlayout.h>
#include <qworkspace.h>
#include <qwidgetstack.h>
#include <qtabbar.h>
#include <qlayout.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qwindowsstyle.h>
#include <qtoolbutton.h>
#include <qtooltip.h>

#include "cresource.h"

#include "cworkspace.h"

#if 0
namespace {

class MyTabBar : public QTabBar {
public:
	MyTabBar ( QWidget *parent, const char *name = 0 )
		: QTabBar ( parent, name )
	{ }

	virtual void layoutTabs ( )
	{
		static bool lock = false;

		if ( lock )
			return;
		else
			lock = true;

		QFont f = font ( );
		QFont fb = f;
		fb. setBold ( true );
		setFont ( fb );

		QTabBar::layoutTabs ( );

		setFont ( f );

		lock = false;
	}
	
protected:
	virtual void paintLabel( QPainter *p, const QRect &br, QTab *t, bool has_focus ) const
	{
		bool draw_bold = false;

		if ( t && ( t == tab ( currentTab ( ))))
			draw_bold = true;

		if ( draw_bold ) {
			p-> save ( );
			QFont f = p-> font ( );
			f. setBold ( true );
			p-> setFont ( f );
		}

		QTabBar::paintLabel ( p, br, t, has_focus );

		if ( draw_bold )
			p-> restore ( );
	}
};

} // namespace
#endif

CWorkspace::CWorkspace ( QWidget *parent, const char *name )
	: QWidget ( parent, name )
{
	m_mode        = Tabbed;
	m_block_close = false;
	m_reparenting = false;

	m_tabbar    = new QTabBar ( this, "tabbar" );
	m_stack     = new QWidgetStack ( this, "stack" );
	m_workspace = new QWorkspace ( this, "workspace" );
	m_close     = new QToolButton ( this );

	m_close-> setAutoRaise ( true );
	m_close-> setIconSet ( CResource::inst ( )-> iconSet ( "file_close" ));
	m_close-> setFixedSize ( 0, 0 );
	QObject::connect ( m_close, SIGNAL( clicked ( )), this, SLOT( closeTabClicked ( )));
	QToolTip::add ( m_close, tr( "Close current document" ) );

	/*
	m_tabbar-> setShape ( QTabBar::TriangularBelow );

	if ( m_tabbar-> style ( ). isA ( "QWindowsXPStyle" ))
		m_tabbar-> setStyle ( new QWindowsStyle ( ));
	*/

	m_tabbar-> hide ( );
	m_close-> hide ( );

	if ( m_mode == MDI )
		m_stack-> hide ( );
	else
		m_workspace-> hide ( );

	QBoxLayout *lay = new QVBoxLayout ( this, 0, -1 );
	lay-> addWidget ( m_workspace, 10 );

	if (( m_tabbar-> shape ( ) == QTabBar::RoundedBelow ) ||
		( m_tabbar-> shape ( ) == QTabBar::TriangularBelow )) {
		lay-> addWidget ( m_stack, 10 );
		lay-> addWidget ( m_tabbar );
	}
	else {
		QHBoxLayout *tablay = new QHBoxLayout ( lay );
		tablay-> addSpacing ( 4 );
		tablay-> addWidget ( m_tabbar );
		tablay-> addStretch ( 10 );
		tablay-> addSpacing ( 4 );
		tablay-> addWidget ( m_close );
		tablay-> addSpacing ( 4 );
		lay-> addWidget ( m_stack, 10 );
	}

	connect ( m_tabbar, SIGNAL( selected ( int )), this, SLOT( activateTabbed ( int )));
	connect ( m_workspace, SIGNAL( windowActivated ( QWidget * )), this, SIGNAL( windowActivated ( QWidget * )));
}

CWorkspace::Mode CWorkspace::mode ( ) const
{
	return m_mode;
}

void CWorkspace::setMode ( Mode m )
{
	if ( m == m_mode )
		return;

	QWidget *active = activeWindow ( );
	m_mode = m;

	for ( QWidgetListIt it ( m_windows ); it. current ( ); ++it ) {
		QWidget *w = it. current ( );

		if ( m == MDI ) { // from Tabbed to MDI
			m_stack-> removeWidget ( w );
			w-> reparent ( m_workspace, QPoint ( 0, 0 ));
		}
		else { // from MDI to Tabbed
			m_stack-> addWidget ( w );
		}
	}

	if ( m == MDI ) {
		m_tabbar-> hide ( ); 
		m_stack-> hide ( );
		m_close-> hide ( );
		m_workspace-> show ( );
	}
	else {
		m_workspace-> hide ( );
		m_stack-> show ( );

		checkTabBarVisible ( );
	}
	activateWindow ( active );
}


bool CWorkspace::eventFilter( QObject *o, QEvent *e )
{
	if ( !o || !o-> isWidgetType ( ))
		return false;

	QWidget *w = static_cast <QWidget *> ( o );
	bool result = false;

	switch ( e-> type ( )) {
		case QEvent::CaptionChange: {
			QTab *t = m_window2tab [w];

			if ( t ) 
				t-> setText ( w-> caption ( ));
			break;
		}
		case QEvent::Close: {
			if ( m_block_close )
				break;
			m_block_close = true;

			QApplication::sendEvent ( w, e );
			result = true;

			if ( static_cast <QCloseEvent *> ( e )-> isAccepted ( ))
				removeAndActivateNext ( w );
			m_block_close = false;
			break;
		}
		default:
			break;
	}
	return result ? result : QWidget::eventFilter ( o, e );
}

void CWorkspace::childEvent( QChildEvent * e )
{
    if ( e-> inserted ( ) && e-> child ( )-> isWidgetType ( )) {
		QWidget* w = static_cast <QWidget *> ( e-> child ( ));
		
		if ( !w || !w-> testWFlags ( WStyle_Title | WStyle_NormalBorder | WStyle_DialogBorder ) || 
		   ( w == m_tabbar ) || ( w == m_stack ) || ( w == m_workspace ) || ( w == m_close ))
		    return;

		QTab *t = new QTab ( );
		t-> setText ( w-> caption ( ));
		m_tabbar-> addTab ( t );
		m_windows. append ( w );
		m_window2tab. insert ( w, t );
		m_tab2window. insert ( t, w );

		checkTabBarVisible ( );

		w-> installEventFilter ( this );
		connect ( w, SIGNAL( destroyed( )), this, SLOT( childDestroyed ( )));
	
		m_reparenting = true;

		if ( m_mode == MDI ) {
			w-> reparent ( m_workspace, QPoint ( 0, 0 ));
		}
		else {
			m_stack-> addWidget ( w );
			m_stack-> raiseWidget ( w );
			m_tabbar-> setCurrentTab ( m_window2tab [w] );
			emit windowActivated ( w );
		}
		m_reparenting = false;

		int s = m_tabbar-> sizeHint ( ). height ( );
		m_close-> setFixedSize ( s, s );
		m_close-> setEnabled ( true );
    }
}

void CWorkspace::childDestroyed ( )
{
	removeAndActivateNext ( static_cast <QWidget *> ( const_cast <QObject *> ( sender ( ))));
}

void CWorkspace::closeTabClicked ( )
{
	if (( m_mode == Tabbed ) && ( m_stack-> visibleWidget ( )))
		m_stack-> visibleWidget ( )-> close ( );
}

void CWorkspace::removeAndActivateNext ( QWidget *w )
{
	QTab *t = m_window2tab [w];

	if ( w && t ) {
		int oldpos = m_tabbar-> indexOf ( t-> identifier ( ));

		m_windows. removeRef ( w );
		m_window2tab. remove ( w );
		m_tab2window. remove ( t );
		m_tabbar-> removeTab ( t );

		if ( m_mode == Tabbed )
			m_stack-> removeWidget ( w );

		int count = m_tabbar-> count ( );

		t = ( count == 0 ) ? 0 : m_tabbar-> tabAt (( oldpos == count ) ? ( count - 1 ) : ( oldpos ));

		checkTabBarVisible ( );

		activateTabbed ( t ? t-> identifier ( ) : -1 );
	}
}

void CWorkspace::activateTabbed ( int id )
{
	QTab *t = m_tabbar-> tab ( id );

	if ( t && ( m_mode == Tabbed )) {
		QWidget *w = m_tab2window [t];

		if ( w ) {
			m_stack-> raiseWidget ( w );
			w-> setFocus ( );
			emit windowActivated ( w );
			m_close-> setEnabled ( true );
			return;
		}
	}
	emit windowActivated ( 0 );
	m_close-> setEnabled ( false );
	m_close-> setFixedSize ( 0, 0 );
}

QWidget *CWorkspace::activeWindow ( ) const
{
	if ( m_mode == MDI )
		return m_workspace-> activeWindow ( );
	else
		return m_stack-> visibleWidget ( );
}

void CWorkspace::activateWindow ( QWidget *w )
{
	QTab *t = m_window2tab [w];

	if ( w && t ) {
		if ( m_mode == MDI ) {
			w-> showNormal ( );
		}
		else {
			if ( m_tabbar-> isVisible ( ))
				m_tabbar-> setCurrentTab ( t );
			else
				activateTabbed ( t-> identifier ( ));
		}
		w-> setFocus ( );
	}
}

QWidgetList CWorkspace::windowList ( WindowOrder order ) const
{
	if ( m_mode == MDI ) {
		return m_workspace-> windowList ( order == CreationOrder ? QWorkspace::CreationOrder : QWorkspace::StackingOrder );
	}
	else {
		if ( order == CreationOrder ) {
			return m_windows;
		}
		else {
			QWidgetList wlist = m_windows;

			QWidget *top = m_stack-> visibleWidget ( );
			wlist. removeRef ( top );
			wlist. append ( top );

			return wlist;
		}
	}
}

void CWorkspace::cascade ( )
{
	if ( m_mode == MDI )
		m_workspace-> cascade ( );
}

void CWorkspace::tile ( )
{
	if ( m_mode == MDI )
		m_workspace-> tile ( );
}

void CWorkspace::closeActiveWindow ( )
{
	if ( m_mode == MDI ) {
		m_workspace-> closeActiveWindow ( );
	}
	else {
		if ( m_stack-> visibleWidget ( ))
			m_stack-> visibleWidget ( )-> close ( );
	}
}

void CWorkspace::closeAllWindows ( )
{
	if ( m_mode == MDI ) {
		m_workspace-> closeAllWindows ( );
	}
	else {
		QWidgetList wl = m_windows;

		while ( !wl. isEmpty ( )) {
			if ( !wl. take ( 0 )-> close ( ))
				return;
		}
	}
}

void CWorkspace::activateNextWindow ( )
{
	if ( m_mode == MDI ) {
		m_workspace-> activateNextWindow ( );
	}
	else {
		if ( activeWindow ( ) && ( m_windows. find ( activeWindow ( )) != -1 )) {
			QWidget *w = m_windows. next ( );

			if ( w ) {
				m_stack-> raiseWidget ( w );
				emit windowActivated ( w );
			}
		}
	}
}

void CWorkspace::activatePrevWindow ( )
{
	if ( m_mode == MDI ) {
		m_workspace-> activatePrevWindow ( );
	}
	else {
		if ( activeWindow ( ) && ( m_windows. find ( activeWindow ( )) != -1 )) {
			QWidget *w = m_windows. prev ( );

			if ( w ) {
				m_stack-> raiseWidget ( w );
				emit windowActivated ( w );
			}
		}
	}
}

void CWorkspace::checkTabBarVisible ( )
{
	bool b = (( m_mode == Tabbed ) && ( m_windows. count ( ) > 1 ));

	if ( b ) {
		m_tabbar-> show ( );
		m_close-> show ( );
	}
	else {
		m_tabbar-> hide ( );
		m_close-> hide ( );
	}
}
