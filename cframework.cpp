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
#include <float.h>

#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qsignalmapper.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qstatusbar.h>
#include <qwidget.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qobject.h>
#include <qtooltip.h>
#include <qcursor.h>
#include <q3accel.h>
//Added by qt3to4:
#include <QMoveEvent>
#include <QResizeEvent>
#include <QPixmap>
#include <QCloseEvent>
#include <Q3ValueList>
#include <Q3TextStream>
#include <QDragEnterEvent>
#include <QEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDesktopWidget>

#include "capplication.h"
#include "cmessagebox.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cconfig.h"
#include "cmoney.h"
#include "cresource.h"
#include "cmultiprogressbar.h"
#include "ciconfactory.h"
#include "cutility.h"
#include "cspinner.h"
#include "cundo.h"
#include "ctaskpanemanager.h"
#include "ctaskwidgets.h"
#include "cprogressdialog.h"
#include "cupdatedatabase.h"
#include "csplash.h"

#include "dlgadditemimpl.h"
#include "dlgsettingsimpl.h"

#include "cframework.h"

namespace {

enum ProgressItem {
	PGI_PriceGuide,
	PGI_Picture
};

} // namespace


CFrameWork::RecentListProvider::RecentListProvider ( CFrameWork *fw )
	: CListAction::Provider ( ), m_fw ( fw )
{ }

CFrameWork::RecentListProvider::~RecentListProvider ( )
{ }

QStringList CFrameWork::RecentListProvider::list ( int & /*active*/, Q3ValueList <int> & )
{
	return m_fw-> m_recent_files;
}


CFrameWork::WindowListProvider::WindowListProvider ( CFrameWork *fw )
	: CListAction::Provider ( ), m_fw ( fw )
{ }

CFrameWork::WindowListProvider::~WindowListProvider ( )
{ }

QStringList CFrameWork::WindowListProvider::list ( int &active, Q3ValueList <int> & )
{
	QStringList sl;
    QList<QMdiSubWindow *> l = m_fw-> m_mdi-> subWindowList( QMdiArea::CreationOrder );
    QMdiSubWindow *aw = m_fw-> m_mdi-> activeSubWindow ( );

    for ( int i = 0; i < l. count ( ); i++ ) {
        QMdiSubWindow *w = l. at ( i );

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
		(void) new CFrameWork ( 0, "FrameWork", Qt::WDestructiveClose );

	return s_inst;
}

void CFrameWork::setActiveSubWindow(QWidget *window)
{
    if (!window)
        return;
    m_mdi->setActiveSubWindow(qobject_cast<QMdiSubWindow *>(window));
}

CFrameWork::CFrameWork ( QWidget *parent, const char *name, Qt::WFlags fl )
    : QMainWindow ( parent, name, fl )
{
	s_inst = this;

	m_running = false;

	qApp-> setMainWidget ( this );

#if defined( Q_WS_X11 )
 	if ( !icon ( ) || icon ( )-> isNull ( )) {
		QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
		if ( !pix. isNull ( ))
			setIcon ( pix );
	}
#endif
	
	setCaption ( cApp-> appName ( ));
    //setUsesBigPixmaps ( true );

//	(void) new CIconFactory ( );

	connect ( cApp, SIGNAL( openDocument ( const QString & )), this, SLOT( openDocument ( const QString & )));

	m_recent_files = CConfig::inst ( )-> readListEntry ( "/Files/Recent" );

	while ( m_recent_files. count ( ) > MaxRecentFiles )
		m_recent_files. pop_back ( );

	m_current_window = 0;

    m_mdi = new QMdiArea ( this );
    connect ( m_mdi, SIGNAL( subWindowActivated( QMdiSubWindow * )), this, SLOT(connectWindow ( QMdiSubWindow * )));
    QSignalMapper *windowMapper = new QSignalMapper(this);
    connect(windowMapper, SIGNAL(mapped(QWidget*)), this, SLOT(setActiveSubWindow(QWidget*)));

	setCentralWidget ( m_mdi );

    m_taskpanes = new CTaskPaneManager ( this, "taskpanemanager" );
	m_taskpanes-> setMode ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Infobar/Mode", CTaskPaneManager::Modern ) != CTaskPaneManager::Classic ? CTaskPaneManager::Modern : CTaskPaneManager::Classic );

	m_task_info = new CTaskInfoWidget ( 0, "iteminfowidget" );
	m_taskpanes-> addItem ( m_task_info, CResource::inst ( )-> pixmap ( "sidebar/info" ));
	m_taskpanes-> setItemVisible ( m_task_info, CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Infobar/InfoVisible", true ));

	m_task_priceguide = new CTaskPriceGuideWidget ( 0, "priceguidewidget" );
    m_taskpanes-> addItem ( m_task_priceguide, CResource::inst ( )-> pixmap ( "sidebar/priceguide" ));
	m_taskpanes-> setItemVisible ( m_task_priceguide, CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Infobar/PriceguideVisible", true ));

	m_task_appears = new CTaskAppearsInWidget ( 0, "appearsinwidget" );
	m_taskpanes-> addItem ( m_task_appears,  CResource::inst ( )-> pixmap ( "sidebar/appearsin" ));
	m_taskpanes-> setItemVisible ( m_task_appears, CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Infobar/AppearsinVisible", true ));

	m_task_links = new CTaskLinksWidget ( 0, "linkswidget" );
	m_taskpanes-> addItem ( m_task_links,  CResource::inst ( )-> pixmap ( "sidebar/links" ));
	m_taskpanes-> setItemVisible ( m_task_links, CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Infobar/LinksVisible", true ));

	createActions ( );

	QString str;
	QStringList sl;

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/File" );
	if ( sl. isEmpty ( ))
		sl << "file_new"
		   << "file_open"
		   << "file_open_recent"
		   << "-"
		   << "file_save"
		   << "file_saveas"
		   << "-"
		   << "file_import"
		   << "file_export"
		   << "-"
           << "file_print"
           << "-"
		   << "file_close"
		   << "-"
		   << "file_exit";

    m_menuid_file = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Edit" );
	if ( sl. isEmpty ( ))
		sl << "edit_undo"
		   << "edit_redo"
		   << "-"
		   << "edit_cut"
		   << "edit_copy"
		   << "edit_paste"
		   << "edit_delete"
		   << "-"
		   << "edit_select_all"
		   << "edit_select_none"
		   << "-"
		   << "edit_additems"
		   << "edit_subtractitems"
		   << "edit_mergeitems"
		   << "edit_partoutitems"
		   << "-"
           << "edit_status"
           << "edit_cond"
           << "edit_color"
           << "edit_qty"
           << "edit_price"
           << "edit_bulk"
           << "edit_sale"
           << "edit_comment"
           << "edit_remark"
           << "edit_retain"
           << "edit_stockroom"
           << "edit_reserved"
		   << "-"
		   << "edit_reset_diffs"
		   << "edit_copyremarks"
		   << "-"
           << "edit_bl_catalog"
           << "edit_bl_priceguide"
           << "edit_bl_lotsforsale"
           << "edit_bl_myinventory";

	m_menuid_edit = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/View" );
	if ( sl. isEmpty ( ))
		sl << "view_toolbar"
		   << "view_infobar"
		   << "view_statusbar"
		   << "-"
		   << "view_fullscreen"
		   << "-"
		   << "view_simple_mode"
		   << "view_show_input_errors"
		   << "view_difference_mode"
		   << "-"
		   << "view_save_default_col"; 

	m_menuid_view = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Extras" );
	if ( sl. isEmpty ( ))
		sl << "extras_net"
		   << "-"
		   << "extras_update_database"
		   << "-"
		   << "extras_configure";

	// Make sure there is a possibility to open the pref dialog!
	if ( sl. find ( "extras_configure" ) == sl. end ( ))
		sl << "-" << "extras_configure";

	m_menuid_extras = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Window" );
	if ( sl. isEmpty ( ))
		sl << "window_mode"
		   << "-"
		   << "window_cascade"
		   << "window_tile"
		   << "-"
		   << "window_list";

	m_menuid_window = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Menubar/Help" );
	if ( sl. isEmpty ( ))
		sl << "help_updates"
		   << "-"
		   << "help_registration"
		   << "-"
		   << "help_about";

	m_menuid_help = menuBar ( )-> insertItem ( QString ( ), createMenu ( sl ));

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/ContextMenu/Item" );
	if ( sl. isEmpty ( ))
		sl << "edit_cut"
		   << "edit_copy"
		   << "edit_paste"
		   << "edit_delete"
		   << "-"
		   << "edit_select_all"
		   << "-"
		   << "edit_mergeitems"
		   << "edit_partoutitems"
		   << "-"
           << "edit_status"
           << "edit_cond"
           << "edit_color"
           << "edit_qty"
           << "edit_price"
           << "edit_remark"
		   << "-"
           << "edit_bl_catalog"
           << "edit_bl_priceguide"
           << "edit_bl_lotsforsale"
           << "edit_bl_myinventory";

	m_contextmenu = createMenu ( sl );

	sl = CConfig::inst ( )-> readListEntry ( "/MainWindow/Toolbar/Buttons" );
	if ( sl. isEmpty ( ))
		sl << "file_new"
		   << "file_open"
		   << "file_save"
		   << "-"
		   << "file_import"
		   << "file_export"
		   << "-"
		   << "edit_undo"
		   << "edit_redo"
		   << "-"
		   << "edit_cut"
		   << "edit_copy"
		   << "edit_paste"
		   << "-"
		   << "edit_additems"
		   << "edit_subtractitems"
		   << "edit_mergeitems"
		   << "edit_partoutitems"
		   << "-"
		   << "edit_price_to_priceguide"
		   << "edit_price_inc_dec"
		   << "-"
		   << "edit_bl_catalog"
		   << "edit_bl_priceguide"
		   << "edit_bl_lotsforsale"
           << "-"
           << "extras_net";

	m_toolbar = createToolBar ( QString ( ), sl );
    connect ( m_toolbar, SIGNAL( visibilityChanged ( bool )), findChild<QAction *> ( "view_toolbar" ), SLOT( setOn ( bool )));

	createStatusBar ( );
    findChild<QAction *> ( "view_statusbar" )-> setChecked ( CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Statusbar/Visible", true ));

	languageChange ( );

	str = CConfig::inst ( )-> readEntry ( "/MainWindow/Layout/DockWindows" );
	if ( str. isEmpty ( )) {
        findChild<QAction *> ( "view_toolbar" )-> setChecked ( true );
	}
	else {
		Q3TextStream ts ( &str, QIODevice::ReadOnly ); 
        //ts >> *this;
	}

	QRect r ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Left",   -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Top",    -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Width",  -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Height", -1 ));

    Qt::WindowState wstate = (Qt::WindowState)CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/State", Qt::WindowNoState );

	// make sure at least SOME part of the window ends up on the screen
	if ( !r. isValid ( ) || r. isEmpty ( ) || ( r & qApp-> desktop ( )-> rect ( )). isEmpty ( )) {
		float dw = qApp-> desktop ( )-> width ( ) / 10.f;
		float dh = qApp-> desktop ( )-> height ( ) / 10.f;

		r = QRect ( int( dw ), int( dh ), int ( 8 * dw ), int( 8 * dh )); 
		wstate = Qt::WindowMaximized;
	}

	resize ( r. size ( ));  // X11 compat. mode -- KDE 3.x doesn't seem to like this though...
	move ( r. topLeft ( ));
	//setGeometry ( r );  // simple code (which shouldn't work on X11, but does on KDE 3.x...)

	m_normal_geometry = QRect ( pos ( ), size ( ));

    setWindowState ( (Qt::WindowState)(wstate & ( /*WindowMinimized |*/ Qt::WindowMaximized | Qt::WindowFullScreen )));
    findChild<QAction *> ( "view_fullscreen" )-> setChecked ( wstate & Qt::WindowFullScreen );

    QAction *windowAction;
	switch ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/WindowMode", 1 )) {
        case  0:
            windowAction = findChild<QAction *> ( "window_mode_mdi" );
            windowAction-> setChecked ( true );
            break;
        case  1:
            windowAction = findChild<QAction *> ( "window_mode_tab_above" );
            windowAction-> setChecked ( true );
            break;
        case  2:
        default:
            windowAction = findChild<QAction *> ( "window_mode_tab_below" );
            windowAction-> setChecked ( true );
            break;
	}
    setWindowMode ( windowAction );

	BrickLink *bl = BrickLink::inst ( );

	connect ( CConfig::inst ( ), SIGNAL( onlineStatusChanged ( bool )), bl, SLOT( setOnlineStatus ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( blUpdateIntervalsChanged ( int, int )), bl, SLOT( setUpdateIntervals ( int, int )));
	connect ( CConfig::inst ( ), SIGNAL( proxyChanged ( bool, const QString &, int )), bl, SLOT( setHttpProxy ( bool, const QString &, int )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( statisticsUpdate ( )));
	connect ( CConfig::inst ( ), SIGNAL( weightSystemChanged ( CConfig::WeightSystem )), this, SLOT( statisticsUpdate ( )));
	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( setSimpleMode ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( registrationChanged ( CConfig::Registration )), this, SLOT( registrationUpdate ( )));

    findChild<QAction *> ( "view_show_input_errors" )-> setChecked ( CConfig::inst ( )-> showInputErrors ( ));

	registrationUpdate ( );

    findChild<QAction *> ( CConfig::inst ( )-> onlineStatus ( ) ? "extras_net_online" : "extras_net_offline" )-> setChecked ( true );
	 
	bl-> setOnlineStatus ( CConfig::inst ( )-> onlineStatus ( ));
	bl-> setHttpProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
	{
		int pic, pg;
		CConfig::inst ( )-> blUpdateIntervals ( pic, pg );
		bl-> setUpdateIntervals ( pic, pg );
	}

	connect ( bl, SIGNAL( priceGuideProgress ( int, int )), this, SLOT( gotPriceGuideProgress ( int, int )));
	connect ( bl, SIGNAL( pictureProgress ( int, int )),    this, SLOT( gotPictureProgress ( int, int )));

	connect ( m_progress, SIGNAL( statusChange ( bool )), m_spinner, SLOT( setActive ( bool )));
	connect ( CUndoManager::inst ( ), SIGNAL( cleanChanged ( bool )), this, SLOT( modificationUpdate ( )));

	CSplash::inst ( )-> message ( qApp-> translate ( "CSplash", "Loading Database..." ));

	bool dbok = BrickLink::inst ( )-> readDatabase ( );

	if ( !dbok ) {
		if ( CMessageBox::warning ( this, tr( "Could not load the BrickLink database files.<br /><br />Should these files be updated now?" ), CMessageBox::Yes, CMessageBox::No ) == CMessageBox::Yes )
			dbok = updateDatabase ( );
	}

	if ( dbok )
		cApp-> enableEmitOpenDocument ( );
	else
		CMessageBox::warning ( this, tr( "Could not load the BrickLink database files.<br /><br />The program is not functional without these files." ));

	setAcceptDrops ( true );

	m_add_dialog = 0;
	m_running = true;

    connectAllActions ( false, 0 ); // init enabled/disabled status of document actions
	connectWindow ( 0 );

    CCheckForUpdates *m_cfu = new CCheckForUpdates ( 0 );
    connect (m_cfu, SIGNAL ( newVersionAvailable ( ) ), this, SLOT ( newVersionAvailable ( ) ));

    QDateTime dt = CConfig::inst ( )-> lastApplicationUpdateCheck ( );
    if ( dt.secsTo( QDateTime::currentDateTime ( ) ) > 24 * 60 * 60 ) {
        m_cfu->checkNow();
    }

    connect (m_newversion, SIGNAL ( linkActivated ( const QString & ) ), cApp, SLOT( checkForUpdates ( ) ));
}

void CFrameWork::languageChange ( )
{
	m_toolbar-> setLabel ( tr( "Toolbar" ));
	m_progress-> setItemLabel ( PGI_PriceGuide, tr( "Price Guide updates" ));
	m_progress-> setItemLabel ( PGI_Picture,    tr( "Image updates" ));

	m_taskpanes-> setItemText ( m_task_info,       tr( "Info" ));
	m_taskpanes-> setItemText ( m_task_priceguide, tr( "Price Guide" ));
	m_taskpanes-> setItemText ( m_task_appears,    tr( "Appears In Sets" ));
	m_taskpanes-> setItemText ( m_task_links,      tr( "Links" ));

	menuBar ( )-> changeItem ( m_menuid_file,   tr( "&File" ));
	menuBar ( )-> changeItem ( m_menuid_edit,   tr( "&Edit" ));
	menuBar ( )-> changeItem ( m_menuid_view,   tr( "&View" ));
	menuBar ( )-> changeItem ( m_menuid_extras, tr( "E&xtras" ));
	menuBar ( )-> changeItem ( m_menuid_window, tr( "&Windows" ));
	menuBar ( )-> changeItem ( m_menuid_help,   tr( "&Help" ));

	struct {
		const char *action; QString text; QString accel;
	} *atptr, actiontable [] = {
		{ "file_new",                       tr( "New", "File|New" ),                    tr( "Ctrl+N", "File|New" ) },
		{ "file_open",                      tr( "Open..." ),                            tr( "Ctrl+O", "File|Open" ) },
		{ "file_open_recent",               tr( "Open Recent" ),                        0 },
		{ "file_save",                      tr( "Save" ),                               tr( "Ctrl+S", "File|Save" ) },
		{ "file_saveas",                    tr( "Save As..." ),                         0 },
		{ "file_print",                     tr( "Print..." ),                           tr( "Ctrl+P", "File|Print" ) },
		{ "file_import",                    tr( "Import" ),                             0 },
		{ "file_import_bl_inv",             tr( "BrickLink Set Inventory..." ),         tr( "Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory" ) },
		{ "file_import_bl_xml",             tr( "BrickLink XML..." ),                   tr( "Ctrl+I,Ctrl+X", "File|Import BrickLink XML" ) },
        { "file_import_bl_order",           tr( "BrickLink Order..." ),                 tr( "Ctrl+I,Ctrl+O", "File|Import BrickLink Order" ) },
		{ "file_import_bl_store_inv",       tr( "BrickLink Store Inventory..." ),       tr( "Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory" ) },
		{ "file_import_bl_cart",            tr( "BrickLink Shopping Cart..." ),         tr( "Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart" ) },
		{ "file_import_peeron_inv",         tr( "Peeron Inventory..." ),                tr( "Ctrl+I,Ctrl+P", "File|Import Peeron Inventory" ) },
		{ "file_import_ldraw_model",        tr( "LDraw Model..." ),                     tr( "Ctrl+I,Ctrl+L", "File|Import LDraw Model" ) },
		{ "file_import_briktrak",           tr( "BrikTrak Inventory..." ),              0 },
		{ "file_export",                    tr( "Export" ),                             0 },
		{ "file_export_bl_xml",             tr( "BrickLink XML..." ),                         tr( "Ctrl+E,Ctrl+X", "File|Import BrickLink XML" ) },
		{ "file_export_bl_xml_clip",        tr( "BrickLink Mass-Upload XML to Clipboard" ),   tr( "Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload" ) },
		{ "file_export_bl_update_clip",     tr( "BrickLink Mass-Update XML to Clipboard" ),   tr( "Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update" ) },
		{ "file_export_bl_invreq_clip",     tr( "BrickLink Set Inventory XML to Clipboard" ), tr( "Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory" ) },
		{ "file_export_bl_wantedlist_clip", tr( "BrickLink Wanted List XML to Clipboard" ),   tr( "Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List" ) },
		{ "file_export_briktrak",           tr( "BrikTrak Inventory..." ),              0 },
		{ "file_close",                     tr( "Close" ),                              tr( "Ctrl+W", "File|Close" ) },
		{ "file_exit",                      tr( "Exit" ),                               tr( "Ctrl+Q", "File|Quit" ) },
		{ "edit_undo",                      0,                                          tr( "Ctrl+Z", "Edit|Undo" ) },
		{ "edit_redo",                      0,                                          tr( "Ctrl+Y", "Edit|Redo" ) },
		{ "edit_cut",                       tr( "Cut" ),                                tr( "Ctrl+X", "Edit|Cut" ) },
		{ "edit_copy",                      tr( "Copy" ),                               tr( "Ctrl+C", "Edit|Copy" ) },
		{ "edit_paste",                     tr( "Paste" ),                              tr( "Ctrl+V", "Edit|Paste" ) },
		{ "edit_delete",                    tr( "Delete" ),                             tr( "Delete", "Edit|Delete" ) },
		{ "edit_additems",                  tr( "Add Items..." ),                       tr( "Insert", "Edit|AddItems" ) },
		{ "edit_subtractitems",             tr( "Subtract Items..." ),                  0 },
		{ "edit_mergeitems",                tr( "Consolidate Items..." ),               0 },
		{ "edit_partoutitems",              tr( "Part out Item..." ),                   0 },
		{ "edit_reset_diffs",               tr( "Reset Differences" ),                  0 }, 
		{ "edit_copyremarks",               tr( "Copy Remarks from Document..." ),      0 },
		{ "edit_select_all",                tr( "Select All" ),                         tr( "Ctrl+A", "Edit|SelectAll" ) },
		{ "edit_select_none",               tr( "Select None" ),                        tr( "Ctrl+Shift+A", "Edit|SelectNone" ) },
		{ "view_simple_mode",               tr( "Buyer/Collector Mode" ),               0 },
		{ "view_toolbar",                   tr( "View Toolbar" ),                       0 },
		{ "view_infobar",                   tr( "View Infobars" ),                      0 },
		{ "view_statusbar",                 tr( "View Statusbar" ),                     0 },
		{ "view_fullscreen",                tr( "Full Screen" ),                        tr( "F11" ) },
		{ "view_show_input_errors",         tr( "Show Input Errors" ),                  0 },
		{ "view_difference_mode",           tr( "Difference Mode" ),                    0 },
		{ "view_save_default_col",          tr( "Save Column Layout as Default" ),      0 },
		{ "extras_update_database",         tr( "Update Database" ),                    0 },
		{ "extras_configure",               tr( "Configure..." ),                       0 },
		{ "extras_net_online",              tr( "Online Mode" ),                        0 },
		{ "extras_net_offline",             tr( "Offline Mode" ),                       0 },
		{ "window_mode_mdi",                tr( "Standard MDI Mode" ),                  0 },
		{ "window_mode_tab_above",          tr( "Show Tabs at Top" ),                   0 },
		{ "window_mode_tab_below",          tr( "Show Tabs at Bottom" ),                0 },
		{ "window_cascade",                 tr( "Cascade" ),                            0 },
		{ "window_tile",                    tr( "Tile" ),                               0 },
        { "window_list",                    tr( "Windows" ),                            0 },
        { "help_whatsthis",                 tr( "What's this?" ),                       tr( "Shift+F1", "Help|WhatsThis" ) },
		{ "help_about",                     tr( "About..." ),                           0 },
		{ "help_updates",                   tr( "Check for Program Updates..." ),       0 },
		{ "help_registration",              tr( "Registration..." ),                    0 },
		{ "edit_status",                    tr( "Status" ),                             0 },
		{ "edit_status_include",            tr( "Include" ),                            0 },
		{ "edit_status_exclude",            tr( "Exclude" ),                            0 },
		{ "edit_status_extra",              tr( "Extra" ),                              0 },
		{ "edit_status_toggle",             tr( "Toggle Include/Exclude" ),             0 },
		{ "edit_cond",                      tr( "Condition" ),                          0 },
		{ "edit_cond_new",                  tr( "New", "Cond|New" ),                    0 },
		{ "edit_cond_used",                 tr( "Used" ),                               0 },
		{ "edit_cond_toggle",               tr( "Toggle New/Used" ),                    0 },
		{ "edit_color",                     tr( "Color..." ),                           0 },
		{ "edit_qty",                       tr( "Quantity" ),                           0 },
		{ "edit_qty_multiply",              tr( "Multiply..." ),                        tr( "Ctrl+*", "Edit|Quantity|Multiply" ) },
		{ "edit_qty_divide",                tr( "Divide..." ),                          tr( "Ctrl+/", "Edit|Quantity|Divide" ) },
		{ "edit_price",                     tr( "Price" ),                              0 },
		{ "edit_price_round",               tr( "Round to 2 Decimal Places" ),          0 },
		{ "edit_price_set",                 tr( "Set..." ),                             0 },
		{ "edit_price_to_priceguide",       tr( "Set to Price Guide..." ),              tr( "Ctrl+G", "Edit|Price|Set to PriceGuide" ) },
		{ "edit_price_inc_dec",             tr( "Inc- or Decrease..." ),                tr( "Ctrl++", "Edit|Price|Inc/Dec" ) },
		{ "edit_bulk",                      tr( "Bulk Quantity..." ),                   0 },
		{ "edit_sale",                      tr( "Sale..." ),                            tr( "Ctrl+%", "Edit|Sale" ) },
		{ "edit_comment",                   tr( "Comment" ),                            0 },
		{ "edit_comment_set",               tr( "Set..." ),                             0 },
		{ "edit_comment_add",               tr( "Add to..." ),                          0 },
		{ "edit_comment_rem",               tr( "Remove from..." ),                     0 },
		{ "edit_remark",                    tr( "Remark" ),                             0 },
		{ "edit_remark_set",                tr( "Set..." ),                             0 },
		{ "edit_remark_add",                tr( "Add to..." ),                          0 },
		{ "edit_remark_rem",                tr( "Remove from..." ),                     0 },
		{ "edit_retain",                    tr( "Retain in Inventory" ),                0 },
		{ "edit_retain_yes",                tr( "Yes" ),                                0 },
		{ "edit_retain_no",                 tr( "No" ),                                 0 },
		{ "edit_retain_toggle",             tr( "Toggle Yes/No" ),                      0 },
		{ "edit_stockroom",                 tr( "Stockroom Item" ),                     0 },
		{ "edit_stockroom_yes",             tr( "Yes" ),                                0 },
		{ "edit_stockroom_no",              tr( "No" ),                                 0 },
		{ "edit_stockroom_toggle",          tr( "Toggle Yes/No" ),                      0 },
		{ "edit_reserved",                  tr( "Reserved for..." ),                    0 },
		{ "edit_bl_catalog",                tr( "Show BrickLink Catalog Info..." ),     0 },
		{ "edit_bl_priceguide",             tr( "Show BrickLink Price Guide Info..." ), 0 },
		{ "edit_bl_lotsforsale",            tr( "Show Lots for Sale on BrickLink..." ), 0 },
		{ "edit_bl_myinventory",            tr( "Show in my Store on BrickLink..." ),   0 },

		{ 0, 0, 0 }
	};

	for ( atptr = actiontable; atptr-> action; atptr++ ) {
        QAction *a = findChild<QAction *> ( atptr-> action );
        QMenu *q = findChild<QMenu *> ( atptr-> action );

		if ( a ) {
			if ( !atptr-> text. isNull ( ))
				a-> setText ( atptr-> text );
			if ( !atptr-> accel. isNull ( ))
				a-> setAccel ( QKeySequence ( atptr-> accel ));
		}
        if ( q ) {
            if ( !atptr-> text. isNull ( ))
                q->setTitle( atptr-> text );
        }
	}

	statisticsUpdate ( );
}

CFrameWork::~CFrameWork ( )
{
	CConfig::inst ( )-> writeEntry ( "/Files/Recent", m_recent_files );

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Statusbar/Visible",         statusBar ( )-> isVisibleTo ( this ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/Mode",              m_taskpanes-> mode ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/InfoVisible",       m_taskpanes-> isItemVisible ( m_task_info ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/PriceguideVisible", m_taskpanes-> isItemVisible ( m_task_priceguide ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/AppearsinVisible",  m_taskpanes-> isItemVisible ( m_task_appears ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Infobar/LinksVisible",      m_taskpanes-> isItemVisible ( m_task_links ));

	QString str;
    { Q3TextStream ts ( &str, QIODevice::WriteOnly ); /*ts << *this;*/ }

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/DockWindows", str );

	int wstate = windowState ( ) & ( Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen );
	QRect wgeo = wstate ? m_normal_geometry : QRect ( pos ( ), size ( ));

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/State",  wstate );
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Left",   wgeo. x ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Top",    wgeo. y ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Width",  wgeo. width ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Height", wgeo. height ( ));

    CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/WindowMode", m_mdi->viewMode() == QMdiArea::TabbedView ? ( m_mdi-> tabPosition() == QTabWidget::South ? 2 : 1 ) : 0 );

	if ( m_add_dialog ) {
		int wstate = m_add_dialog-> windowState ( ) & Qt::WindowMaximized;
		QRect wgeo = wstate ? m_normal_geometry_adddlg : QRect ( m_add_dialog-> pos ( ), m_add_dialog-> size ( ));

		CConfig::inst ( )-> writeEntry ( "/MainWindow/AddItemDialog/State",  wstate );
		CConfig::inst ( )-> writeEntry ( "/MainWindow/AddItemDialog/Left",   wgeo. x ( ));
		CConfig::inst ( )-> writeEntry ( "/MainWindow/AddItemDialog/Top",    wgeo. y ( ));
		CConfig::inst ( )-> writeEntry ( "/MainWindow/AddItemDialog/Width",  wgeo. width ( ));
		CConfig::inst ( )-> writeEntry ( "/MainWindow/AddItemDialog/Height", wgeo. height ( ));
	}
	s_inst = 0;
}

void CFrameWork::dragMoveEvent ( QDragMoveEvent *e )
{
	e-> acceptAction ( e-> action ( ) == QDropEvent::Copy );
}

void CFrameWork::dragEnterEvent ( QDragEnterEvent *e )
{
	if ( Q3UriDrag::canDecode ( e ))
		e-> accept ( );
}

void CFrameWork::dropEvent ( QDropEvent *e )
{
	QStringList sl;

	if ( Q3UriDrag::decodeLocalFiles( e, sl )) {
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it ) {
			openDocument ( *it );
		}
	}
}

void CFrameWork::moveEvent ( QMoveEvent *e )
{
	if (!( windowState ( ) & ( Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen )))
		m_normal_geometry. setTopLeft ( pos ( ));
    QMainWindow::moveEvent ( e );
}

void CFrameWork::resizeEvent ( QResizeEvent *e )
{
    if (!( windowState ( ) & ( Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen )))
		m_normal_geometry. setSize ( size ( ));
    QMainWindow::resizeEvent ( e );
}

bool CFrameWork::eventFilter ( QObject *o, QEvent *e )
{
	if ( o && ( o == m_add_dialog )) {
		if ( e-> type ( ) == QEvent::Resize ) {
			if (!( m_add_dialog-> windowState ( ) & ( Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen )))
				m_normal_geometry_adddlg. setSize ( m_add_dialog-> size ( ));
		}
		else if ( e-> type ( ) == QEvent::Move ) {
			if (!( m_add_dialog-> windowState ( ) & ( Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen )))
				m_normal_geometry_adddlg. setTopLeft ( m_add_dialog-> pos ( ));
		}
	}
    return QMainWindow::eventFilter ( o, e );
}

void CFrameWork::createStatusBar ( )
{
    m_newversion = new QLabel ( statusBar ( ));
    statusBar ( )-> addWidget ( m_newversion, 0 );

	m_errors = new QLabel ( statusBar ( ));
    statusBar ( )-> addPermanentWidget ( m_errors, 0 );

	m_statistics = new QLabel ( statusBar ( ));
    statusBar ( )-> addPermanentWidget ( m_statistics, 0 );

	m_modified = new QLabel ( statusBar ( ));
    statusBar ( )-> addPermanentWidget ( m_modified, 0 );

	m_progress = new CMultiProgressBar ( statusBar ( ));
	m_progress-> setFixedWidth ( fontMetrics ( ). height ( ) * 10 );
	m_progress-> setFixedHeight ( fontMetrics ( ). height ( ));

	QPixmap p;
	p = CResource::inst ( )-> pixmap ( "status/stop" );
	if ( !p. isNull ( ))
		m_progress-> setStopPixmap ( p );

	m_progress-> addItem ( QString ( ), PGI_PriceGuide );
	m_progress-> addItem ( QString ( ), PGI_Picture    );

	//m_progress-> setProgress ( -1, 100 );

    statusBar ( )-> addPermanentWidget ( m_progress, 0 );

	connect ( m_progress, SIGNAL( stop ( )), this, SLOT( cancelAllTransfers ( )));

	statusBar ( )-> hide ( );
}

QMenu *CFrameWork::createMenu ( const QStringList &a_names )
{
	if ( a_names. isEmpty ( ))
		return 0;

    QMenu *p = new QMenu ( this );
	p-> setCheckable ( true );

	for ( QStringList::ConstIterator it = a_names. begin ( ); it != a_names. end ( ); ++it ) {
		const char *a_name = ( *it ). latin1 ( );

		if ( a_name [0] == '-' && a_name [1] == 0 ) {
			p-> insertSeparator ( );
		}
		else {
            QAction *a = findChild<QAction *> ( a_name );
            QActionGroup *ag = findChild<QActionGroup *> ( a_name );
            QMenu *q = findChild<QMenu *> ( a_name );
            CListAction *l = findChild<CListAction *> ( a_name );

			if ( a )
                a-> addTo ( p );
            else if ( ag )
                ag -> addTo ( p );
            else if ( q )
                p->addMenu( q );
            else if ( l )
                p->addMenu( l );
            else
				qWarning ( "Couldn't find action '%s'", a_name );
		}
	}
	return p;
}

QToolBar *CFrameWork::createToolBar ( const QString &label, const QStringList &a_names )
{
	if ( a_names. isEmpty ( ))
		return 0;

    QToolBar *t = new QToolBar ( this );
    this->addToolBar(Qt::TopToolBarArea, t);

	t-> setLabel ( label );

	for ( QStringList::ConstIterator it = a_names. begin ( ); it != a_names. end ( ); ++it ) {
		const char *a_name = ( *it ). latin1 ( );

		if ( a_name [0] == '-' && a_name [1] == 0 ) {
			t-> addSeparator ( );
		}
		else {
            QAction *a = findChild<QAction *> ( a_name );
            QActionGroup *ag = findChild<QActionGroup *> ( a_name );
            QMenu *q = findChild<QMenu *> ( a_name );

            if ( a )
                a-> addTo ( t );
            else if ( ag )
                ag -> addTo ( t );
            else if ( q ) {
                QToolButton* toolButton = new QToolButton();
                const char *tbName = (QString(q->name()) + "_tb").latin1();
                toolButton->setName( tbName );
                toolButton->setMenu( q );
                toolButton->setPopupMode(QToolButton::InstantPopup);
                toolButton->setIcon(q->icon());
                t->addWidget(toolButton);
            }
			else
                qWarning ( "Couldn't find action '%s'", a_name );
		}
	}

    t->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);

    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    t->addWidget(spacer);

    m_spinner = new CSpinner ( t, "spinner" );
	QToolTip::add ( m_spinner, tr( "Download activity indicator" ));
	m_spinner-> setPixmap ( CResource::inst ( )-> pixmap ( "spinner" ));
	m_spinner-> show ( );
    t->addWidget( m_spinner );

	t-> hide ( );
	return t;
}

void CFrameWork::createActions ( )
{
    QAction *a;
    CListAction *l;
    QActionGroup *g;
    QMenu *q;

    Q3Accel *acc = new Q3Accel ( this );

    //File Menu
    {
        a = new QAction ( this, "file_new" );connect ( a, SIGNAL( activated ( )), this, SLOT( fileNew ( )));
        a = new QAction ( this, "file_open" );connect ( a, SIGNAL( activated ( )), this, SLOT( fileOpen ( )));

        l = new CListAction ( true, this, "file_open_recent" );
        l-> setListProvider ( new RecentListProvider ( this ));
        l-> setIcon (CResource::inst ( )-> icon ( "file_open" ));
        connect ( l, SIGNAL( activated ( int )), this, SLOT( fileOpenRecent ( int )));

        a = new QAction ( this, "file_save" );
        a = new QAction ( this, "file_saveas" );
        a = new QAction ( this, "file_print" );

        //ImportMenu
        {
            q = new QMenu ("file_import", this);
            q->setObjectName("file_import");
            q->setIcon(CResource::inst ( )-> icon ( "file_import" ));

            a = new QAction ( this, "file_import_bl_inv" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkInventory ( )));
            a = new QAction ( this, "file_import_bl_xml" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkXML ( )));
            a = new QAction ( this, "file_import_bl_order" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkOrder ( )));
            a = new QAction ( this, "file_import_bl_store_inv" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkStore ( )));
            a = new QAction ( this, "file_import_bl_cart" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkCart ( )));
            a = new QAction ( this, "file_import_peeron_inv" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportPeeronInventory ( )));
            a = new QAction ( this, "file_import_ldraw_model" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportLDrawModel ( )));
            a = new QAction ( this, "file_import_briktrak" );q->addAction(a);connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrikTrakInventory ( )));
        }

        //Export Menu
        {
            q = new QMenu ("file_export", this);
            q->setObjectName("file_export");
            q->setIcon(CResource::inst ( )-> icon ( "file_export" ));

            a = new QAction ( this, "file_export_bl_xml" );q->addAction(a);
            a = new QAction ( this, "file_export_bl_xml_clip" );q->addAction(a);
            a = new QAction ( this, "file_export_bl_update_clip" );q->addAction(a);
            a = new QAction ( this, "file_export_bl_invreq_clip" );q->addAction(a);
            a = new QAction ( this, "file_export_bl_wantedlist_clip" );q->addAction(a);
            a = new QAction ( this, "file_export_briktrak" );q->addAction(a);
        }

        a = new QAction ( this, "file_close" );connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( closeActiveSubWindow ( )));
        a = new QAction ( this, "file_exit" );connect ( a, SIGNAL( activated ( )), this, SLOT( close ( )));
    }

    //Edit Menu
    {
        a = CUndoManager::inst ( )-> createUndoAction ( this, "edit_undo" );
        a = CUndoManager::inst ( )-> createRedoAction ( this, "edit_redo" );

        a = new QAction ( this, "edit_cut" );acc-> connectItem ( acc-> insertItem ( Qt::SHIFT  + Qt::Key_Delete ), a, SLOT( trigger (  ))); // old style cut
        a = new QAction ( this, "edit_copy" );acc-> connectItem ( acc-> insertItem ( Qt::SHIFT + Qt::Key_Insert ), a, SLOT( trigger ( ))); // old style copy
        a = new QAction ( this, "edit_paste" );acc-> connectItem ( acc-> insertItem ( Qt::CTRL  + Qt::Key_Insert ), a, SLOT( trigger ( ))); // old style paste
        a = new QAction ( this, "edit_delete" );

        a = new QAction ( this, "edit_select_all" );
        a = new QAction ( this, "edit_select_none" );

        a = new QAction ( this, "edit_additems" );a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), this, SLOT( toggleAddItemDialog ( bool )));
        a = new QAction ( this, "edit_subtractitems" );
        a = new QAction ( this, "edit_mergeitems" );
        a = new QAction ( this, "edit_partoutitems" );

        q = new QMenu ("edit_status", this);
        q->setObjectName("edit_status");
        q->setIcon(CResource::inst ( )-> icon ( "edit_status" ));

        a = new QAction ( q, "edit_status_include" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "status_include" ));q->addAction( a );
        a = new QAction ( q, "edit_status_exclude" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "status_exclude" ));q->addAction( a );
        a = new QAction ( q, "edit_status_extra" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "status_extra" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_status_toggle" );q->addAction( a );

        q = new QMenu ("edit_cond", this);
        q->setObjectName("edit_cond");
        q->setIcon(CResource::inst ( )-> icon ( "edit_cond" ));
        a = new QAction ( q, "edit_cond_new" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_cond_new" ));q->addAction( a );
        a = new QAction ( q, "edit_cond_used" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_cond_used" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_cond_toggle" );q->addAction( a );

        a = new QAction ( this, "edit_color" );

        q = new QMenu ("edit_qty", this);
        q->setObjectName("edit_qty");
        q->setIcon(CResource::inst ( )-> icon ( "edit_qty" ));
        a = new QAction ( q, "edit_qty_multiply" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_qty_multiply" ));q->addAction( a );
        a = new QAction ( q, "edit_qty_divide" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_qty_divide" ));q->addAction( a );

        q = new QMenu ("edit_price", this);
        q->setObjectName("edit_price");
        q->setIcon(CResource::inst ( )-> icon ( "edit_price" ));
        a = new QAction ( q, "edit_price_set" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_price_set" ));q->addAction( a );
        a = new QAction ( q, "edit_price_inc_dec" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_price_inc_dec" ));q->addAction( a );
        a = new QAction ( q, "edit_price_to_priceguide" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_price_to_priceguide" ));q->addAction( a );
        a = new QAction ( q, "edit_price_round" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_price_round" ));q->addAction( a );

        a = new QAction ( this, "edit_bulk" );
        a = new QAction ( this, "edit_sale" );

        q = new QMenu ("edit_comment", this);
        q->setObjectName("edit_comment");
        q->setIcon(CResource::inst ( )-> icon ( "edit_comment" ));
        a = new QAction ( q, "edit_comment_set" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_comment_set" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_comment_add" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_comment_add" ));q->addAction( a );
        a = new QAction ( q, "edit_comment_rem" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_comment_rem" ));q->addAction( a );

        q = new QMenu ("edit_remark", this);
        q->setObjectName("edit_remark");
        q->setIcon(CResource::inst ( )-> icon ( "edit_remark" ));
        a = new QAction ( q, "edit_remark_set" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_remark_set" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_remark_add" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_remark_add" ));q->addAction( a );
        a = new QAction ( q, "edit_remark_rem" );a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_remark_rem" ));q->addAction( a );

        q = new QMenu ("edit_retain", this);
        q->setObjectName("edit_retain");
        q->setIcon(CResource::inst ( )-> icon ( "edit_retain" ));
        a = new QAction ( q, "edit_retain_yes" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_retain_yes" ));q->addAction( a );
        a = new QAction ( q, "edit_retain_no" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_retain_no" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_retain_toggle" );q->addAction( a );

        q = new QMenu ("edit_stockroom", this);
        q->setObjectName("edit_stockroom");
        q->setIcon(CResource::inst ( )-> icon ( "edit_stockroom" ));
        a = new QAction ( q, "edit_stockroom_yes" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_stockroom_yes" ));q->addAction( a );
        a = new QAction ( q, "edit_stockroom_no" );a->setCheckable(true);a-> setIconSet ( CResource::inst ( )-> pixmap ( "edit_stockroom_no" ));q->addAction( a );
        q->addSeparator();
        a = new QAction ( q, "edit_stockroom_toggle" );q->addAction( a );

        a = new QAction ( this, "edit_reserved" );

        a = new QAction ( this, "edit_reset_diffs" );
        a = new QAction ( this, "edit_copyremarks" );

        a = new QAction ( this, "edit_bl_catalog" );
        a = new QAction ( this, "edit_bl_priceguide" );
        a = new QAction ( this, "edit_bl_lotsforsale" );
        a = new QAction ( this, "edit_bl_myinventory" );
    }

    //View Menu
    {
        a = new QAction ( this, "view_toolbar" );a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewToolBar ( bool )));
        a = new QAction ( this, "view_statusbar" );a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewStatusBar ( bool )));
        a = new QAction ( this, "view_fullscreen" );a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewFullScreen ( bool )));

        q = m_taskpanes-> createItemVisibilityAction ( this, "view_infobar" );

        a = new QAction ( this, "view_simple_mode");a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setSimpleMode ( bool )));
        a = new QAction ( this, "view_show_input_errors" );a->setCheckable(true);connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setShowInputErrors ( bool )));
        a = new QAction ( this, "view_difference_mode" );a->setCheckable(true);

        a = new QAction ( this, "view_save_default_col" );
    }

    //Extras Menu
    {
        g = new QActionGroup ( this );g->setExclusive(true);g->setObjectName( "extras_net" );connect ( g, SIGNAL( selected ( QAction * )), this, SLOT( setOnlineStatus ( QAction * )));
        a = new QAction ( g, "extras_net_online" );a->setCheckable(true);
        a = new QAction ( g, "extras_net_offline" );a->setCheckable(true);

        a = new QAction ( this, "extras_update_database" );connect ( a, SIGNAL( activated ( )), this, SLOT( updateDatabase ( )));

        a = new QAction ( this, "extras_configure" );connect ( a, SIGNAL( activated ( )), this, SLOT( configure ( )));
    }

    //Windows Menu
    {
        g = new QActionGroup ( this );g->setExclusive(true);g->setObjectName( "window_mode" );connect ( g, SIGNAL( selected ( QAction * )), this, SLOT( setWindowMode ( QAction * )));
        a = new QAction ( g, "window_mode_mdi" );a->setCheckable(true);
        a = new QAction ( g, "window_mode_tab_above" );a->setCheckable(true);
        a = new QAction ( g, "window_mode_tab_below" );a->setCheckable(true);

        a = new QAction ( this, "window_cascade" );connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( cascadeSubWindows ( )));
        a = new QAction ( this, "window_tile" );connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( tileSubWindows ( )));

        l = new CListAction ( true, this, "window_list" );
        l-> setListProvider ( new WindowListProvider ( this ));
        connect ( l, SIGNAL( activated ( int )), this, SLOT( windowActivate ( int )));
    }

    //Help Menu
    {
        //a = new QAction ( this, "help_whatsthis" );connect ( a, SIGNAL( activated ( )), this, SLOT( whatsThis ( )));
        a = new QAction ( this, "help_about" );connect ( a, SIGNAL( activated ( )), cApp, SLOT( about ( )));
        a = new QAction ( this, "help_updates" );connect ( a, SIGNAL( activated ( )), cApp, SLOT( checkForUpdates ( )));
        a = new QAction ( this, "help_registration" );a-> setEnabled ( CConfig::inst ( )-> registration ( ) != CConfig::OpenSource );connect ( a, SIGNAL( activated ( )), cApp, SLOT( registration ( )));
    }

	// set all icons that have a pixmap corresponding to name()

    foreach (QObject *obj, queryList ( "QAction", 0, false, true )) {
        const char *name = obj-> name ( );

        if ( name && name [0] ) {
            QIcon set = CResource::inst ( )-> icon ( name );

            if ( !set. isNull ( ))
                static_cast <QAction *> ( obj )-> setIconSet ( set );
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

void CFrameWork::viewFullScreen ( bool b )
{
	int ws = windowState ( );
    setWindowState ( (Qt::WindowState)(b ? ws | Qt::WindowFullScreen : ws & ~Qt::WindowFullScreen) );
}

void CFrameWork::openDocument ( const QString &file )
{
	createWindow ( CDocument::fileOpen ( file ));
}

void CFrameWork::fileNew ( )
{
	createWindow ( CDocument::fileNew ( ));
}

void CFrameWork::fileOpen ( )
{
	createWindow ( CDocument::fileOpen ( ));
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

void CFrameWork::fileImportPeeronInventory ( )
{
	createWindow ( CDocument::fileImportPeeronInventory ( ));
}

void CFrameWork::fileImportBrikTrakInventory ( )
{
	createWindow ( CDocument::fileImportBrikTrakInventory ( ));
}

void CFrameWork::fileImportBrickLinkInventory ( )
{
	fileImportBrickLinkInventory ( 0 );
}

void CFrameWork::fileImportBrickLinkInventory ( const BrickLink::Item *item )
{
	createWindow ( CDocument::fileImportBrickLinkInventory ( item ));
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

	createWindows ( CDocument::fileImportBrickLinkOrders ( ));
}

void CFrameWork::fileImportBrickLinkStore ( )
{
	if ( !checkBrickLinkLogin ( ))
		return;

	createWindow ( CDocument::fileImportBrickLinkStore ( ));
}

void CFrameWork::fileImportBrickLinkXML ( )
{
	createWindow ( CDocument::fileImportBrickLinkXML ( ));
}

void CFrameWork::fileImportBrickLinkCart ( )
{
	createWindow ( CDocument::fileImportBrickLinkCart ( ));
}

void CFrameWork::fileImportLDrawModel ( )
{
	createWindow ( CDocument::fileImportLDrawModel ( ));
}

bool CFrameWork::createWindows ( const Q3ValueList<CDocument *> &docs )
{
    bool ok = true;

    foreach ( CDocument *doc, docs ) {
        ok &= createWindow ( doc );
    }
    return ok;
}

bool CFrameWork::createWindow ( CDocument *doc )
{
	if ( !doc )
		return false;

    QList <CWindow *> list = allWindows ( );
    QListIterator<CWindow *> i(list);

    while (i.hasNext()) {
        CWindow *w = i.next();
        if (w->document() == doc) {
            w->setFocus();
            return true;
        }
    }

    CWindow *w = new CWindow ( doc, 0, "window" );
    QMdiSubWindow *sw = new QMdiSubWindow();
    sw->setWidget(w);
    sw->setAttribute(Qt::WA_DeleteOnClose);
    m_mdi->addSubWindow(sw);

	QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
	// Qt/Win32 bug: MDI child window icon()s have to be 16x16 or smaller...
//	pix. convertFromImage ( pix. convertToImage ( ). smoothScale ( pix. size ( ). boundedTo ( QSize ( 16, 16 ))));
	if ( !pix. isNull ( ))
		w-> setIcon ( pix );

    QWidget *act = m_mdi-> activeSubWindow ( );

	if ( !act || ( act == w ) || ( act-> isMaximized ( )))
		w-> showMaximized ( );
	else
		w-> show ( );

	return true;
}


bool CFrameWork::updateDatabase ( )
{
    m_mdi-> closeAllSubWindows ( );
    if ( !m_mdi-> currentSubWindow ( )) {
		delete m_add_dialog;

		CProgressDialog d ( this );
		CUpdateDatabase update ( &d );

		return d. exec ( );
	}
	return false;
}

void CFrameWork::windowActivate ( int i )
{
    QList<QMdiSubWindow *> l = m_mdi-> subWindowList ( QMdiArea::CreationOrder );

	if (( i >= 0 ) && ( i < int( l. count ( )))) {
        QMdiSubWindow *w = l. at ( i );

		w-> setFocus ( );
	}
}

void CFrameWork::connectAction ( bool do_connect, const char *name, CWindow *window, const char *windowslot, bool ( CWindow::* is_on_func ) ( ) const )
{
    QAction *a = findChild<QAction *> ( name );

	if ( a ) {
        QObject *a2 = a;

        while ( a2 && ::qobject_cast<QActionGroup *> ( a2-> parent ( )))
            a2 = static_cast<QAction *> ( a2-> parent ( ));

        if (a2)
            static_cast<QAction *> (a2)-> setEnabled ( do_connect );

        a2 = a;

        while ( a2 && ::qobject_cast<QMenu *> ( a2-> parent ( )))
            a2 = static_cast<QMenu *> ( a2-> parent ( ));

        if (::qobject_cast<QMenu *>(a2))
            static_cast<QMenu *> (a2)-> setEnabled ( do_connect );
    }

    QMenu *q = findChild<QMenu *> ( name );

    if ( q ) {
        q->setEnabled( do_connect );

        QToolButton *tb = findChild<QToolButton *>( (QString(name) + "_tb").latin1() );
        if (tb)
                tb->setEnabled( do_connect );
    }

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
        it. key ( )-> setChecked (( window->* it. data ( )) ( ));
	}
}

void CFrameWork::connectAllActions ( bool do_connect, CWindow *window )
{
	m_toggle_updates. clear ( );

	connectAction ( do_connect, "file_save", window, SLOT( fileSave ( )));
	connectAction ( do_connect, "file_saveas", window, SLOT( fileSaveAs ( )));
    connectAction ( do_connect, "file_print", window, SLOT( filePrint ( )));
	connectAction ( do_connect, "file_export", 0, 0 );
	connectAction ( do_connect, "file_export_briktrak", window, SLOT( fileExportBrikTrakInventory ( )));
	connectAction ( do_connect, "file_export_bl_xml", window, SLOT( fileExportBrickLinkXML ( )));
	connectAction ( do_connect, "file_export_bl_xml_clip", window, SLOT( fileExportBrickLinkXMLClipboard ( )));
	connectAction ( do_connect, "file_export_bl_update_clip", window, SLOT( fileExportBrickLinkUpdateClipboard ( )));
	connectAction ( do_connect, "file_export_bl_invreq_clip", window, SLOT( fileExportBrickLinkInvReqClipboard ( )));
	connectAction ( do_connect, "file_export_bl_wantedlist_clip", window, SLOT( fileExportBrickLinkWantedListClipboard ( )));
    connectAction ( do_connect, "file_close", 0, 0 );//window, SLOT( close ( )));

	connectAction ( do_connect, "edit_cut", window, SLOT( editCut ( )));
	connectAction ( do_connect, "edit_copy", window, SLOT( editCopy ( )));
	connectAction ( do_connect, "edit_paste", window, SLOT( editPaste ( )));
	connectAction ( do_connect, "edit_delete", window, SLOT( editDelete ( )));

	connectAction ( do_connect, "edit_select_all", window, SLOT( selectAll ( )));
	connectAction ( do_connect, "edit_select_none", window, SLOT( selectNone ( )));

	connectAction ( do_connect, "edit_subtractitems", window, SLOT( editSubtractItems ( )));
	connectAction ( do_connect, "edit_mergeitems", window, SLOT( editMergeItems ( )));
	connectAction ( do_connect, "edit_partoutitems", window, SLOT( editPartOutItems ( )));
	connectAction ( do_connect, "edit_reset_diffs", window, SLOT( editResetDifferences ( )));
	connectAction ( do_connect, "edit_copyremarks", window, SLOT( editCopyRemarks ( )));

	connectAction ( do_connect, "edit_status_include", window, SLOT( editStatusInclude ( )));
	connectAction ( do_connect, "edit_status_exclude", window, SLOT( editStatusExclude ( )));
	connectAction ( do_connect, "edit_status_extra", window, SLOT( editStatusExtra ( )));
	connectAction ( do_connect, "edit_status_toggle", window, SLOT( editStatusToggle ( )));
	
    connectAction ( do_connect, "edit_cond_new", window, SLOT( editConditionNew ( )));
    connectAction ( do_connect, "edit_cond_used", window, SLOT( editConditionUsed ( )));
    connectAction ( do_connect, "edit_cond_toggle", window, SLOT( editConditionToggle ( )));

    connectAction ( do_connect, "edit_color", window, SLOT( editColor ( )));

    connectAction ( do_connect, "edit_qty_multiply", window, SLOT( editQtyMultiply ( )));
    connectAction ( do_connect, "edit_qty_divide", window, SLOT( editQtyDivide ( )));

    connectAction ( do_connect, "edit_price_set", window, SLOT( editPrice ( )));
    connectAction ( do_connect, "edit_price_round", window, SLOT( editPriceRound ( )));
    connectAction ( do_connect, "edit_price_to_priceguide", window, SLOT( editPriceToPG ( )));
    connectAction ( do_connect, "edit_price_inc_dec", window, SLOT( editPriceIncDec ( )));

    connectAction ( do_connect, "edit_bulk", window, SLOT( editBulk ( )));
    connectAction ( do_connect, "edit_sale", window, SLOT( editSale ( )));
    connectAction ( do_connect, "edit_comment_set", window, SLOT( editComment ( )));
    connectAction ( do_connect, "edit_comment_add", window, SLOT( addComment ( )));
    connectAction ( do_connect, "edit_comment_rem", window, SLOT( removeComment ( )));
    connectAction ( do_connect, "edit_remark_set", window, SLOT( editRemark ( )));
    connectAction ( do_connect, "edit_remark_add", window, SLOT( addRemark ( )));
    connectAction ( do_connect, "edit_remark_rem", window, SLOT( removeRemark ( )));

    connectAction ( do_connect, "edit_retain_yes", window, SLOT( editRetainYes ( )));
    connectAction ( do_connect, "edit_retain_no", window, SLOT( editRetainNo ( )));
    connectAction ( do_connect, "edit_retain_toggle", window, SLOT( editRetainToggle ( )));

    connectAction ( do_connect, "edit_stockroom_yes", window, SLOT( editStockroomYes ( )));
    connectAction ( do_connect, "edit_stockroom_no", window, SLOT( editStockroomNo ( )));
    connectAction ( do_connect, "edit_stockroom_toggle", window, SLOT( editStockroomToggle ( )));

    connectAction ( do_connect, "edit_reserved", window, SLOT( editReserved ( )));

    connectAction ( do_connect, "edit_bl_catalog", window, SLOT( showBLCatalog ( )));
    connectAction ( do_connect, "edit_bl_priceguide", window, SLOT( showBLPriceGuide ( )));
    connectAction ( do_connect, "edit_bl_lotsforsale", window, SLOT( showBLLotsForSale ( )));
    connectAction ( do_connect, "edit_bl_myinventory", window, SLOT( showBLMyInventory ( )));

	connectAction ( do_connect, "view_difference_mode", window, SLOT( setDifferenceMode ( bool )), &CWindow::isDifferenceMode );
	connectAction ( do_connect, "view_save_default_col", window, SLOT ( saveDefaultColumnLayout ( )));

	updateAllToggleActions ( window );
}

void CFrameWork::connectWindow ( QMdiSubWindow *w )
{
    if ( w && ( w->widget() == m_current_window ))
		return;

	if ( m_current_window ) {
		CDocument *doc = m_current_window-> document ( );

        connectAllActions ( false, m_current_window );

		disconnect ( doc, SIGNAL( statisticsChanged ( )), this, SLOT( statisticsUpdate ( )));
		disconnect ( doc, SIGNAL( selectionChanged ( const CDocument::ItemList & )), this, SLOT( selectionUpdate ( const CDocument::ItemList & )));

		m_current_window = 0;
	}

    if ( w && ::qobject_cast <CWindow *> ( w->widget() )) {
        CWindow *window = static_cast <CWindow *> ( w->widget() );
		CDocument *doc = window-> document ( );

        connectAllActions ( true, window );

		connect ( doc, SIGNAL( statisticsChanged ( )), this, SLOT( statisticsUpdate ( )));
		connect ( doc, SIGNAL( selectionChanged ( const CDocument::ItemList & )), this, SLOT( selectionUpdate ( const CDocument::ItemList & )));

        m_mdi-> setActiveSubWindow ( w );
		m_current_window = window;
	}

	if ( m_add_dialog ) {
		m_add_dialog-> attach ( m_current_window );
		if ( !m_current_window )
			m_add_dialog-> close ( );
	}
    findChild<QAction *> ( "edit_additems" )-> setEnabled (( m_current_window ));

	selectionUpdate ( m_current_window ? m_current_window-> document ( )-> selection ( ) : CDocument::ItemList ( ));
	statisticsUpdate ( );
	modificationUpdate ( );

    emit windowActivated ( m_current_window );
    emit documentActivated ( m_current_window ? m_current_window-> document ( ) : 0 );
}

void CFrameWork::selectionUpdate ( const CDocument::ItemList &selection )
{
	static const int NeedLotId = 1;
	static const int NeedInventory = 2;

	struct {
		const char *m_name;
		uint m_minsel;
		uint m_maxsel;
		int  m_flags;
	} endisable_actions [] = {
		{ "edit_cut",                 1, 0, 0 },
		{ "edit_copy",                1, 0, 0 },
		{ "edit_delete",              1, 0, 0 },
        { "edit_status",              1, 0, 0 },
        { "edit_cond",                1, 0, 0 },
        { "edit_color",               1, 0, 0 },
        { "edit_qty",                 1, 0, 0 },
        { "edit_price",               1, 0, 0 },
        { "edit_bulk",                1, 0, 0 },
        { "edit_sale",                1, 0, 0 },
        { "edit_comment",             1, 0, 0 },
        { "edit_remark",              1, 0, 0 },
        { "edit_retain",              1, 0, 0 },
        { "edit_stockroom",           1, 0, 0 },
        { "edit_reserved",            1, 0, 0 },
        { "edit_bl_catalog",          1, 1, 0 },
		{ "edit_bl_priceguide",       1, 1, 0 },
		{ "edit_bl_lotsforsale",      1, 1, 0 },
		{ "edit_bl_myinventory",      1, 1, NeedLotId },
		{ "edit_mergeitems",          2, 0, 0 },
		{ "edit_partoutitems",        1, 0, NeedInventory },
		{ "edit_reset_diffs",         1, 0, 0 },

		{ 0, 0, 0, 0 }
	}, *endisable_ptr;

	uint cnt = selection. count ( );

	for ( endisable_ptr = endisable_actions; endisable_ptr-> m_name; endisable_ptr++ ) {
        QAction *a = findChild<QAction *> ( endisable_ptr-> m_name );

		if ( a ) {
			uint &mins = endisable_ptr-> m_minsel;
			uint &maxs = endisable_ptr-> m_maxsel;

			bool b = true;

			if ( endisable_ptr-> m_flags )
			{
				int f = endisable_ptr-> m_flags;

				foreach ( CDocument::Item *item, selection ) {
					if ( f & NeedLotId )
						b &= ( item-> lotId ( ) != 0 );
					if ( f & NeedInventory )
						b &= ( item-> item ( ) && item-> item ( )-> hasInventory ( ));
				}
			}

			a-> setEnabled ( b && ( mins ? ( cnt >= mins ) : true ) && ( maxs ? ( cnt <= maxs ) : true ));
		}
	}

	int status    = -1;
	int condition = -1;
	int retain    = -1;
	int stockroom = -1;

	if ( cnt ) {
		status    = selection. front ( )-> status ( );
		condition = selection. front ( )-> condition ( );
		retain    = selection. front ( )-> retain ( )    ? 1 : 0;
		stockroom = selection. front ( )-> stockroom ( ) ? 1 : 0;

		foreach ( CDocument::Item *item, selection ) {
			if (( status >= 0 ) && ( status != item-> status ( )))
				status = -1;
			if (( condition >= 0 ) && ( condition != item-> condition ( )))
				condition = -1;
			if (( retain >= 0 ) && ( retain != ( item-> retain ( ) ? 1 : 0 )))
				retain = -1;
			if (( stockroom >= 0 ) && ( stockroom != ( item-> stockroom ( ) ? 1 : 0 )))
				stockroom = -1;
		}
	}
    findChild<QAction *> ( "edit_status_include" )-> setChecked ( status == BrickLink::InvItem::Include );
    findChild<QAction *> ( "edit_status_exclude" )-> setChecked ( status == BrickLink::InvItem::Exclude );
    findChild<QAction *> ( "edit_status_extra" )-> setChecked ( status == BrickLink::InvItem::Extra );

    findChild<QAction *> ( "edit_cond_new" )-> setChecked ( condition == BrickLink::New );
    findChild<QAction *> ( "edit_cond_used" )-> setChecked ( condition == BrickLink::Used );

    findChild<QAction *> ( "edit_retain_yes" )-> setChecked ( retain == 1 );
    findChild<QAction *> ( "edit_retain_no" )-> setChecked ( retain == 0 );

    findChild<QAction *> ( "edit_stockroom_yes" )-> setChecked ( stockroom == 1 );
    findChild<QAction *> ( "edit_stockroom_no" )-> setChecked ( stockroom == 0 );
}

void CFrameWork::newVersionAvailable  ( ) {
    m_newversion-> setOpenExternalLinks( false );
    m_newversion-> setText ( QString ( "<a href=\"#newversion\">%1</a>" ) .arg( tr ( "Update available!" )) );
    m_newversion->setTextFormat(Qt::RichText);
}

void CFrameWork::statisticsUpdate ( )
{
	QString ss, es;

	if ( m_current_window )
	{
		CDocument::ItemList not_exclude;

		foreach ( CDocument::Item *item, m_current_window-> document ( )-> items ( )) {
			if ( item-> status ( ) != BrickLink::InvItem::Exclude )
				not_exclude. append ( item );
		}

		CDocument::Statistics stat = m_current_window-> document ( )-> statistics ( not_exclude );
		QString valstr, wgtstr;

		if ( stat. value ( ) != stat. minValue ( )) {
			valstr = QString ( "%1 (%2 %3)" ).
					arg( stat. value ( ). toLocalizedString ( true )).
					arg( tr( "min." )).
					arg( stat. minValue ( ). toLocalizedString ( true ));
		}
		else
			valstr = stat. value ( ). toLocalizedString ( true );

		if ( stat. weight ( ) == -DBL_MIN ) {
			wgtstr = "-";
		}
		else {
			double weight = stat. weight ( );

			if ( weight < 0 ) {
				weight = -weight;
				wgtstr = tr( "min." ) + " ";
			}

			wgtstr += CUtility::weightToString ( weight, ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), true, true );
		}

		ss = QString( "  %1: %2   %3: %4   %5: %6   %7: %8  " ).
		     arg( tr( "Lots" )). arg ( stat. lots ( )).
		     arg( tr( "Items" )). arg ( stat. items ( )).
		     arg( tr( "Value" )). arg ( valstr ).
		     arg( tr( "Weight" )). arg ( wgtstr );

		if (( stat. errors ( ) > 0 ) && CConfig::inst ( )-> showInputErrors ( ))
			es = QString( "  %1: %2  " ). arg( tr( "Errors" )). arg( stat. errors ( ));
	}
	
	m_statistics-> setText ( ss );
	m_errors-> setText ( es );

	emit statisticsChanged ( m_current_window );
}

void CFrameWork::modificationUpdate ( )
{
	bool b = CUndoManager::inst ( )-> currentStack ( ) ? CUndoManager::inst ( )-> currentStack ( )-> isClean ( ) : true;

    QAction *a = findChild<QAction *> ( "file_save" );
	if ( a )
		a-> setEnabled ( !b );

	static QPixmap pnomod, pmod;

	if ( pmod. isNull ( )) {
		int h = m_modified-> height ( ) - 2;

		pmod. resize ( h, h );
		pmod. fill ( Qt::black );

		pnomod = pmod;
		pnomod. setMask ( pnomod. createHeuristicMask ( ));

		QPixmap p = CResource::inst ( )-> pixmap ( "status/modified" );
		if ( !p. isNull ( ))
			pmod. convertFromImage ( p. convertToImage ( ). smoothScale ( p. size ( ). boundedTo ( QSize ( h, h ))));
	}
	m_modified-> setPixmap ( b ? pnomod : pmod );

	if ( b )
		QToolTip::remove ( m_modified );
	else
		QToolTip::add ( m_modified, tr( "Unsaved modifications" ));
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

void CFrameWork::setWindowMode ( QAction *act )
{
    bool tabbed = ( act != findChild<QAction *> ( "window_mode_mdi" ));
    bool execllike = ( act == findChild<QAction *> ( "window_mode_tab_below" ));

    m_mdi-> setViewMode (tabbed ? QMdiArea::TabbedView : QMdiArea::SubWindowView);
    m_mdi-> setTabPosition ( execllike ? QTabWidget::South : QTabWidget::North );
    m_mdi-> setTabShape ( execllike ? QTabWidget::Triangular : QTabWidget::Rounded );
	
    if (!tabbed && m_mdi-> currentSubWindow ( ))
        m_mdi-> currentSubWindow ( )-> showMaximized ( );

    findChild<QAction *> ( "window_cascade" )-> setEnabled ( !tabbed );
    findChild<QAction *> ( "window_tile" )-> setEnabled ( !tabbed );
}

void CFrameWork::setOnlineStatus ( QAction *act )
{
    bool b = ( act == findChild<QAction *> ( "extras_net_online" ));

	if ( !b && m_running )
		cancelAllTransfers ( );
		
	CConfig::inst ( )-> setOnlineStatus ( b );
}

void CFrameWork::registrationUpdate ( )
{
	bool personal = ( CConfig::inst ( )-> registration ( ) == CConfig::Personal );
	bool demo = ( CConfig::inst ( )-> registration ( ) == CConfig::Demo );
    bool full = ( CConfig::inst ( )-> registration ( ) == CConfig::Full );

    QAction *a = findChild<QAction *> ( "view_simple_mode" );
	
	// personal -> always on
	// demo, full -> always off
    // opensource -> don't change

    if ( personal )
        a-> setChecked ( true );
    else if ( demo || full )
        a-> setChecked ( false );
    else
        a-> setChecked ( CConfig::inst ( )-> simpleMode ( ));

    a-> setEnabled ( !personal );
    setSimpleMode ( a-> isOn ( ));
}

void CFrameWork::setSimpleMode ( bool b )
{
	static const char *actions [] = {
		"file_import_bl_xml",
		"file_import_bl_store_inv",
		"file_export_bl_xml",
		"file_export_bl_xml_clip",
		"file_export_bl_update_clip",
		"edit_bulk",
		"edit_sale",
		"edit_reserved",
		"edit_retain",
		"edit_stockroom",
		"edit_comment",
		"edit_bl_myinventory",

		0
	};

	for ( const char **action_ptr = actions; *action_ptr; action_ptr++ ) {
        QAction *a = findChild<QAction *> ( *action_ptr );

		if ( a )
			a-> setVisible ( !b );
	}
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
    m_mdi-> closeAllSubWindows ( );
    if ( m_mdi-> currentSubWindow ( )) {
		e-> ignore ( );
		return;
	}

    QMainWindow::closeEvent ( e );
}

QList <CWindow *> CFrameWork::allWindows ( ) const
{
    QList <CWindow *> list;

    QList<QMdiSubWindow *> wl = m_mdi-> subWindowList ( QMdiArea::CreationOrder );

	if ( !wl. isEmpty ( )) {
        for ( int i = 0; i < wl. count ( ); i++ ) {
            CWindow *w = ::qobject_cast <CWindow *> ( wl. at ( i )->widget ( ));
			if ( w )
				list. append ( w );
		}
	}
	return list;
}

void CFrameWork::cancelAllTransfers ( )
{
	if ( CMessageBox::question ( this, tr( "Do you want to cancel all outstanding inventory, image and Price Guide transfers?" ), CMessageBox::Yes | CMessageBox::Default, CMessageBox::No | CMessageBox::Escape ) == CMessageBox::Yes ) {
		BrickLink::inst ( )-> cancelPictureTransfers ( );
		BrickLink::inst ( )-> cancelPriceGuideTransfers ( );
	}
}

void CFrameWork::toggleAddItemDialog ( bool b )
{
	if ( !m_add_dialog ) {
		m_add_dialog = new DlgAddItemImpl ( this, "additems", false );
		
		QRect r ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/Left",   -1 ),
		          CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/Top",    -1 ),
		          CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/Width",  -1 ),
		          CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/Height", -1 ));

		int wstate = CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/State", Qt::WindowNoState );

		// make sure at least SOME part of the window ends up on the screen
		if ( r. isValid ( ) && !r. isEmpty ( ) && !( r & qApp-> desktop ( )-> rect ( )). isEmpty ( )) {
			m_add_dialog-> resize ( r. size ( ));  // X11 compat. mode -- KDE 3.x doesn't seem to like this though...
			m_add_dialog-> move ( r. topLeft ( ));
			//m_add_dialog-> setGeometry ( r );  // simple code (which shouldn't work on X11, but does on KDE 3.x...)
		}
		m_normal_geometry_adddlg = QRect ( m_add_dialog-> pos ( ), m_add_dialog-> size ( ));
        m_add_dialog-> setWindowState ( (Qt::WindowState)(wstate & Qt::WindowMaximized) );

		Q3Accel *acc = new Q3Accel ( m_add_dialog );

        QAction *action = findChild<QAction *> ( "edit_additems" );
		acc-> connectItem ( acc-> insertItem ( action-> accel ( )), action, SLOT( toggle ( )));
        if ( action-> accel ( )[0] == Qt::Key_Insert ) {
            acc-> insertItem ( Qt::SHIFT + Qt::Key_Insert ); // ignore old style copy
            acc-> insertItem ( Qt::CTRL  + Qt::Key_Insert ); // ignore old style paste
        }
		
		connect ( m_add_dialog, SIGNAL( closed ( )), this, SLOT( closedAddItemDialog ( )));

		m_add_dialog-> attach ( m_current_window );
	}

	if ( b ) {
		if ( m_add_dialog-> isVisible ( )) {
			m_add_dialog-> raise ( );
			m_add_dialog-> setActiveWindow ( );
		}
		else {
			m_add_dialog-> show ( );
		}
	}
	else
		m_add_dialog-> close ( );
}

void CFrameWork::closedAddItemDialog ( )
{
    findChild<QAction *> ( "edit_additems" )-> setChecked ( m_add_dialog && m_add_dialog-> isVisible ( ));
}

