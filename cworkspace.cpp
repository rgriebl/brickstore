#include <qlayout.h>
#include <qworkspace.h>
#include <qwidgetstack.h>
#include <qtabbar.h>
#include <qlayout.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qstylefactory.h>
#include <qstyle.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qmainwindow.h>

#include "cresource.h"

#include "cworkspace.h"


namespace {

static QStyle *windowsstyle = 0;

static void free_win_style ( )
{
	delete windowsstyle;
}

static QStyle *get_win_style ( )
{
	if ( !windowsstyle ) {
		windowsstyle = QStyleFactory::create ( "windows" );

		if ( windowsstyle )
			atexit ( free_win_style );
	}
	return windowsstyle;
}

} // namespace

CWorkspace::CWorkspace ( QMainWindow *parent, const char *name )
	: QWorkspace ( parent, name )
{
	m_mainwindow = parent;
	m_showtabs   = false;
	m_exceltabs  = false;

	m_tabbar = new QTabBar ( parent, "tabbar" );
	m_tabbar-> hide ( );

	connect ( m_tabbar, SIGNAL( selected ( int )), this, SLOT( tabClicked ( int )));
	connect ( this, SIGNAL( windowActivated ( QWidget * )), this, SLOT( setActiveTab ( QWidget * )));
}

bool CWorkspace::showTabs ( ) const
{
	return m_showtabs;
}

void CWorkspace::setShowTabs ( bool b )
{
	if ( b != m_showtabs ) {
		if ( b ) {
			QWidget *container = new QWidget ( m_mainwindow, "cworkspace_container" );
			refillContainer ( container );

			show ( );
			if ( m_tabbar-> count ( ) > 1 )
				m_tabbar-> show ( );

			m_mainwindow-> setCentralWidget ( container );
			container-> show ( );
		}
		else {
			QWidget *container = parentWidget ( );
			m_tabbar-> hide ( );
			m_tabbar-> reparent ( m_mainwindow, QPoint ( ), false );
			reparent ( m_mainwindow, m_mainwindow-> centralWidget ( )-> pos ( ), false );
			m_mainwindow-> setCentralWidget ( this );
			show ( );
			delete container;
		}
		m_showtabs = b;
	}
}

void CWorkspace::refillContainer ( QWidget *container )
{
	delete container-> layout ( );
	QBoxLayout *lay = new QVBoxLayout ( container, 0, 0 );

	for ( int i = 0; i < 2; i++ ) {
		if ( i == ( m_exceltabs ? 0 : 1 )) {
			if ( parentWidget ( ) != container )
				reparent ( container, QPoint ( 0, 0 ), false );
			lay-> addWidget ( this, 10 );
		}
		else {
			QHBoxLayout *tablay = new QHBoxLayout ( lay );
			tablay-> addSpacing ( 4 );
			if ( m_tabbar-> parentWidget ( ) != container )
				m_tabbar-> reparent ( container, QPoint ( 0, 0 ), false );
			tablay-> addWidget ( m_tabbar );
			tablay-> addSpacing ( 4 );
		}
	}
	lay-> activate ( );
}

bool CWorkspace::spreadSheetTabs ( ) const
{
	return m_exceltabs;
}

void CWorkspace::setSpreadSheetTabs ( bool b )
{
	if ( b != m_exceltabs ) {
		if ( b ) {
			m_tabbar-> setShape ( QTabBar::TriangularBelow );

			if ( m_tabbar-> style ( ). isA ( "QWindowsXPStyle" ))
				m_tabbar-> setStyle ( get_win_style ( ));
		}
		else {
			m_tabbar-> setShape ( QTabBar::RoundedAbove );

			if ( m_tabbar-> style ( ). isA ( "QWindowsStyle" ))
				m_tabbar-> setStyle ( &style ( ));
		}
		m_exceltabs = b;

		if ( m_showtabs )
			refillContainer ( parentWidget ( ));
	}
}
void CWorkspace::tabClicked ( int id )
{
	QTab *t = m_tabbar-> tab ( id );

	for ( QPtrDictIterator<QTab> it ( m_widget2tab ); it. current ( ); ++it ) {
		if ( t == it. current ( )) {
			QWidget *w = static_cast <QWidget *> ( it. currentKey ( ));
	
			if ( w != activeWindow ( ))
				w-> setFocus ( );
			if ( !w-> isMaximized ( ))
				w-> showMaximized ( );
		}
	}
}

void CWorkspace::setActiveTab ( QWidget * w )
{
	QTab *t = m_widget2tab [w];

	if ( t && ( m_tabbar-> currentTab ( ) != t-> identifier ( ))) {
		m_tabbar-> blockSignals ( true );
		m_tabbar-> setCurrentTab ( t );
		m_tabbar-> blockSignals ( false );
	}
}

bool CWorkspace::eventFilter( QObject *o, QEvent *e )
{
	if ( !o || !o-> isWidgetType ( ))
		return false;

	QWidget *w = static_cast <QWidget *> ( o );

	bool result = false;

	switch ( e-> type ( )) {
		case QEvent::CaptionChange: {
			QTab *t = m_widget2tab [w];

			if ( t ) 
				t-> setText ( w-> caption ( ));
			break;
		}
		default:
			break;
	}
	return result ? result : QWorkspace::eventFilter ( o, e );
}


void CWorkspace::childEvent( QChildEvent * e )
{
	QWorkspace::childEvent ( e );

	QWidgetList after = windowList ( CreationOrder );

	if ( e-> inserted ( )) {
		for ( QWidgetListIt it ( after ); it. current ( ); ++it ) {
			QWidget *w = it. current ( );
			QTab *t = m_widget2tab [w];

			if ( w && !t ) { // new
				w-> installEventFilter ( this );

				t = new QTab ( w-> caption ( ));
				m_tabbar-> addTab ( t );
				m_widget2tab.insert ( w, t );
			}
		}
	}
	else {
		QPtrDict <QTab> copy_of_widget2tab = m_widget2tab;

		for ( QPtrDictIterator<QTab> it ( copy_of_widget2tab ); it. current ( ); ++it ) {
			QWidget *w = static_cast <QWidget *> ( it. currentKey ( ));
			QTab *t = it. current ( );

			if ( w && t && ( after. findRef ( w ) == -1 )) {
				m_tabbar-> removeTab ( t );
				m_widget2tab. remove ( w );
			}
		}
	}
	if ( m_showtabs )
		m_tabbar-> setShown ( m_tabbar-> count ( ) > 1 );
}
