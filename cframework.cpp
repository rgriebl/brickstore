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
#include <float.h>

#include <qaction.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qstatusbar.h>
#include <qwidgetlist.h>
#include <qworkspace.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qobjectlist.h>
#include <qtooltip.h>
#include <qcursor.h>

#include "capplication.h"
#include "cmessagebox.h"
#include "cwindow.h"
#include "cconfig.h"
#include "cmoney.h"
#include "cresource.h"
#include "cmultiprogressbar.h"
#include "cinfobar.h"
#include "ciconfactory.h"
#include "dlgsettingsimpl.h"
#include "dlgdbupdateimpl.h"
#include "cutility.h"
#include "cspinner.h"

#include "cframework.h"


namespace {

enum ProgressItem {
	PGI_Inventory,
	PGI_PriceGuide,
	PGI_Picture
};

} // namespace


CFrameWork::RecentListProvider::RecentListProvider ( CFrameWork *fw )
	: CListAction::Provider ( ), m_fw ( fw )
{ }

CFrameWork::RecentListProvider::~RecentListProvider ( )
{ }

QStringList CFrameWork::RecentListProvider::list ( int & /*active*/ )
{
	return m_fw-> m_recent_files;
}


CFrameWork::WindowListProvider::WindowListProvider ( CFrameWork *fw )
	: CListAction::Provider ( ), m_fw ( fw )
{ }

CFrameWork::WindowListProvider::~WindowListProvider ( )
{ }

QStringList CFrameWork::WindowListProvider::list ( int &active )
{
	QStringList sl;
	QWidgetList l = m_fw-> m_mdi-> windowList ( QWorkspace::CreationOrder );
	QWidget *aw = m_fw-> m_mdi-> activeWindow ( );

	for ( uint i = 0; i < l. count ( ); i++ ) {
		QWidget *w = l. at ( i );

		sl << w-> caption ( );

		if ( w == aw )
			active = i;
	}
	return sl;
}

CFrameWork *CFrameWork::s_inst = 0;

CFrameWork *CFrameWork::inst ( )
{
	if ( !s_inst )
		s_inst = new CFrameWork ( 0, "FrameWork", Qt::WDestructiveClose );

	return s_inst;
}


CFrameWork::CFrameWork ( QWidget *parent, const char *name, WFlags fl )
	: QMainWindow ( parent, name, fl )
{
	m_running = false;

	qApp-> setMainWidget ( this );

#if defined( Q_WS_X11 )
 	if ( !icon ( ) || icon ( )-> isNull ( )) {
		QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
		if ( !pix. isNull ( ))
			setIcon ( pix );
	}
#endif
	
	setCaption ( tr( "BrickStore" ));
	setUsesBigPixmaps ( true );

	(void) new CIconFactory ( );

	connect ( cApp, SIGNAL( openDocument ( const QString & )), this, SLOT( openDocument ( const QString & )));

	m_recent_files = CConfig::inst ( )-> readListEntry ( "/Files/Recent" );

	while ( m_recent_files. count ( ) > MaxRecentFiles )
		m_recent_files. pop_back ( );

	m_current_window = 0;

	m_mdi = new QWorkspace ( this );
	connect ( m_mdi, SIGNAL( windowActivated ( QWidget * )), this, SLOT( connectWindow ( QWidget * )));

	setCentralWidget ( m_mdi );

	createActions ( );

	QString str;
	QStringList sl;

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/File" );
	if ( sl. isEmpty ( ))  sl << "file_new" << "file_open" << "file_open_recent" << "-" << "file_save" << "file_saveas" << "-" << "file_import" << "file_export" << "-" << "file_print" << "-" << "file_close" << "-" << "file_exit";
	createMenu ( AC_File, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Edit" );
	if ( sl. isEmpty ( ))  sl << "edit_cut" << "edit_copy" << "edit_paste" << "edit_delete" << "-" << "edit_select_all" << "edit_select_none" << "-" 
		                      << "edit_additems" << "edit_subtractitems" << "edit_mergeitems" << "edit_partoutitems" << "-" 
							  << "edit_multiply_qty" << "edit_divide_qty" << "edit_price_to_priceguide" << "edit_price_inc_dec" << "edit_set_sale" << "edit_set_condition" << "edit_set_remark" << "edit_set_reserved" << "-" 
							  << "edit_reset_diffs" << "-"
							  << "edit_bl_catalog" << "edit_bl_priceguide" << "edit_bl_lotsforsale";
	createMenu ( AC_Edit, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/View" );
	if ( sl. isEmpty ( ))  sl << "settings_view_toolbar" << "settings_view_infobar" << "settings_view_statusbar" << "-" << "view_simple_mode" << "view_show_input_errors" << "-" << "view_difference_mode"; 
	createMenu ( AC_View, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Extras" );
	if ( sl. isEmpty ( ))  sl << "extras_net" << "-" << "extras_update_database" << "-" << "extras_configure";
	// Make sure there is a possibility to open the pref dialog!
	if ( sl. find ( "extras_configure" ) == sl. end ( ))
		sl << "-" << "extras_configure";
	createMenu ( AC_Extras, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Window" );
	if ( sl. isEmpty ( ))  sl << "window_cascade" << "window_tile" << "-" << "window_list";
	createMenu ( AC_Window, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Help" );
	if ( sl. isEmpty ( ))  sl << "help_updates" << "-" << "help_about";
	createMenu ( AC_Help, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/ContextMenu/Item" );
	if ( sl. isEmpty ( ))  sl << "edit_cut" << "edit_copy" << "edit_paste" << "edit_delete" << "-" << "edit_select_all" << "-" << "edit_mergeitems" << "edit_partoutitems" << "-" << "edit_multiply_qty" << "edit_price_to_priceguide" << "edit_price_inc_dec" << "edit_set_sale" << "edit_set_condition" << "edit_set_remark" << "-" << "edit_bl_catalog" << "edit_bl_priceguide" << "edit_bl_lotsforsale";
	createMenu ( AC_Context, sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Toolbar/Buttons" );
	if ( sl. isEmpty ( ))  sl << "file_new" << "file_open" << "file_save" << "-" << "file_import" << "file_export" << "-" << "edit_cut" << "edit_copy" << "edit_paste" << "-" << "edit_additems" << "edit_subtractitems" << "edit_mergeitems" << "edit_partoutitems" << "-" << "edit_price_to_priceguide" << "edit_price_inc_dec" << "-" << "edit_bl_catalog" << "edit_bl_priceguide" << "edit_bl_lotsforsale" << "-" << "extras_net"; // << "-" << "help_whatsthis";
	m_toolbar = createToolBar ( tr( "Toolbar" ), sl );
	connect ( m_toolbar, SIGNAL( visibilityChanged ( bool )), findAction ( "settings_view_toolbar" ), SLOT( setOn ( bool )));

	createStatusBar ( );

	connect ( m_progress, SIGNAL( statusChange ( bool )), m_spinner, SLOT( setActive ( bool )));

	m_infobar = new CInfoBar ( tr( "Infobar" ), this );
	connect ( m_infobar, SIGNAL( visibilityChanged ( bool )), findAction ( "settings_view_infobar" ), SLOT( setOn ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( infoBarLookChanged ( int )), m_infobar, SLOT( setLook ( int )));
	m_infobar-> hide ( );

	str = CConfig::inst ( )-> readEntry ( "/MainWindow/Layout/DockWindows" );
	{ QTextStream ts ( &str, IO_ReadOnly ); ts >> *this; }

	moveDockWindow ( m_infobar, DockLeft );
	m_infobar-> setLook ( CInfoBar::Look ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Infobar/Look", CInfoBar::ModernLeft )));

	QRect r ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Left",   -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Top",    -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Width",  -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Height", -1 ));

	int wstate = CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/State", WindowNoState );

	// make sure at least SOME part of the window ends up on the screen
	if ( r. isValid ( ) && !r. isEmpty ( ) && !( r & qApp-> desktop ( )-> rect ( )). isEmpty ( )) {
		resize ( r. size ( ));  // X11 compat. mode -- KDE 3.x doesn't seem to like this though...
		move ( r. topLeft ( ));
		//setGeometry ( r );  // simple code (which shouldn't work on X11, but does on KDE 3.x...)
	}
	m_normal_geometry = QRect ( pos ( ), size ( ));

	setWindowState ( wstate & ( WindowMinimized | WindowMaximized | WindowFullScreen ));


	findAction ( "settings_view_toolbar"   )-> setOn ( CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Toolbar/Visible",   true ));
	findAction ( "settings_view_infobar"   )-> setOn ( CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Infobar/Visible",   true ));
	findAction ( "settings_view_statusbar" )-> setOn ( CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Statusbar/Visible", true ));

	findAction ( "view_show_input_errors"  )-> setOn ( CConfig::inst ( )-> showInputErrors ( ));
	findAction ( "view_simple_mode"        )-> setOn ( CConfig::inst ( )-> simpleMode ( ));

	findAction ( CConfig::inst ( )-> onlineStatus ( ) ? "net_online" : "net_offline" )-> setOn ( true );
	

	BrickLink *bl = BrickLink::inst ( );

	bl-> setOnlineStatus ( CConfig::inst ( )-> onlineStatus ( ));
	bl-> setHttpProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
	{
		int db, inv, pic, pg;
		CConfig::inst ( )-> blUpdateIntervals ( db, inv, pic, pg );
		bl-> setUpdateIntervals ( db, inv, pic, pg );
	}

	connect ( CConfig::inst ( ), SIGNAL( onlineStatusChanged ( bool )), bl, SLOT( setOnlineStatus ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( blUpdateIntervalsChanged ( int, int, int, int )), bl, SLOT( setUpdateIntervals ( int, int, int, int )));
	connect ( CConfig::inst ( ), SIGNAL( proxyChanged ( bool, const QString &, int )), bl, SLOT( setHttpProxy ( bool, const QString &, int )));

	connect ( bl, SIGNAL( inventoryProgress ( int, int )),  this, SLOT( gotInventoryProgress ( int, int )));
	connect ( bl, SIGNAL( priceGuideProgress ( int, int )), this, SLOT( gotPriceGuideProgress ( int, int )));
	connect ( bl, SIGNAL( pictureProgress ( int, int )),    this, SLOT( gotPictureProgress ( int, int )));

	QTimer::singleShot ( 0, this, SLOT( initBrickLinkDelayed ( )));

	connectAllActions ( false, 0 ); // init enabled/disabled status of document actions

	setAcceptDrops ( true );
	
	m_running = true;
}

CFrameWork::~CFrameWork ( )
{
	CConfig::inst ( )-> writeEntry ( "/Files/Recent", m_recent_files );
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Toolbar/Visible", findAction ( "settings_view_toolbar" )-> isOn ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/Visible", findAction ( "settings_view_infobar" )-> isOn ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Statusbar/Visible", findAction ( "settings_view_statusbar" )-> isOn ( ));

	QString str;
	{ QTextStream ts ( &str, IO_WriteOnly ); ts << *this; }

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/DockWindows", str );

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Left",   m_normal_geometry. x ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Top",    m_normal_geometry. y ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Width",  m_normal_geometry. width ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Height", m_normal_geometry. height ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/State",  (int) windowState ( ));

	s_inst = 0;
}

void CFrameWork::dragMoveEvent ( QDragMoveEvent *e )
{
	e-> acceptAction ( e-> action ( ) == QDropEvent::Copy );
}

void CFrameWork::dragEnterEvent ( QDragEnterEvent *e )
{
	if ( QUriDrag::canDecode ( e ))
		e-> accept ( );
}

void CFrameWork::dropEvent ( QDropEvent *e )
{
	QStringList sl;

	if ( QUriDrag::decodeLocalFiles( e, sl )) {
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it ) {
			openDocument ( *it );
		}
	}
}

void CFrameWork::moveEvent ( QMoveEvent * )
{
	if (!( windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
		m_normal_geometry. setTopLeft ( pos ( ));
}

void CFrameWork::resizeEvent ( QResizeEvent * )
{
	if (!( windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
		m_normal_geometry. setSize ( size ( ));
}

void CFrameWork::initBrickLinkDelayed ( )
{
	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	bool dbok = BrickLink::inst ( )-> readDatabase ( );

	QApplication::restoreOverrideCursor ( );		

	if ( !dbok ) {
		if ( CMessageBox::warning ( this, tr( "Could not load the BrickLink database files.<br /><br />Should these files be updated now?" ), CMessageBox::Yes, CMessageBox::No ) == CMessageBox::Yes ) {
			DlgDBUpdateImpl dbu ( this );
			dbu. exec ( );
			dbok = !dbu. errors ( );
		}
	}

	if ( dbok )
		cApp-> enableEmitOpenDocument ( );
	else
		CMessageBox::warning ( this, tr( "Could not load the BrickLink database files.<br /><br />The program is not functional without these files." ));
}

QAction *CFrameWork::findAction ( const char *name, ActionCategory cat )
{
	if ( cat < 0 || cat > AC_Count || !name )
		return 0;

	for ( int i = 0; i < AC_Count; i++ ) {
		if (( cat == AC_Count ) || ( cat == i )) {
			for ( QPtrListIterator<QAction> it ( m_actions [i] ); it. current ( ); ++it ) {
				const char *a_name = it. current ( )-> name ( );

				if ( a_name && ( qstrcmp ( name, a_name ) == 0 ))
					return it. current ( );
			}
		}
	}
	return 0;
}

void CFrameWork::createStatusBar ( )
{
	m_errors = new QLabel ( statusBar ( ));
	statusBar ( )-> addWidget ( m_errors, 0, true );

	m_statistics = new QLabel ( statusBar ( ));
	statusBar ( )-> addWidget ( m_statistics, 0, true );

	m_modified = new QLabel ( statusBar ( ));
	statusBar ( )-> addWidget ( m_modified, 0, true );

	m_progress = new CMultiProgressBar ( statusBar ( ));
	m_progress-> setFixedWidth ( fontMetrics ( ). height ( ) * 10 );
	m_progress-> setFixedHeight ( fontMetrics ( ). height ( ));

	QPixmap p;
	p = CResource::inst ( )-> pixmap ( "status/popup" );
	if ( !p. isNull ( ))
		m_progress-> setPopupPixmap ( p );
	p = CResource::inst ( )-> pixmap ( "status/stop" );
	if ( !p. isNull ( ))
		m_progress-> setStopPixmap ( p );

	m_progress-> addItem ( tr( "Priceguide updates" ), PGI_PriceGuide );
	m_progress-> addItem ( tr( "Inventory updates" ),  PGI_Inventory  );
	m_progress-> addItem ( tr( "Picture updates" ),    PGI_Picture    );

	//m_progress-> setProgress ( -1, 100 );

	statusBar ( )-> addWidget ( m_progress, 0, true );

	connect ( m_progress, SIGNAL( stop ( )), this, SLOT( cancelAllTransfers ( )));

	statusBar ( )-> hide ( );
}

QPopupMenu *CFrameWork::createMenu ( ActionCategory cat, const QStringList &a_names )
{
	if ( a_names. isEmpty ( ))
		return 0;

	QString name;

	switch ( cat ) {
		case AC_File    : name = tr( "&File" ); break;
		case AC_Edit    : name = tr( "&Edit" ); break;
		case AC_View    : name = tr( "&View" ); break;
		case AC_Extras  : name = tr( "E&xtras" ); break;
		case AC_Window  : name = tr( "&Windows" ); break;
		case AC_Help    : name = tr( "&Help" ); break;
		case AC_Context : name = "dummy";
		case AC_Count   : break;
	}

	if ( !name )
		return 0;

	QPopupMenu *p = new QPopupMenu ( this );
	p-> setCheckable ( true );

	for ( QStringList::ConstIterator it = a_names. begin ( ); it != a_names. end ( ); ++it ) {
		const char *a_name = ( *it ). latin1 ( );

		if ( a_name [0] == '-' && a_name [1] == 0 ) {
			p-> insertSeparator ( );
		}
		else {
			QAction *a = findAction ( a_name, cat );

			if ( !a )
				a = findAction ( a_name );

			if ( a )
				a-> addTo ( p );
			else
				qWarning ( "Couldn't find action '%s'", a_name );
		}
	}

	if ( cat == AC_Context )
		m_contextmenu = p;
	else
		menuBar ( )-> insertItem ( name, p, cat );
	return p;
}

QToolBar *CFrameWork::createToolBar ( const QString &label, const QStringList &a_names )
{
	if ( a_names. isEmpty ( ))
		return 0;

	QToolBar *t = new QToolBar ( this );
	t-> setLabel ( label );

	for ( QStringList::ConstIterator it = a_names. begin ( ); it != a_names. end ( ); ++it ) {
		const char *a_name = ( *it ). latin1 ( );

		if ( a_name [0] == '-' && a_name [1] == 0 ) {
			t-> addSeparator ( );
		}
		else {
			QAction *a = findAction ( a_name );

			if ( a )
				a-> addTo ( t );
		}
	}

	t-> setStretchableWidget ( new QWidget ( t ));
	t-> setHorizontallyStretchable ( true );
	t-> setVerticallyStretchable ( true );

	QWidget *sw1 = new QWidget ( t );
	sw1-> setFixedSize ( 4, 4 );

	m_spinner = new CSpinner ( t, "spinner" );
	m_spinner-> setPixmap ( CResource::inst ( )-> pixmap ( "spinner" ));
	m_spinner-> show ( );

	QWidget *sw2 = new QWidget ( t );
	sw2-> setFixedSize ( 4, 4 );

	t-> hide ( );
	return t;
}

void CFrameWork::createActions ( )
{
	QAction *a;
	CListAction *l;
	QActionGroup *g;

	a = new QAction ( this, "file_new" );
	a-> setText ( tr( "New..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+N", "File|New" )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileNew ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "file_open" );
	a-> setText ( tr( "Open..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+O", "File|Open" )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileOpen ( )));
	m_actions [AC_File]. append ( a );

	l = new CListAction ( this, "file_open_recent" );
	l-> setText ( tr( "Open Recent" ));
	l-> setUsesDropDown ( true );
	l-> setListProvider ( new RecentListProvider ( this ));
	connect ( l, SIGNAL( activated ( int )), this, SLOT( fileOpenRecent ( int )));
	m_actions [AC_File]. append ( l );

	a = new QAction ( this, "file_save" );
	a-> setText ( tr( "Save" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+S", "File|Save" )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "file_saveas" );
	a-> setText ( tr( "Save As..." ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "file_print" );
	a-> setText ( tr( "Print..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+P", "File|Print" )));
	m_actions [AC_File]. append ( a );


	g = new QActionGroup ( this, "file_import" );
	g-> setText ( tr( "Import" ));
	g-> setExclusive ( false );
	g-> setUsesDropDown ( true );
	m_actions [AC_File]. append ( g );

	a = new QAction ( g, "file_import_bl_inventory" );
	a-> setText ( tr( "BrickLink Inventory..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkInventory ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_import_bl_xml" );
	a-> setText ( tr( "BrickLink XML..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkXML ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_import_bl_order" );
	a-> setText ( tr( "BrickLink Order..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkOrder ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_import_bl_store_inventory" );
	a-> setText ( tr( "BrickLink Store Inventory..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkStore ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_import_ldraw_model" );
	a-> setText ( tr( "LDraw Model..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportLDrawModel ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_import_briktrak" );
	a-> setText ( tr( "BrikTrak Inventory..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrikTrakInventory ( )));
	m_actions [AC_File]. append ( a );

	g = new QActionGroup ( this, "file_export" );
	g-> setText ( tr( "Export" ));
	g-> setExclusive ( false );
	g-> setUsesDropDown ( true );
	m_actions [AC_File]. append ( g );

	a = new QAction ( g, "file_export_bl_xml" );
	a-> setText ( tr( "BrickLink XML..." ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_export_bl_xml_clip" );
	a-> setText ( tr( "BrickLink XML to Clipboard" ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_export_bl_update_clip" );
	a-> setText ( tr( "BrickLink Mass-Update XML to Clipboard" ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_export_bl_invreq_clip" );
	a-> setText ( tr( "BrickLink Inventory XML to Clipboard" ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_export_bl_wantedlist_clip" );
	a-> setText ( tr( "BrickLink Wanted List XML to Clipboard" ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( g, "file_export_briktrak" );
	a-> setText ( tr( "BrikTrak Inventory..." ));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "file_close" );
	a-> setText ( tr( "Close" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+W", "File|Close" )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "file_exit" );
	a-> setText ( tr( "Exit" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+Q", "File|Quit" )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( close ( )));
	m_actions [AC_File]. append ( a );

	a = new QAction ( this, "edit_undo" );
	a-> setText ( tr( "Undo" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+Z", "Edit|Undo" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_redo" );
	a-> setText ( tr( "Redo" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+Y", "Edit|Redo" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_cut" );
	a-> setText ( tr( "Cut" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+X", "Edit|Cut" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_copy" );
	a-> setText ( tr( "Copy" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+C", "Edit|Copy" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_paste" );
	a-> setText ( tr( "Paste" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+V", "Edit|Paste" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_delete" );
	a-> setText ( tr( "Delete" ));
	a-> setAccel ( QKeySequence ( tr( "Delete", "Edit|Delete" ))); //TODO: this is dangerous: implement an undo function
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_additems" );
	a-> setText ( tr( "Add Items..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+I", "Edit|AddItems" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_subtractitems" );
	a-> setText ( tr( "Subtract Items..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_mergeitems" );
	a-> setText ( tr( "Consolidate Items..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_partoutitems" );
	a-> setText ( tr( "Part out Item..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_reset_diffs" );
	a-> setText ( tr( "Reset Differences" ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_select_all" );
	a-> setText ( tr( "Select All" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+A", "Edit|SelectAll" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_select_none" );
	a-> setText ( tr( "Select None" ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+Shift+A", "Edit|SelectNone" )));
	m_actions [AC_Edit]. append ( a );


	a = new QAction ( this, "view_simple_mode" );
	a-> setText ( tr( "Buyer/Collector View" ));
	a-> setToggleAction ( true );
	connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setSimpleMode ( bool )));
	m_actions [AC_View]. append ( a );

	a = new QAction ( this, "settings_view_toolbar" );
	a-> setText ( tr( "View Toolbar" ));
	a-> setToggleAction ( true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewToolBar ( bool )));
	m_actions [AC_View]. append ( a );

	a = new QAction ( this, "settings_view_infobar" );
	a-> setText ( tr( "View Infobar" ));
	a-> setToggleAction ( true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewInfoBar ( bool )));
	m_actions [AC_View]. append ( a );

	a = new QAction ( this, "settings_view_statusbar" );
	a-> setText ( tr( "View Statusbar" ));
	a-> setToggleAction ( true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewStatusBar ( bool )));
	m_actions [AC_View]. append ( a );

	a = new QAction ( this, "view_show_input_errors" );
	a-> setText ( tr( "Show Input Errors" ));
	a-> setToggleAction ( true );
	connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setShowInputErrors ( bool )));
	m_actions [AC_View]. append ( a );

	a = new QAction ( this, "view_difference_mode" );
	a-> setText ( tr( "Difference Mode" ));
	a-> setToggleAction ( true );
	m_actions [AC_View]. append ( a );


	a = new QAction ( this, "extras_update_database" );
	a-> setText ( tr( "Update Database" ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( updateDatabase ( )));
	m_actions [AC_Extras]. append ( a );

	a = new QAction ( this, "extras_configure" );
	a-> setText ( tr( "Configure..." ));
	connect ( a, SIGNAL( activated ( )), this, SLOT( configure ( )));
	m_actions [AC_Extras]. append ( a );

	g = new QActionGroup ( this, "extras_net" );
	g-> setExclusive ( true );
	connect ( g, SIGNAL( selected ( QAction * )), this, SLOT( setOnlineStatus ( QAction * )));
	m_actions [AC_Extras]. append ( g );

	a = new QAction ( g, "net_online" );
	a-> setText ( tr( "Online Mode" ));
	a-> setToggleAction ( true );
	m_actions [AC_Extras]. append ( a );

	a = new QAction ( g, "net_offline" );
	a-> setText ( tr( "Offline Mode" ));
	a-> setToggleAction ( true );
	m_actions [AC_Extras]. append ( a );


	a = new QAction ( this, "window_cascade" );
	a-> setText ( tr( "Cascade" ));
	connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( cascade ( )));
	m_actions [AC_Window]. append ( a );

	a = new QAction ( this, "window_tile" );
	a-> setText ( tr( "Tile" ));
	connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( tile ( )));
	m_actions [AC_Window]. append ( a );

	l = new CListAction ( this, "window_list" );
	l-> setListProvider ( new WindowListProvider ( this ));
	connect ( l, SIGNAL( activated ( int )), this, SLOT( windowActivate ( int )));
	m_actions [AC_Window]. append ( l );


	a = new QAction ( this, "help_whatsthis" );
	a-> setText ( tr( "What's this?" ));
	a-> setAccel ( QKeySequence ( tr( "Shift+F1", "Help|WhatsThis" )));
	connect ( a, SIGNAL( activated ( )), this, SLOT( whatsThis ( )));
	m_actions [AC_Help]. append ( a );

	a = new QAction ( this, "help_about" );
	a-> setText ( tr( "About" ));
	connect ( a, SIGNAL( activated ( )), cApp, SLOT( about ( )));
	m_actions [AC_Help]. append ( a );

	a = new QAction ( this, "help_updates" );
	a-> setText ( tr( "Check for Program Updates..." ));
	connect ( a, SIGNAL( activated ( )), cApp, SLOT( checkForUpdates ( )));
	m_actions [AC_Help]. append ( a );


	a = new QAction ( this, "edit_price_to_fixed" );
	a-> setText ( tr( "Set Prices..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_price_to_priceguide" );
	a-> setText ( tr( "Set Prices to Price-Guide..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+G", "Edit|Set to PriceGuide" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_price_inc_dec" );
	a-> setText ( tr( "Inc- or Decrease Prices..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl++", "Edit| Inc/Dec Prices" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_set_sale" );
	a-> setText ( tr( "Set Sale..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+%", "Edit|Set Sale" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_set_remark" );
	a-> setText ( tr( "Set Remark..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_set_reserved" );
	a-> setText ( tr( "Set Reserved for..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_set_condition" );
	a-> setText ( tr( "Set Condition..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_multiply_qty" );
	a-> setText ( tr( "Multiply Quantities..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+*", "Edit|Multiply Quantities" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_divide_qty" );
	a-> setText ( tr( "Divide Quantities..." ));
	a-> setAccel ( QKeySequence ( tr( "Ctrl+/", "Edit|Divide Quantities" )));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_bl_catalog" );
	a-> setText ( tr( "Show BrickLink Catalog Info..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_bl_priceguide" );
	a-> setText ( tr( "Show BrickLink Price Guide Info..." ));
	m_actions [AC_Edit]. append ( a );

	a = new QAction ( this, "edit_bl_lotsforsale" );
	a-> setText ( tr( "Lots for Sale on BrickLink..." ));
	m_actions [AC_Edit]. append ( a );


	// set all icons that have a pixmap corresponding to name()

	for ( int i = 0; i < AC_Count; i++ ) {
		for ( QPtrListIterator<QAction> it ( m_actions [i] ); it. current ( ); ++it ) {
			const char *name = it. current ( )-> name ( );

			if ( name && name [0] ) {
				QIconSet set = CResource::inst ( )-> iconSet ( name );

				if ( !set. isNull ( ))
					it. current ( )-> setIconSet ( set );
			}
		}
	}
}


void CFrameWork::viewStatusBar ( bool b )
{
	statusBar ( )-> setShown ( b );
}

void CFrameWork::viewToolBar ( bool b )
{
	m_toolbar-> setShown ( b );
}

void CFrameWork::viewInfoBar ( bool b )
{
	m_infobar-> setShown ( b );
}

void CFrameWork::openDocument ( const QString &file )
{
	bool old_bti_file = ( file. right ( 4 ) == ".bti" );

	CWindow *w = createWindow ( );
	bool ok = showOrDeleteWindow ( w, old_bti_file ? w-> fileImportBrikTrakInventory ( file ) : w-> fileOpen ( file ));

	if ( old_bti_file && ok )
		CMessageBox::information ( this, tr( "BrickStore has switched to a new file format (.bsx - BrickStore XML).<br /><br />Your document has been automatically imported and it will be converted as soon as you save it." ));
}

void CFrameWork::fileNew ( )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, true );
}

void CFrameWork::fileOpen ( )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileOpen ( ));
}

void CFrameWork::fileOpenRecent ( int i )
{
	if ( i < int( m_recent_files. count ( ))) {
		// we need to copy the string here, because we delete it later
		// (this seems to work on Linux, but XP crashes in m_recent_files. remove ( ... )
		QString tmp = m_recent_files [i];

		openDocument ( tmp );
	}
}

void CFrameWork::fileImportBrikTrakInventory ( )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportBrikTrakInventory ( ));
}

void CFrameWork::fileImportBrickLinkInventory ( )
{
	fileImportBrickLinkInventory ( 0 );
}


void CFrameWork::fileImportBrickLinkInventory ( const BrickLink::Item *item )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportBrickLinkInventory ( item ));
}

bool CFrameWork::checkBrickLinkLogin ( )
{
	while ( CConfig::inst ( )-> blLoginUsername ( ). isEmpty ( ) ||
	        CConfig::inst ( )-> blLoginPassword ( ). isEmpty ( )) {
		if ( CMessageBox::question ( this, tr( "No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?" ), CMessageBox::Yes, CMessageBox::No ) == CMessageBox::Yes )
			configure ( "network" );
		else
			return false;
	}
	return true;
}

void CFrameWork::fileImportBrickLinkOrder ( )
{
	if ( !checkBrickLinkLogin ( ))
		return;

	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportBrickLinkOrder ( ));
}

void CFrameWork::fileImportBrickLinkStore ( )
{
	if ( !checkBrickLinkLogin ( ))
		return;

	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportBrickLinkStore ( ));
}

void CFrameWork::fileImportBrickLinkXML ( )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportBrickLinkXML ( ));
}

void CFrameWork::fileImportLDrawModel ( )
{
	CWindow *w = createWindow ( );
	showOrDeleteWindow ( w, w-> fileImportLDrawModel ( ));
}


CWindow *CFrameWork::createWindow ( )
{
	CWindow *w = new CWindow ( m_mdi );

	QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
	// Qt/Win32 bug: MDI child window icon()s have to be 16x16 or smaller...
	pix. convertFromImage ( pix. convertToImage ( ). smoothScale ( pix. size ( ). boundedTo ( QSize ( 16, 16 ))));
	if ( !pix. isNull ( ))
		w-> setIcon ( pix );

	return w;
}

bool CFrameWork::showOrDeleteWindow ( CWindow *w, bool b )
{
	if ( b ) {
		if ( !m_mdi-> activeWindow ( ) || ( m_mdi-> activeWindow ( ) == w ))
			w-> showMaximized ( );
		else
			w-> show ( );
	}
	else {
		if ( w == m_current_window ) // Qt/X11 (Win?,Mac?) Bug: window is active without being shown...
			connectWindow ( 0 );
		delete w;
	}
	return b;
}


void CFrameWork::updateDatabase ( )
{
	if ( closeAllWindows ( )) {
		DlgDBUpdateImpl d ( this );
		d. exec ( );
	}
}


void CFrameWork::windowActivate ( int i )
{
	QWidgetList l = m_mdi-> windowList ( QWorkspace/*CWindowManager*/::CreationOrder );

	if (( i >= 0 ) && ( i < int( l. count ( )))) {
		QWidget * w = l. at ( i );

		w-> showNormal ( );
		w-> setFocus ( );
	}
}

void CFrameWork::connectAction ( bool do_connect, const char *name, CWindow *window, const char *windowslot, bool ( CWindow::* is_on_func ) ( ) const )
{
	QAction *a = findAction ( name );

	if ( a )
		a-> setEnabled ( do_connect );

	if ( a && window ) {
		bool is_toggle = ( ::strstr ( windowslot, "bool" ));

		const char *actionsignal = is_toggle ? SIGNAL( toggled ( bool )) : SIGNAL( activated ( ));

		if ( do_connect )
			connect ( a, actionsignal, window, windowslot );
		else
			disconnect ( a, actionsignal, window, windowslot );

		if ( is_toggle && do_connect && is_on_func )
			m_toggle_updates. insert ( a, is_on_func );
	}
}

void CFrameWork::updateAllToggleActions ( CWindow *window )
{
	for ( QMap <QAction *, bool ( CWindow::* ) ( ) const>::iterator it = m_toggle_updates. begin ( ); it != m_toggle_updates. end ( ); ++it ) {
		it. key ( )-> setOn (( window->* it. data ( )) ( ));
	}
}

void CFrameWork::connectAllActions ( bool do_connect, CWindow *window )
{
	m_toggle_updates. clear ( );

	connectAction ( do_connect, "file_save", window, SLOT( fileSave ( )));
	connectAction ( do_connect, "file_saveas", window, SLOT( fileSaveAs ( )));
	connectAction ( do_connect, "file_print", window, SLOT( filePrint ( )));
	connectAction ( do_connect, "file_export_briktrak", window, SLOT( fileExportBrikTrakInventory ( )));
	connectAction ( do_connect, "file_export_bl_xml", window, SLOT( fileExportBrickLinkXML ( )));
	connectAction ( do_connect, "file_export_bl_xml_clip", window, SLOT( fileExportBrickLinkXMLClipboard ( )));
	connectAction ( do_connect, "file_export_bl_update_clip", window, SLOT( fileExportBrickLinkUpdateClipboard ( )));
	connectAction ( do_connect, "file_export_bl_invreq_clip", window, SLOT( fileExportBrickLinkInvReqClipboard ( )));
	connectAction ( do_connect, "file_export_bl_wantedlist_clip", window, SLOT( fileExportBrickLinkWantedListClipboard ( )));
	connectAction ( do_connect, "file_close", window, SLOT( close ( )));

	connectAction ( do_connect, "edit_undo", window, SLOT( editUndo ( )));
	connectAction ( do_connect, "edit_redo", window, SLOT( editRedo ( )));

	connectAction ( do_connect, "edit_cut", window, SLOT( editCut ( )));
	connectAction ( do_connect, "edit_copy", window, SLOT( editCopy ( )));
	connectAction ( do_connect, "edit_paste", window, SLOT( editPaste ( )));
	connectAction ( do_connect, "edit_delete", window, SLOT( editDelete ( )));

	connectAction ( do_connect, "edit_select_all", window, SLOT( selectAll ( )));
	connectAction ( do_connect, "edit_select_none", window, SLOT( selectNone ( )));

	connectAction ( do_connect, "edit_additems", window, SLOT( editAddItems ( )));
	connectAction ( do_connect, "edit_subtractitems", window, SLOT( editSubtractItems ( )));
	connectAction ( do_connect, "edit_mergeitems", window, SLOT( editMergeItems ( )));
	connectAction ( do_connect, "edit_partoutitems", window, SLOT( editPartOutItems ( )));
	connectAction ( do_connect, "edit_reset_diffs", window, SLOT( editResetDifferences ( )));

	connectAction ( do_connect, "edit_price_to_fixed", window, SLOT( editSetPrice ( )));
	connectAction ( do_connect, "edit_price_to_priceguide", window, SLOT( editSetPriceToPG ( )));
	connectAction ( do_connect, "edit_price_inc_dec", window, SLOT( editPriceIncDec ( )));
	connectAction ( do_connect, "edit_set_sale", window, SLOT( editSetSale ( )));
	connectAction ( do_connect, "edit_set_remark", window, SLOT( editSetRemark ( )));
	connectAction ( do_connect, "edit_set_reserved", window, SLOT( editSetReserved ( )));
	connectAction ( do_connect, "edit_set_condition", window, SLOT( editSetCondition ( )));
	connectAction ( do_connect, "edit_multiply_qty", window, SLOT( editMultiplyQty ( )));
	connectAction ( do_connect, "edit_divide_qty", window, SLOT( editDivideQty ( )));
	connectAction ( do_connect, "edit_bl_catalog", window, SLOT( showBLCatalog ( )));
	connectAction ( do_connect, "edit_bl_priceguide", window, SLOT( showBLPriceGuide ( )));
	connectAction ( do_connect, "edit_bl_lotsforsale", window, SLOT( showBLLotsForSale ( )));

	connectAction ( do_connect, "view_difference_mode", window, SLOT( setDifferenceMode ( bool )), &CWindow::isDifferenceMode );

	updateAllToggleActions ( window );
}

void CFrameWork::connectWindow ( QWidget *w )
{
	if ( w == m_current_window )
		return;

	if ( m_current_window ) {
		connectAllActions ( false, m_current_window );

		disconnect ( m_current_window, SIGNAL( statisticsChanged ( const QPtrList<BrickLink::InvItem> & )), this, SLOT( showStatistics ( const QPtrList<BrickLink::InvItem> & )));
		disconnect ( m_current_window, SIGNAL( selectionChanged ( const QPtrList<BrickLink::InvItem> & )), this, SLOT( showInfoForSelection ( const QPtrList<BrickLink::InvItem> & )));
		disconnect ( m_current_window, SIGNAL( modificationChanged ( bool )), this, SLOT( showModification ( bool )));

		disconnect ( m_infobar, SIGNAL( priceDoubleClicked ( money_t )), m_current_window, SLOT( setPrice ( money_t )));

		m_current_window = 0;

		showInfoForSelection ( QPtrList<BrickLink::InvItem> ( ));
		showStatistics ( QPtrList<BrickLink::InvItem> ( ));
		showModification ( false );
	}

	if ( w && ::qt_cast <CWindow *> ( w )) {
		CWindow *window = static_cast <CWindow *> ( w );

		connectAllActions ( true, window );

		connect ( window, SIGNAL( selectionChanged ( const QPtrList<BrickLink::InvItem> & )), this, SLOT( showInfoForSelection ( const QPtrList<BrickLink::InvItem> & )));
		connect ( window, SIGNAL( statisticsChanged ( const QPtrList<BrickLink::InvItem> & )), this, SLOT( showStatistics ( const QPtrList<BrickLink::InvItem> & )));
		connect ( window, SIGNAL( modificationChanged ( bool )), this, SLOT( showModification ( bool )));

		connect ( m_infobar, SIGNAL( priceDoubleClicked ( money_t )), window, SLOT( setPrice ( money_t )));

		m_current_window = window;

		window-> triggerStatisticsUpdate ( );
		window-> triggerSelectionUpdate ( );
		showModification ( window-> isModified ( ));
	}
}

void CFrameWork::showInfoForSelection ( const QPtrList<BrickLink::InvItem> &selection )
{
	struct {
		const char *m_name;
		uint m_minsel;
		uint m_maxsel;
	} endisable_actions [] = {
		{ "edit_cut",                 1, 0 },
		{ "edit_copy",                1, 0 },
		{ "edit_delete",              1, 0 },
		{ "edit_set_sale",            1, 0 },
		{ "edit_set_remark",          1, 0 },
		{ "edit_set_reserved",        1, 0 },
		{ "edit_set_condition",       1, 0 },
		{ "edit_price_to_fixes",      1, 0 },
		{ "edit_price_inc_dec",       1, 0 },
		{ "edit_price_to_priceguide", 1, 0 },
		{ "edit_multiply_qty",        1, 0 },
		{ "edit_divide_qty",          1, 0 },
		{ "edit_bl_catalog",          1, 1 },
		{ "edit_bl_priceguide",       1, 1 },
		{ "edit_bl_lotsforsale",      1, 1 },
		{ "edit_mergeitems",          2, 0 },
		{ "edit_partoutitems",        1, 1 },
		{ "edit_reset_diffs",         1, 0 },

		{ 0, 0, 0 }
	}, *endisable_ptr;

	uint cnt = selection. count ( );

	if ( cnt == 1 ) {
		const BrickLink::InvItem *item = selection. getFirst ( );

		m_infobar-> setPriceGuide ( BrickLink::inst ( )-> priceGuide ( item, true ));
		m_infobar-> setPicture ( BrickLink::inst ( )-> picture ( item, true ));
	}
	else if ( cnt > 1 ) {
		int lots = -1, items = -1, errors = -1;
		money_t value, minvalue;
		double weight = 0.;

		if ( m_current_window )
			m_current_window-> calculateStatistics ( selection, lots, items, value, minvalue, weight, errors );

		QString s;

		if ( lots >= 0 ) {
			QString valstr, wgtstr;

			if ( value != minvalue ) {
				valstr = QString ( "%1 (%2 %3)" ).
							arg( value. toLocalizedString ( )).
							arg( tr( "min." )).
							arg( minvalue. toLocalizedString ( ));
			}
			else
				valstr = value. toLocalizedString ( );

			if ( weight == -DBL_MIN ) {
				wgtstr = "-";
			}
			else {
				if ( weight < 0 ) {
					weight = -weight;
					wgtstr = tr( "min." ) + " ";
				}

				wgtstr += CUtility::weightToString ( weight, ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), true, true );
			}

			s = QString ( "<h3>%1</h3>&nbsp;&nbsp;%2: %3<br />&nbsp;&nbsp;%4: %5<br /><br />&nbsp;&nbsp;%6: %7<br /><br />&nbsp;&nbsp;%8: %9" ). 
			    arg ( tr( "Multiple lots selected" )). 
			    arg ( tr( "Lots" )). arg ( lots ).
			    arg ( tr( "Items" )). arg ( items ).
			    arg ( tr( "Value" )). arg ( valstr ).
			    arg ( tr( "Weight" )). arg ( wgtstr );

			if (( errors > 0 ) && CConfig::inst ( )-> showInputErrors ( ))
				s += QString ( "<br /><br />&nbsp;&nbsp;%1: %2" ). arg ( tr( "Errors" )). arg ( errors );
		}

		m_infobar-> setPriceGuide ( 0 );
		m_infobar-> setInfoText ( s );
	}
	else {
		m_infobar-> setPriceGuide ( 0 );
		m_infobar-> setPicture ( 0 );
	}

	for ( endisable_ptr = endisable_actions; endisable_ptr-> m_name; endisable_ptr++ ) {
		QAction *a = findAction ( endisable_ptr-> m_name );

		if ( a ) {
			uint &mins = endisable_ptr-> m_minsel;
			uint &maxs = endisable_ptr-> m_maxsel;

			a-> setEnabled (( mins ? ( cnt >= mins ) : true ) && ( maxs ? ( cnt <= maxs ) : true ));
		}
	}
}

void CFrameWork::showStatistics ( const QPtrList <BrickLink::InvItem> &all_items )
{
	int lots = -1, items = -1, errors = -1;
	money_t value, minvalue;
	double weight = 0.;

	if ( m_current_window ) {
		QPtrList <BrickLink::InvItem> not_exclude;

		for ( QPtrListIterator <BrickLink::InvItem> it ( all_items ); it. current ( ); ++it ) {
			if ( it. current ( )-> status ( ) != BrickLink::InvItem::Exclude )
				not_exclude. append ( it. current ( ));
		}

		m_current_window-> calculateStatistics ( not_exclude, lots, items, value, minvalue, weight, errors );
	}

	QString s;

	if ( lots >= 0 ) {
		QString valstr, wgtstr;

		if ( value != minvalue ) {
			valstr = QString ( "%1 (%2 %3)" ).
			         arg( value. toLocalizedString ( )).
			         arg( tr( "min." )).
			         arg( minvalue. toLocalizedString ( ));
		}
		else
			valstr = value. toLocalizedString ( );

		if ( weight == -DBL_MIN ) {
			wgtstr = "-";
		}
		else {
			if ( weight < 0 ) {
				weight = -weight;
				wgtstr = tr( "min." ) + " ";
			}

			wgtstr += CUtility::weightToString ( weight, ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), true, true );
		}

		s = QString( "  %1: %2   %3: %4   %5: %6   %7: %8  " ).
		    arg( tr( "Lots" )). arg ( lots ).
		    arg( tr( "Items" )). arg ( items ).
		    arg( tr( "Value" )). arg ( valstr ).
		    arg( tr( "Weight" )). arg ( wgtstr );
	}

	m_statistics-> setText ( s );

	s = QString::null;

	if (( errors > 0 ) && CConfig::inst ( )-> showInputErrors ( ))
		s = QString( "  %1: %2  " ). arg( tr( "Errors" )). arg( errors );

	m_errors-> setText ( s );
}

void CFrameWork::showModification ( bool b )
{
	QAction *a = findAction ( "file_save" );
	if ( a )
		a-> setEnabled ( b );

	static QPixmap pnomod, pmod;

	if ( pmod. isNull ( )) {
		int h = m_modified-> height ( ) - 2;

		pmod. resize ( h, h );
		pmod. fill ( black );

		pnomod = pmod;
		pnomod. setMask ( pnomod. createHeuristicMask ( ));

		QPixmap p = CResource::inst ( )-> pixmap ( "status/modified" );
		if ( !p. isNull ( ))
			pmod. convertFromImage ( p. convertToImage ( ). smoothScale ( p. size ( ). boundedTo ( QSize ( h, h ))));
	}
	m_modified-> setPixmap ( b ? pmod : pnomod );

	if ( b )
		QToolTip::add ( m_modified, tr( "Unsaved modifications" ));
	else
		QToolTip::remove ( m_modified );
}

void CFrameWork::gotInventoryProgress ( int p, int t )
{
	m_progress-> setItemProgress ( PGI_Inventory, p, t );
}

void CFrameWork::gotPictureProgress ( int p, int t )
{
	m_progress-> setItemProgress ( PGI_Picture, p, t );
}

void CFrameWork::gotPriceGuideProgress ( int p, int t )
{
	m_progress-> setItemProgress ( PGI_PriceGuide, p, t );
}

void CFrameWork::configure ( )
{
	configure ( 0 );
}

void CFrameWork::configure ( const char *page )
{
	DlgSettingsImpl dlg ( this );

	if ( page )
		dlg. setCurrentPage ( page );

	dlg. exec ( );
}

void CFrameWork::setOnlineStatus ( QAction *act )
{
	bool b = ( act == findAction ( "net_online" ));

	if ( !b && m_running )
		cancelAllTransfers ( );
		
	CConfig::inst ( )-> setOnlineStatus ( b );
}

void CFrameWork::showContextMenu ( bool /*onitem*/, const QPoint &pos )
{
	m_contextmenu-> popup ( pos );
}

void CFrameWork::addToRecentFiles ( const QString &s )
{
	QString name = QDir::convertSeparators ( QFileInfo ( s ). absFilePath ( ));

	m_recent_files. remove ( name );
	m_recent_files. push_front ( name );

	while ( m_recent_files. count ( ) > MaxRecentFiles )
		m_recent_files. pop_back ( );
}

void CFrameWork::closeEvent ( QCloseEvent *e )
{
	if ( !closeAllWindows ( )) {
		e-> ignore ( );
		return;
	}

	QMainWindow::closeEvent ( e );
}


bool CFrameWork::closeAllWindows ( )
{
	QPtrList <CWindow> list = allWindows ( );

	for ( uint i = 0; i < list. count ( ); i++ ) {
		if ( !list. at ( i )-> close ( ))
			return false;
	}
	return true;
}

QPtrList <CWindow> CFrameWork::allWindows ( ) const
{
	QPtrList <CWindow> list;

	QWidgetList wl = m_mdi-> windowList ( QWorkspace::CreationOrder );

	if ( !wl. isEmpty ( )) {
		for ( uint i = 0; i < wl. count ( ); i++ ) {
			CWindow *w = ::qt_cast <CWindow *> ( wl. at ( i ));
			if ( w )
				list. append ( w );
		}
	}
	return list;
}

void CFrameWork::cancelAllTransfers ( )
{
	if ( CMessageBox::question ( this, tr( "Do you want to cancel all outstanding inventory, picture and price guide transfers?" ), CMessageBox::Yes | CMessageBox::Default, CMessageBox::No | CMessageBox::Escape ) == CMessageBox::Yes ) {
		BrickLink::inst ( )-> cancelInventoryTransfers ( );
		BrickLink::inst ( )-> cancelPictureTransfers ( );
		BrickLink::inst ( )-> cancelPriceGuideTransfers ( );
	}
}
