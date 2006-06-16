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
#include <qtimer.h>
#include <qlabel.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qobjectlist.h>
#include <qtooltip.h>
#include <qcursor.h>
#include <qaccel.h>

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
#include "cworkspace.h"
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

QStringList CFrameWork::RecentListProvider::list ( int & /*active*/, QValueList <int> & )
{
	return m_fw-> m_recent_files;
}


CFrameWork::WindowListProvider::WindowListProvider ( CFrameWork *fw )
	: CListAction::Provider ( ), m_fw ( fw )
{ }

CFrameWork::WindowListProvider::~WindowListProvider ( )
{ }

QStringList CFrameWork::WindowListProvider::list ( int &active, QValueList <int> & )
{
	QStringList sl;
	QWidgetList l = m_fw-> m_mdi-> windowList ( CWorkspace::CreationOrder );
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
		(void) new CFrameWork ( 0, "FrameWork", Qt::WDestructiveClose );

	return s_inst;
}


CFrameWork::CFrameWork ( QWidget *parent, const char *name, WFlags fl )
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
	setUsesBigPixmaps ( true );

	(void) new CIconFactory ( );

	connect ( cApp, SIGNAL( openDocument ( const QString & )), this, SLOT( openDocument ( const QString & )));

	m_recent_files = CConfig::inst ( )-> readListEntry ( "/Files/Recent" );

	while ( m_recent_files. count ( ) > MaxRecentFiles )
		m_recent_files. pop_back ( );

	m_current_window = 0;

	m_mdi = new CWorkspace ( this );
	connect ( m_mdi, SIGNAL( windowActivated ( QWidget * )), this, SLOT( connectWindow ( QWidget * )));

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
		   << "edit_modify"
		   << "-"
		   << "edit_reset_diffs"
		   << "-"
		   << "edit_bl_info_group";

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
		   << "edit_modify_context"
		   << "-"
		   << "edit_bl_info_group";

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
		   << "-"             // << "edit_bl_info_group" << "-"
		   << "extras_net";   // << "-" << "help_whatsthis";

	m_toolbar = createToolBar ( QString ( ), sl );
	connect ( m_toolbar, SIGNAL( visibilityChanged ( bool )), findAction ( "view_toolbar" ), SLOT( setOn ( bool )));

	createStatusBar ( );
	findAction ( "view_statusbar" )-> setOn ( CConfig::inst ( )-> readBoolEntry ( "/MainWindow/Statusbar/Visible", true ));

	languageChange ( );

	str = CConfig::inst ( )-> readEntry ( "/MainWindow/Layout/DockWindows" );
	if ( str. isEmpty ( )) {
		findAction ( "view_toolbar" )-> setOn ( true );
	}
	else {
		QTextStream ts ( &str, IO_ReadOnly ); 
		ts >> *this; 
	}

	QRect r ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Left",   -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Top",    -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Width",  -1 ),
	          CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/Height", -1 ));

	int wstate = CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/State", WindowNoState );

	// make sure at least SOME part of the window ends up on the screen
	if ( !r. isValid ( ) || r. isEmpty ( ) || ( r & qApp-> desktop ( )-> rect ( )). isEmpty ( )) {
		float dw = qApp-> desktop ( )-> width ( ) / 10.f;
		float dh = qApp-> desktop ( )-> height ( ) / 10.f;

		r = QRect ( int( dw ), int( dh ), int ( 8 * dw ), int( 8 * dh )); 
		wstate = WindowMaximized;
	}

	resize ( r. size ( ));  // X11 compat. mode -- KDE 3.x doesn't seem to like this though...
	move ( r. topLeft ( ));
	//setGeometry ( r );  // simple code (which shouldn't work on X11, but does on KDE 3.x...)

	m_normal_geometry = QRect ( pos ( ), size ( ));

	setWindowState ( wstate & ( /*WindowMinimized |*/ WindowMaximized | WindowFullScreen ));
	findAction ( "view_fullscreen" )-> setOn ( wstate & WindowFullScreen );

	switch ( CConfig::inst ( )-> readNumEntry ( "/MainWindow/Layout/WindowMode", 1 )) {
		case  0: findAction ( "window_mode_mdi" )-> setOn ( true ); break;
		case  1: findAction ( "window_mode_tab_above" )-> setOn ( true ); break;
		case  2:
		default: findAction ( "window_mode_tab_below" )-> setOn ( true ); break;
	}

	BrickLink *bl = BrickLink::inst ( );

	connect ( CConfig::inst ( ), SIGNAL( onlineStatusChanged ( bool )), bl, SLOT( setOnlineStatus ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( blUpdateIntervalsChanged ( int, int )), bl, SLOT( setUpdateIntervals ( int, int )));
	connect ( CConfig::inst ( ), SIGNAL( proxyChanged ( bool, const QString &, int )), bl, SLOT( setHttpProxy ( bool, const QString &, int )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( statisticsUpdate ( )));
	connect ( CConfig::inst ( ), SIGNAL( weightSystemChanged ( CConfig::WeightSystem )), this, SLOT( statisticsUpdate ( )));
	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( setSimpleMode ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( registrationChanged ( CConfig::Registration )), this, SLOT( registrationUpdate ( )));

	findAction ( "view_show_input_errors" )-> setOn ( CConfig::inst ( )-> showInputErrors ( ));

	registrationUpdate ( );

	findAction ( CConfig::inst ( )-> onlineStatus ( ) ? "extras_net_online" : "extras_net_offline" )-> setOn ( true );
	 
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
		{ "file_import_bl_inv",             tr( "BrickLink Inventory..." ),             tr( "Ctrl+I", "File|Import BrickLink Inventory" ) },
		{ "file_import_bl_xml",             tr( "BrickLink XML..." ),                   0 },
		{ "file_import_bl_order",           tr( "BrickLink Order..." ),                 0 },
		{ "file_import_bl_store_inv",       tr( "BrickLink Store Inventory..." ),       0 },
		{ "file_import_bl_cart",            tr( "BrickLink Shopping Cart..." ),         0 },
		{ "file_import_peeron_inv",         tr( "Peeron Inventory..." ),                0 },
		{ "file_import_ldraw_model",        tr( "LDraw Model..." ),                     0 },
		{ "file_import_briktrak",           tr( "BrikTrak Inventory..." ),              0 },
		{ "file_export",                    tr( "Export" ),                             0 },
		{ "file_export_bl_xml",             tr( "BrickLink XML..." ),                   0 },
		{ "file_export_bl_xml_clip",        tr( "BrickLink XML to Clipboard" ),         0 },
		{ "file_export_bl_update_clip",     tr( "BrickLink Mass-Update XML to Clipboard" ), 0 },
		{ "file_export_bl_invreq_clip",     tr( "BrickLink Inventory XML to Clipboard" ),   0 },
		{ "file_export_bl_wantedlist_clip", tr( "BrickLink Wanted List XML to Clipboard" ), 0 },
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
		QAction *a = findAction ( atptr-> action );

		if ( a ) {
			if ( !atptr-> text. isNull ( ))
				a-> setText ( atptr-> text );
			if ( !atptr-> accel. isNull ( ))
				a-> setAccel ( QKeySequence ( atptr-> accel ));
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
	{ QTextStream ts ( &str, IO_WriteOnly ); ts << *this; }

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/DockWindows", str );

	int wstate = windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen );
	QRect wgeo = wstate ? m_normal_geometry : QRect ( pos ( ), size ( ));

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/State",  wstate );
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Left",   wgeo. x ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Top",    wgeo. y ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Width",  wgeo. width ( ));
	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/Height", wgeo. height ( ));

	CConfig::inst ( )-> writeEntry ( "/MainWindow/Layout/WindowMode", m_mdi-> showTabs ( ) ? ( m_mdi-> spreadSheetTabs ( ) ? 2 : 1 ) : 0 );

	if ( m_add_dialog ) {
		int wstate = m_add_dialog-> windowState ( ) & WindowMaximized;
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

void CFrameWork::moveEvent ( QMoveEvent *e )
{
	if (!( windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
		m_normal_geometry. setTopLeft ( pos ( ));
	QMainWindow::moveEvent ( e );
}

void CFrameWork::resizeEvent ( QResizeEvent *e )
{
	if (!( windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
		m_normal_geometry. setSize ( size ( ));
	QMainWindow::resizeEvent ( e );
}

bool CFrameWork::eventFilter ( QObject *o, QEvent *e )
{
	if ( o && ( o == m_add_dialog )) {
		if ( e-> type ( ) == QEvent::Resize ) {
			if (!( m_add_dialog-> windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
				m_normal_geometry_adddlg. setSize ( m_add_dialog-> size ( ));
		}
		else if ( e-> type ( ) == QEvent::Move ) {
			if (!( m_add_dialog-> windowState ( ) & ( WindowMinimized | WindowMaximized | WindowFullScreen )))
				m_normal_geometry_adddlg. setTopLeft ( m_add_dialog-> pos ( ));
		}
	}
	return QMainWindow::eventFilter ( o, e );
}

QAction *CFrameWork::findAction ( const char *name )
{
	return name ? static_cast <QAction *> ( child ( name, "QAction", true )) : 0;
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
	p = CResource::inst ( )-> pixmap ( "status/stop" );
	if ( !p. isNull ( ))
		m_progress-> setStopPixmap ( p );

	m_progress-> addItem ( QString ( ), PGI_PriceGuide );
	m_progress-> addItem ( QString ( ), PGI_Picture    );

	//m_progress-> setProgress ( -1, 100 );

	statusBar ( )-> addWidget ( m_progress, 0, true );

	connect ( m_progress, SIGNAL( stop ( )), this, SLOT( cancelAllTransfers ( )));

	statusBar ( )-> hide ( );
}

QPopupMenu *CFrameWork::createMenu ( const QStringList &a_names )
{
	if ( a_names. isEmpty ( ))
		return 0;

	QPopupMenu *p = new QPopupMenu ( this );
	p-> setCheckable ( true );

	for ( QStringList::ConstIterator it = a_names. begin ( ); it != a_names. end ( ); ++it ) {
		const char *a_name = ( *it ). latin1 ( );

		if ( a_name [0] == '-' && a_name [1] == 0 ) {
			p-> insertSeparator ( );
		}
		else {
			QAction *a = findAction ( a_name );

			if ( a )
				a-> addTo ( p );
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
			else
				qWarning ( "Couldn't find action '%s'", a_name );
		}
	}

	t-> setStretchableWidget ( new QWidget ( t ));
	t-> setHorizontallyStretchable ( true );
	t-> setVerticallyStretchable ( true );

	QWidget *sw1 = new QWidget ( t );
	sw1-> setFixedSize ( 4, 4 );

	m_spinner = new CSpinner ( t, "spinner" );
	QToolTip::add ( m_spinner, tr( "Download activity indicator" ));
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
	QActionGroup *g, *g2, *g3;

	a = new QAction ( this, "file_new" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileNew ( )));

	a = new QAction ( this, "file_open" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileOpen ( )));

	l = new CListAction ( true, this, "file_open_recent" );
	l-> setUsesDropDown ( true );
	l-> setListProvider ( new RecentListProvider ( this ));
	connect ( l, SIGNAL( activated ( int )), this, SLOT( fileOpenRecent ( int )));

	(void) new QAction ( this, "file_save" );
	(void) new QAction ( this, "file_saveas" );
	(void) new QAction ( this, "file_print" );

	g = new QActionGroup ( this, "file_import", false );
	g-> setUsesDropDown ( true );

	a = new QAction ( g, "file_import_bl_inv" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkInventory ( )));

	a = new QAction ( g, "file_import_bl_xml" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkXML ( )));

	a = new QAction ( g, "file_import_bl_order" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkOrder ( )));

	a = new QAction ( g, "file_import_bl_store_inv" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkStore ( )));

	a = new QAction ( g, "file_import_bl_cart" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrickLinkCart ( )));

	a = new QAction ( g, "file_import_peeron_inv" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportPeeronInventory ( )));

	a = new QAction ( g, "file_import_ldraw_model" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportLDrawModel ( )));

	a = new QAction ( g, "file_import_briktrak" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( fileImportBrikTrakInventory ( )));

	g = new QActionGroup ( this, "file_export", false );
	g-> setUsesDropDown ( true );

	(void) new QAction ( g, "file_export_bl_xml" );
	(void) new QAction ( g, "file_export_bl_xml_clip" );
	(void) new QAction ( g, "file_export_bl_update_clip" );
	(void) new QAction ( g, "file_export_bl_invreq_clip" );
	(void) new QAction ( g, "file_export_bl_wantedlist_clip" );
	(void) new QAction ( g, "file_export_briktrak" );

	(void) new QAction ( this, "file_close" );

	a = new QAction ( this, "file_exit" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( close ( )));

	(void) CUndoManager::inst ( )-> createUndoAction ( this, "edit_undo" );
	(void) CUndoManager::inst ( )-> createRedoAction ( this, "edit_redo" );

	(void) new QAction ( this, "edit_cut" );
	(void) new QAction ( this, "edit_copy" );
	(void) new QAction ( this, "edit_paste" );
	(void) new QAction ( this, "edit_delete" );
	a = new QAction ( this, "edit_additems", true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( toggleAddItemDialog ( bool )));

	(void) new QAction ( this, "edit_subtractitems" );
	(void) new QAction ( this, "edit_mergeitems" );
	(void) new QAction ( this, "edit_partoutitems" );
	(void) new QAction ( this, "edit_reset_diffs" );
	(void) new QAction ( this, "edit_select_all" );
	(void) new QAction ( this, "edit_select_none" );

	g = new QActionGroup ( this, "edit_modify", false );
	g3 = new QActionGroup ( this, "edit_modify_context", false );

	g2 = new QActionGroup ( g, "edit_status", false );
	g2-> setUsesDropDown ( true );
	( new QAction ( g2, "edit_status_include", true ))-> setIconSet ( CResource::inst ( )-> pixmap ( "status_include" ));
	( new QAction ( g2, "edit_status_exclude", true ))-> setIconSet ( CResource::inst ( )-> pixmap ( "status_exclude" ));
	( new QAction ( g2, "edit_status_extra",   true ))-> setIconSet ( CResource::inst ( )-> pixmap ( "status_extra" ));
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_status_toggle" );
	g3-> add ( g2 );

	g2 = new QActionGroup ( g, "edit_cond", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_cond_new", true );
	(void) new QAction ( g2, "edit_cond_used", true );
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_cond_toggle" );
	g3-> add ( g2 );

	g3-> add ( new QAction ( g, "edit_color" ));

	g2 = new QActionGroup ( g, "edit_qty", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_qty_multiply" );
	(void) new QAction ( g2, "edit_qty_divide" );
	g3-> add ( g2 );

	g2 = new QActionGroup ( g, "edit_price", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_price_set" );
	(void) new QAction ( g2, "edit_price_inc_dec" );
	(void) new QAction ( g2, "edit_price_to_priceguide" );
	g3-> add ( g2 );

	(void) new QAction ( g, "edit_bulk" );
	(void) new QAction ( g, "edit_sale" );
		
	g2 = new QActionGroup ( g, "edit_comment", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_comment_set" );
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_comment_add" );
	(void) new QAction ( g2, "edit_comment_rem" );
	
	g2 = new QActionGroup ( g, "edit_remark", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_remark_set" );
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_remark_add" );
	(void) new QAction ( g2, "edit_remark_rem" );
	g3-> add ( g2 );

	//tier

	g2 = new QActionGroup ( g, "edit_retain", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_retain_yes", true );
	(void) new QAction ( g2, "edit_retain_no", true );
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_retain_toggle" );

	g2 = new QActionGroup ( g, "edit_stockroom", false );
	g2-> setUsesDropDown ( true );
	(void) new QAction ( g2, "edit_stockroom_yes", true );
	(void) new QAction ( g2, "edit_stockroom_no", true );
	g2-> addSeparator ( );
	(void) new QAction ( g2, "edit_stockroom_toggle" );

	(void) new QAction ( g, "edit_reserved" );

	g = new QActionGroup ( this, "edit_bl_info_group", false );

	(void) new QAction ( g, "edit_bl_catalog" );
	(void) new QAction ( g, "edit_bl_priceguide" );
	(void) new QAction ( g, "edit_bl_lotsforsale" );
	(void) new QAction ( g, "edit_bl_myinventory" );

	a = new QAction ( this, "view_fullscreen", true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewFullScreen ( bool )));	

	a = new QAction ( this, "view_simple_mode", true );
	connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setSimpleMode ( bool )));

	a = new QAction ( this, "view_toolbar", true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewToolBar ( bool )));

	(void) m_taskpanes-> createItemVisibilityAction ( this, "view_infobar" );

	a = new QAction ( this, "view_statusbar", true );
	connect ( a, SIGNAL( toggled ( bool )), this, SLOT( viewStatusBar ( bool )));

	a = new QAction ( this, "view_show_input_errors", true );
	connect ( a, SIGNAL( toggled ( bool )), CConfig::inst ( ), SLOT( setShowInputErrors ( bool )));

	(void) new QAction ( this, "view_difference_mode", true );
	
	(void) new QAction ( this, "view_save_default_col" );

	a = new QAction ( this, "extras_update_database" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( updateDatabase ( )));

	a = new QAction ( this, "extras_configure" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( configure ( )));

	g = new QActionGroup ( this, "extras_net", true );
	connect ( g, SIGNAL( selected ( QAction * )), this, SLOT( setOnlineStatus ( QAction * )));

	(void) new QAction ( g, "extras_net_online", true );
	(void) new QAction ( g, "extras_net_offline", true );

	g = new QActionGroup ( this, "window_mode", true );
	connect ( g, SIGNAL( selected ( QAction * )), this, SLOT( setWindowMode ( QAction * )));

	(void) new QAction ( g, "window_mode_mdi", true );
	(void) new QAction ( g, "window_mode_tab_above", true );
	(void) new QAction ( g, "window_mode_tab_below", true );

	a = new QAction ( this, "window_cascade" );
	connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( cascade ( )));

	a = new QAction ( this, "window_tile" );
	connect ( a, SIGNAL( activated ( )), m_mdi, SLOT( tile ( )));

	l = new CListAction ( true, this, "window_list" );
	l-> setListProvider ( new WindowListProvider ( this ));
	connect ( l, SIGNAL( activated ( int )), this, SLOT( windowActivate ( int )));

	a = new QAction ( this, "help_whatsthis" );
	connect ( a, SIGNAL( activated ( )), this, SLOT( whatsThis ( )));

	a = new QAction ( this, "help_about" );
	connect ( a, SIGNAL( activated ( )), cApp, SLOT( about ( )));

	a = new QAction ( this, "help_updates" );
	connect ( a, SIGNAL( activated ( )), cApp, SLOT( checkForUpdates ( )));

	a = new QAction ( this, "help_registration" );
	a-> setEnabled ( CConfig::inst ( )-> registration ( ) != CConfig::OpenSource );
	connect ( a, SIGNAL( activated ( )), cApp, SLOT( registration ( )));

	// set all icons that have a pixmap corresponding to name()

	QObjectList *alist = queryList ( "QAction", 0, false, true );
	for ( QObjectListIt it ( *alist ); it. current ( ); ++it ) {
		const char *name = it. current ( )-> name ( );

		if ( name && name [0] ) {
			QIconSet set = CResource::inst ( )-> iconSet ( name );

			if ( !set. isNull ( ))
				static_cast <QAction *> ( it. current ( ))-> setIconSet ( set );
		}
	}
	delete alist;
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
	setWindowState ( b ? ws | Qt::WindowFullScreen : ws & ~Qt::WindowFullScreen ); 
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

	createWindow ( CDocument::fileImportBrickLinkOrder ( ));
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

bool CFrameWork::createWindow ( CDocument *doc )
{
	if ( !doc )
		return false;

	QPtrList <CWindow> list = allWindows ( );

	for ( QPtrListIterator <CWindow> it ( list ); it. current ( ); ++it ) {
		if ( it. current ( )-> document ( ) == doc ) {
			it. current ( )-> setFocus ( );
			return true;
		}
	}

	CWindow *w = new CWindow ( doc, m_mdi, "window" );

	QPixmap pix = CResource::inst ( )-> pixmap ( "icon" );
	// Qt/Win32 bug: MDI child window icon()s have to be 16x16 or smaller...
	pix. convertFromImage ( pix. convertToImage ( ). smoothScale ( pix. size ( ). boundedTo ( QSize ( 16, 16 ))));
	if ( !pix. isNull ( ))
		w-> setIcon ( pix );

	QWidget *act = m_mdi-> activeWindow ( );

	if ( !act || ( act == w ) || ( act-> isMaximized ( )))
		w-> showMaximized ( );
	else
		w-> show ( );

	return true;
}


bool CFrameWork::updateDatabase ( )
{
	if ( closeAllWindows ( )) {
		delete m_add_dialog;

		CProgressDialog d ( this );
		CUpdateDatabase update ( &d );

		return d. exec ( );
	}
	return false;
}

void CFrameWork::windowActivate ( int i )
{
	QWidgetList l = m_mdi-> windowList ( CWorkspace/*CWindowManager*/::CreationOrder );

	if (( i >= 0 ) && ( i < int( l. count ( )))) {
		QWidget *w = l. at ( i );

		w-> setFocus ( );
	}
}

void CFrameWork::connectAction ( bool do_connect, const char *name, CWindow *window, const char *windowslot, bool ( CWindow::* is_on_func ) ( ) const )
{
	QAction *a = findAction ( name );

	if ( a ) {
		QAction *a2 = a;

		while ( a2 && ::qt_cast<QActionGroup *> ( a2-> parent ( )))
			a2 = static_cast<QAction *> ( a2-> parent ( ));

		a2-> setEnabled ( do_connect );
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
		it. key ( )-> setOn (( window->* it. data ( )) ( ));
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
	connectAction ( do_connect, "file_close", window, SLOT( close ( )));

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

void CFrameWork::connectWindow ( QWidget *w )
{
	if ( w && ( w == m_current_window ))
		return;

	if ( m_current_window ) {
		CDocument *doc = m_current_window-> document ( );

		connectAllActions ( false, m_current_window );

		disconnect ( doc, SIGNAL( statisticsChanged ( )), this, SLOT( statisticsUpdate ( )));
		disconnect ( doc, SIGNAL( selectionChanged ( const CDocument::ItemList & )), this, SLOT( selectionUpdate ( const CDocument::ItemList & )));

		m_current_window = 0;
	}

	if ( w && ::qt_cast <CWindow *> ( w )) {
		CWindow *window = static_cast <CWindow *> ( w );
		CDocument *doc = window-> document ( );

		connectAllActions ( true, window );

		connect ( doc, SIGNAL( statisticsChanged ( )), this, SLOT( statisticsUpdate ( )));
		connect ( doc, SIGNAL( selectionChanged ( const CDocument::ItemList & )), this, SLOT( selectionUpdate ( const CDocument::ItemList & )));

		m_current_window = window;
	}

	if ( m_add_dialog ) {
		m_add_dialog-> attach ( m_current_window );
		if ( !m_current_window )
			m_add_dialog-> close ( );
	}
	findAction ( "edit_additems" )-> setEnabled (( m_current_window ));

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
		{ "edit_modify",              1, 0, 0 },
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
		QAction *a = findAction ( endisable_ptr-> m_name );

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
	findAction ( "edit_status_include" )-> setOn ( status == BrickLink::InvItem::Include );
	findAction ( "edit_status_exclude" )-> setOn ( status == BrickLink::InvItem::Exclude );
	findAction ( "edit_status_extra" )-> setOn ( status == BrickLink::InvItem::Extra );

	findAction ( "edit_cond_new" )-> setOn ( condition == BrickLink::New );
	findAction ( "edit_cond_used" )-> setOn ( condition == BrickLink::Used );

	findAction ( "edit_retain_yes" )-> setOn ( retain == 1 );
	findAction ( "edit_retain_no" )-> setOn ( retain == 0 );

	findAction ( "edit_stockroom_yes" )-> setOn ( stockroom == 1 );
	findAction ( "edit_stockroom_no" )-> setOn ( stockroom == 0 );
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
					arg( stat. value ( ). toLocalizedString ( )).
					arg( tr( "min." )).
					arg( stat. minValue ( ). toLocalizedString ( ));
		}
		else
			valstr = stat. value ( ). toLocalizedString ( );

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

	QAction *a = findAction ( "file_save" );
	if ( a )
		a-> setEnabled ( !b );

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
	bool tabbed = ( act != findAction ( "window_mode_mdi" ));
	bool execllike = ( act == findAction ( "window_mode_tab_below" ));

	m_mdi-> setShowTabs ( tabbed );
	m_mdi-> setSpreadSheetTabs ( execllike );
	
	findAction ( "window_cascade" )-> setEnabled ( !tabbed );
	findAction ( "window_tile" )-> setEnabled ( !tabbed );
}

void CFrameWork::setOnlineStatus ( QAction *act )
{
	bool b = ( act == findAction ( "extras_net_online" ));

	if ( !b && m_running )
		cancelAllTransfers ( );
		
	CConfig::inst ( )-> setOnlineStatus ( b );
}

void CFrameWork::registrationUpdate ( )
{
	bool personal = ( CConfig::inst ( )-> registration ( ) == CConfig::Personal );
	bool demo = ( CConfig::inst ( )-> registration ( ) == CConfig::Demo );

	QAction *a = findAction ( "view_simple_mode" );
	
	// personal -> always on
	// demo -> always off
	a-> setOn (( CConfig::inst ( )-> simpleMode ( ) || personal ) && !demo );
	a-> setEnabled (!( demo || personal ));

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
		QAction *a = findAction ( *action_ptr );

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

	QWidgetList wl = m_mdi-> windowList ( CWorkspace::CreationOrder );

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

		int wstate = CConfig::inst ( )-> readNumEntry ( "/MainWindow/AddItemDialog/State", WindowNoState );

		// make sure at least SOME part of the window ends up on the screen
		if ( r. isValid ( ) && !r. isEmpty ( ) && !( r & qApp-> desktop ( )-> rect ( )). isEmpty ( )) {
			m_add_dialog-> resize ( r. size ( ));  // X11 compat. mode -- KDE 3.x doesn't seem to like this though...
			m_add_dialog-> move ( r. topLeft ( ));
			//m_add_dialog-> setGeometry ( r );  // simple code (which shouldn't work on X11, but does on KDE 3.x...)
		}
		m_normal_geometry_adddlg = QRect ( m_add_dialog-> pos ( ), m_add_dialog-> size ( ));
		m_add_dialog-> setWindowState ( wstate & WindowMaximized );

		QAccel *acc = new QAccel ( m_add_dialog );

		QAction *action = findAction ( "edit_additems" );
		acc-> connectItem ( acc-> insertItem ( action-> accel ( )), action, SLOT( toggle ( )));
		
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
	findAction ( "edit_additems" )-> setOn ( m_add_dialog && m_add_dialog-> isVisible ( ));
}

