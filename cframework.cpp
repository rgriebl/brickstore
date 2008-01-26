/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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

#include <QUndoGroup>
#include <QUndoStack>
#include <QAction>
#include <QDesktopWidget>
#include <QCloseEvent>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qstatusbar.h>
//#include <qwidgetlist.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qbitmap.h>
#include <qdir.h>
#include <qfileinfo.h>
//#include <qobjectlist.h>
#include <qtooltip.h>
#include <qcursor.h>
//#include <qaccel.h>

#include "capplication.h"
#include "cmessagebox.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cconfig.h"
#include "cmoney.h"
#include "cmultiprogressbar.h"
//#include "ciconfactory.h"
//#include "cutility.h"
#include "cspinner.h"
//#include "cundo.h"
#include "cworkspace.h"
#include "ctaskpanemanager.h"
#include "ctaskwidgets.h"
#include "cprogressdialog.h"
#include "cupdatedatabase.h"
#include "csplash.h"
#include "clocalemeasurement.h"

#include "dadditem.h"
//#include "dlgsettingsimpl.h"

#include "cframework.h"


namespace {

enum ProgressItem {
    PGI_PriceGuide,
    PGI_Picture
};

} // namespace


CFrameWork *CFrameWork::s_inst = 0;

CFrameWork *CFrameWork::inst()
{
    if (!s_inst)
        (new CFrameWork())->setAttribute(Qt::WA_DeleteOnClose);

    return s_inst;
}


CFrameWork::CFrameWork(QWidget *parent, Qt::WindowFlags f)
        : QMainWindow(parent, f)
{
    s_inst = this;

    m_running = false;

// qApp->setMainWidget ( this );

#if defined( Q_WS_X11 )
    if (!icon() || icon()->isNull()) {
        QPixmap pix(":/icon");
        if (!pix.isNull())
            setIcon(pix);
    }
#endif

    setWindowTitle(cApp->appName());
    setUnifiedTitleAndToolBarOnMac(true);
// setUsesBigPixmaps ( true );

// (void) new CIconFactory ( );

    m_undogroup = new QUndoGroup(this);

    connect(cApp, SIGNAL(openDocument(const QString &)), this, SLOT(openDocument(const QString &)));

    m_recent_files = CConfig::inst()->value("/Files/Recent").toStringList();

    while (m_recent_files.count() > MaxRecentFiles)
        m_recent_files.pop_back();

    m_current_window = 0;

    m_mdi = new CWorkspace(this);
    connect(m_mdi, SIGNAL(currentChanged(QWidget *)), this, SLOT(connectWindow(QWidget *)));

    setCentralWidget(m_mdi);

    m_taskpanes = new CTaskPaneManager(this);
// m_taskpanes->setMode ( CConfig::inst ( )->value ( "/MainWindow/Infobar/Mode", CTaskPaneManager::Modern ).toInt ( ) != CTaskPaneManager::Classic ? CTaskPaneManager::Modern : CTaskPaneManager::Classic );
    m_taskpanes->setMode(CTaskPaneManager::Classic);

    m_task_info = new CTaskInfoWidget(0);
    m_taskpanes->addItem(m_task_info, QIcon(":/sidebar/info"));
    m_taskpanes->setItemVisible(m_task_info, CConfig::inst()->value("/MainWindow/Infobar/InfoVisible", true).toBool());

    m_task_priceguide = new CTaskPriceGuideWidget(0);
    m_taskpanes->addItem(m_task_priceguide, QIcon(":/sidebar/priceguide"));
    m_taskpanes->setItemVisible(m_task_priceguide, CConfig::inst()->value("/MainWindow/Infobar/PriceguideVisible", true).toBool());

    m_task_appears = new CTaskAppearsInWidget(0);
    m_taskpanes->addItem(m_task_appears,  QIcon(":/sidebar/appearsin"));
    m_taskpanes->setItemVisible(m_task_appears, CConfig::inst()->value("/MainWindow/Infobar/AppearsinVisible", true).toBool());

    m_task_links = new CTaskLinksWidget(0);
    m_taskpanes->addItem(m_task_links,  QIcon(":/sidebar/links"));
    m_taskpanes->setItemVisible(m_task_links, CConfig::inst()->value("/MainWindow/Infobar/LinksVisible", true).toBool());

    createActions();

    QString str;
    QStringList sl;

    sl = CConfig::inst()->value("/MainWindow/Menubar/File").toStringList();
    if (sl.isEmpty())
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

    menuBar()->addMenu(createMenu("file", sl));

    sl = CConfig::inst()->value("/MainWindow/Menubar/Edit").toStringList();
    if (sl.isEmpty())
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
        << "edit_copyremarks"
        << "-"
        << "edit_bl_info_group";

    menuBar()->addMenu(createMenu("edit", sl));

    sl = CConfig::inst()->value("/MainWindow/Menubar/View").toStringList();
    if (sl.isEmpty())
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

    menuBar()->addMenu(createMenu("view", sl));

    sl = CConfig::inst()->value("/MainWindow/Menubar/Extras").toStringList();
    if (sl.isEmpty())
        sl << "extras_net"
        << "-"
        << "extras_update_database"
        << "-"
        << "extras_configure";

    // Make sure there is a possibility to open the pref dialog!
    if (sl.indexOf("extras_configure") == -1)
        sl << "-" << "extras_configure";

    menuBar()->addMenu(createMenu("extras", sl));

    sl = CConfig::inst()->value("/MainWindow/Menubar/Window").toStringList();
    if (sl.isEmpty())
        sl << "window_mode"
        << "-"
        << "window_cascade"
        << "window_tile"
        << "-"
        << "window_list";

    menuBar()->addMenu(createMenu("window", sl));

    sl = CConfig::inst()->value("/MainWindow/Menubar/Help").toStringList();
    if (sl.isEmpty())
        sl << "help_updates"
        << "-"
        << "help_registration"
        << "-"
        << "help_about";

    menuBar()->addMenu(createMenu("help", sl));

    sl = CConfig::inst()->value("/MainWindow/ContextMenu/Item").toStringList();
    if (sl.isEmpty())
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

    m_contextmenu = createMenu("context", sl);

    sl = CConfig::inst()->value("/MainWindow/Toolbar/Buttons").toStringList();
    if (sl.isEmpty())
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

    m_toolbar = createToolBar("toolbar", sl);
    addToolBar(m_toolbar);
    connect(m_toolbar, SIGNAL(visibilityChanged(bool)), findAction("view_toolbar"), SLOT(setChecked(bool)));

    createStatusBar();
    findAction("view_statusbar")->setChecked(CConfig::inst()->value("/MainWindow/Statusbar/Visible", true).toBool());

    languageChange();

    QByteArray ba;

    ba = CConfig::inst()->value("/MainWindow/Layout/Geometry").toByteArray();
    if (ba.isEmpty() || !restoreGeometry(ba)) {
        float dw = qApp->desktop()->width() / 10.f;
        float dh = qApp->desktop()->height() / 10.f;

        setGeometry(int(dw), int(dh), int (8 * dw), int(8 * dh));
        setWindowState(Qt::WindowMaximized);
    }
    findAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

    ba = CConfig::inst()->value("/MainWindow/Layout/DockWindows").toByteArray();
    if (ba.isEmpty() || !restoreState(ba))
        findAction("view_toolbar")->setChecked(true);

    /* switch ( CConfig::inst ( )->value ( "/MainWindow/Layout/WindowMode", 1 ).toInt ( )) {
      default:
      case  1: findAction ( "window_mode_tab_above" )->setChecked ( true ); break;
      case  2: findAction ( "window_mode_tab_below" )->setChecked ( true ); break;
     }
    */
    BrickLink::Core *bl = BrickLink::inst();

    connect(CConfig::inst(), SIGNAL(onlineStatusChanged(bool)), bl, SLOT(setOnlineStatus(bool)));
    connect(CConfig::inst(), SIGNAL(blUpdateIntervalsChanged(int, int)), bl, SLOT(setUpdateIntervals(int, int)));
    connect(CConfig::inst(), SIGNAL(proxyChanged(bool, const QString &, int)), bl, SLOT(setHttpProxy(bool, const QString &, int)));
    connect(CMoney::inst(), SIGNAL(monetarySettingsChanged()), this, SLOT(statisticsUpdate()));
    connect(CConfig::inst(), SIGNAL(weightSystemChanged(CConfig::WeightSystem)), this, SLOT(statisticsUpdate()));
    connect(CConfig::inst(), SIGNAL(simpleModeChanged(bool)), this, SLOT(setSimpleMode(bool)));
    connect(CConfig::inst(), SIGNAL(registrationChanged(CConfig::Registration)), this, SLOT(registrationUpdate()));

    findAction("view_show_input_errors")->setChecked(CConfig::inst()->showInputErrors());

    registrationUpdate();

    findAction(CConfig::inst()->onlineStatus() ? "extras_net_online" : "extras_net_offline")->setChecked(true);

    bl->setOnlineStatus(CConfig::inst()->onlineStatus());
    bl->setHttpProxy(CConfig::inst()->proxy());
    {
        int pic, pg;
        CConfig::inst()->blUpdateIntervals(pic, pg);
        bl->setUpdateIntervals(pic, pg);
    }

    connect(bl, SIGNAL(priceGuideProgress(int, int)), this, SLOT(gotPriceGuideProgress(int, int)));
    connect(bl, SIGNAL(pictureProgress(int, int)),    this, SLOT(gotPictureProgress(int, int)));

// connect ( m_progress, SIGNAL( statusChange ( bool )), m_spinner, SLOT( setActive ( bool )));
    connect(m_undogroup, SIGNAL(cleanChanged(bool)), this, SLOT(modificationUpdate()));

    CSplash::inst()->message(qApp->translate("CSplash", "Loading Database..."));

    bool dbok = BrickLink::inst()->readDatabase();

    if (!dbok) {
        if (CMessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />Should these files be updated now?"), CMessageBox::Yes, CMessageBox::No) == CMessageBox::Yes)
            dbok = updateDatabase();
    }

    if (dbok)
        cApp->enableEmitOpenDocument();
    else
        CMessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />The program is not functional without these files."));

    setAcceptDrops(true);

    m_add_dialog = 0;
    m_running = true;

    connectAllActions(false, 0);    // init enabled/disabled status of document actions
    connectWindow(0);
}

void CFrameWork::languageChange()
{
    m_toolbar->setWindowTitle(tr("Toolbar"));
// m_progress->setItemLabel ( PGI_PriceGuide, tr( "Price Guide updates" ));
// m_progress->setItemLabel ( PGI_Picture,    tr( "Image updates" ));

    m_taskpanes->setItemText(m_task_info,       tr("Info"));
    m_taskpanes->setItemText(m_task_priceguide, tr("Price Guide"));
    m_taskpanes->setItemText(m_task_appears,    tr("Appears In Sets"));
    m_taskpanes->setItemText(m_task_links,      tr("Links"));

    translateActions();

    statisticsUpdate();
}


void CFrameWork::translateActions()
{
    struct {
        const char *m_name;
        QString     m_text;
        QString     m_shortcut;
    } *atptr, actiontable [] = {
        { "file",                           tr("&File"),                              0 },
        { "file_new",                       tr("New", "File|New"),                    tr("Ctrl+N", "File|New") },
        { "file_open",                      tr("Open..."),                            tr("Ctrl+O", "File|Open") },
        { "file_open_recent",               tr("Open Recent"),                        0 },
        { "file_save",                      tr("Save"),                               tr("Ctrl+S", "File|Save") },
        { "file_saveas",                    tr("Save As..."),                         0 },
        { "file_print",                     tr("Print..."),                           tr("Ctrl+P", "File|Print") },
        { "file_import",                    tr("Import"),                             0 },
        { "file_import_bl_inv",             tr("BrickLink Inventory..."),             tr("Ctrl+I", "File|Import BrickLink Inventory") },
        { "file_import_bl_xml",             tr("BrickLink XML..."),                   0 },
        { "file_import_bl_order",           tr("BrickLink Order..."),                 0 },
        { "file_import_bl_store_inv",       tr("BrickLink Store Inventory..."),       0 },
        { "file_import_bl_cart",            tr("BrickLink Shopping Cart..."),         0 },
        { "file_import_peeron_inv",         tr("Peeron Inventory..."),                0 },
        { "file_import_ldraw_model",        tr("LDraw Model..."),                     0 },
        { "file_import_briktrak",           tr("BrikTrak Inventory..."),              0 },
        { "file_export",                    tr("Export"),                             0 },
        { "file_export_bl_xml",             tr("BrickLink XML..."),                   0 },
        { "file_export_bl_xml_clip",        tr("BrickLink XML to Clipboard"),         0 },
        { "file_export_bl_update_clip",     tr("BrickLink Mass-Update XML to Clipboard"), 0 },
        { "file_export_bl_invreq_clip",     tr("BrickLink Inventory XML to Clipboard"),   0 },
        { "file_export_bl_wantedlist_clip", tr("BrickLink Wanted List XML to Clipboard"), 0 },
        { "file_export_briktrak",           tr("BrikTrak Inventory..."),              0 },
        { "file_close",                     tr("Close"),                              tr("Ctrl+W", "File|Close") },
        { "file_exit",                      tr("Exit"),                               tr("Ctrl+Q", "File|Quit") },
        { "edit",                           tr("&Edit"),                              0 },
        { "edit_undo",                      0,                                          tr("Ctrl+Z", "Edit|Undo") },
        { "edit_redo",                      0,                                          tr("Ctrl+Y", "Edit|Redo") },
        { "edit_cut",                       tr("Cut"),                                tr("Ctrl+X", "Edit|Cut") },
        { "edit_copy",                      tr("Copy"),                               tr("Ctrl+C", "Edit|Copy") },
        { "edit_paste",                     tr("Paste"),                              tr("Ctrl+V", "Edit|Paste") },
        { "edit_delete",                    tr("Delete"),                             tr("Delete", "Edit|Delete") },
        { "edit_additems",                  tr("Add Items..."),                       tr("Insert", "Edit|AddItems") },
        { "edit_subtractitems",             tr("Subtract Items..."),                  0 },
        { "edit_mergeitems",                tr("Consolidate Items..."),               0 },
        { "edit_partoutitems",              tr("Part out Item..."),                   0 },
        { "edit_reset_diffs",               tr("Reset Differences"),                  0 },
        { "edit_copyremarks",               tr("Copy Remarks from Document..."),      0 },
        { "edit_select_all",                tr("Select All"),                         tr("Ctrl+A", "Edit|SelectAll") },
        { "edit_select_none",               tr("Select None"),                        tr("Ctrl+Shift+A", "Edit|SelectNone") },
        { "view",                           tr("&View"),                              0 },
        { "view_simple_mode",               tr("Buyer/Collector Mode"),               0 },
        { "view_toolbar",                   tr("View Toolbar"),                       0 },
        { "view_infobar",                   tr("View Infobars"),                      0 },
        { "view_statusbar",                 tr("View Statusbar"),                     0 },
        { "view_fullscreen",                tr("Full Screen"),                        tr("F11") },
        { "view_show_input_errors",         tr("Show Input Errors"),                  0 },
        { "view_difference_mode",           tr("Difference Mode"),                    0 },
        { "view_save_default_col",          tr("Save Column Layout as Default"),      0 },
        { "extras",                         tr("E&xtras"),                            0 },
        { "extras_update_database",         tr("Update Database"),                    0 },
        { "extras_configure",               tr("Configure..."),                       0 },
        { "extras_net_online",              tr("Online Mode"),                        0 },
        { "extras_net_offline",             tr("Offline Mode"),                       0 },
        { "window",                         tr("&Windows"),                           0 },
        { "window_mode_mdi",                tr("Standard MDI Mode"),                  0 },
        { "window_mode_tab_above",          tr("Show Tabs at Top"),                   0 },
        { "window_mode_tab_below",          tr("Show Tabs at Bottom"),                0 },
        { "window_cascade",                 tr("Cascade"),                            0 },
        { "window_tile",                    tr("Tile"),                               0 },
        { "help",                           tr("&Help"),                              0 },
        { "help_whatsthis",                 tr("What's this?"),                       tr("Shift+F1", "Help|WhatsThis") },
        { "help_about",                     tr("About..."),                           0 },
        { "help_updates",                   tr("Check for Program Updates..."),       0 },
        { "help_registration",              tr("Registration..."),                    0 },
        { "edit_status",                    tr("Status"),                             0 },
        { "edit_status_include",            tr("Include"),                            0 },
        { "edit_status_exclude",            tr("Exclude"),                            0 },
        { "edit_status_extra",              tr("Extra"),                              0 },
        { "edit_status_toggle",             tr("Toggle Include/Exclude"),             0 },
        { "edit_cond",                      tr("Condition"),                          0 },
        { "edit_cond_new",                  tr("New", "Cond|New"),                    0 },
        { "edit_cond_used",                 tr("Used"),                               0 },
        { "edit_cond_toggle",               tr("Toggle New/Used"),                    0 },
        { "edit_color",                     tr("Color..."),                           0 },
        { "edit_qty",                       tr("Quantity"),                           0 },
        { "edit_qty_multiply",              tr("Multiply..."),                        tr("Ctrl+*", "Edit|Quantity|Multiply") },
        { "edit_qty_divide",                tr("Divide..."),                          tr("Ctrl+/", "Edit|Quantity|Divide") },
        { "edit_price",                     tr("Price"),                              0 },
        { "edit_price_round",               tr("Round to 2 Decimal Places"),          0 },
        { "edit_price_set",                 tr("Set..."),                             0 },
        { "edit_price_to_priceguide",       tr("Set to Price Guide..."),              tr("Ctrl+G", "Edit|Price|Set to PriceGuide") },
        { "edit_price_inc_dec",             tr("Inc- or Decrease..."),                tr("Ctrl++", "Edit|Price|Inc/Dec") },
        { "edit_bulk",                      tr("Bulk Quantity..."),                   0 },
        { "edit_sale",                      tr("Sale..."),                            tr("Ctrl+%", "Edit|Sale") },
        { "edit_comment",                   tr("Comment"),                            0 },
        { "edit_comment_set",               tr("Set..."),                             0 },
        { "edit_comment_add",               tr("Add to..."),                          0 },
        { "edit_comment_rem",               tr("Remove from..."),                     0 },
        { "edit_remark",                    tr("Remark"),                             0 },
        { "edit_remark_set",                tr("Set..."),                             0 },
        { "edit_remark_add",                tr("Add to..."),                          0 },
        { "edit_remark_rem",                tr("Remove from..."),                     0 },
        { "edit_retain",                    tr("Retain in Inventory"),                0 },
        { "edit_retain_yes",                tr("Yes"),                                0 },
        { "edit_retain_no",                 tr("No"),                                 0 },
        { "edit_retain_toggle",             tr("Toggle Yes/No"),                      0 },
        { "edit_stockroom",                 tr("Stockroom Item"),                     0 },
        { "edit_stockroom_yes",             tr("Yes"),                                0 },
        { "edit_stockroom_no",              tr("No"),                                 0 },
        { "edit_stockroom_toggle",          tr("Toggle Yes/No"),                      0 },
        { "edit_reserved",                  tr("Reserved for..."),                    0 },
        { "edit_bl_catalog",                tr("Show BrickLink Catalog Info..."),     0 },
        { "edit_bl_priceguide",             tr("Show BrickLink Price Guide Info..."), 0 },
        { "edit_bl_lotsforsale",            tr("Show Lots for Sale on BrickLink..."), 0 },
        { "edit_bl_myinventory",            tr("Show in my Store on BrickLink..."),   0 },

        { 0, 0, 0 }
    };

    for (atptr = actiontable; atptr->m_name; atptr++) {
        QAction *a = findAction(atptr->m_name);

        if (a) {
            if (!atptr->m_text.isNull())
                a->setText(atptr->m_text);
            if (!atptr->m_shortcut.isNull())
                a->setShortcut(QKeySequence(atptr->m_shortcut));
        }
    }
}

CFrameWork::~CFrameWork()
{
    CConfig::inst()->setValue("/Files/Recent", m_recent_files);

    CConfig::inst()->setValue("/MainWindow/Statusbar/Visible",         statusBar()->isVisibleTo(this));
    CConfig::inst()->setValue("/MainWindow/Infobar/Mode",              m_taskpanes->mode());
    CConfig::inst()->setValue("/MainWindow/Infobar/InfoVisible",       m_taskpanes->isItemVisible(m_task_info));
    CConfig::inst()->setValue("/MainWindow/Infobar/PriceguideVisible", m_taskpanes->isItemVisible(m_task_priceguide));
    CConfig::inst()->setValue("/MainWindow/Infobar/AppearsinVisible",  m_taskpanes->isItemVisible(m_task_appears));
    CConfig::inst()->setValue("/MainWindow/Infobar/LinksVisible",      m_taskpanes->isItemVisible(m_task_links));

    CConfig::inst()->setValue("/MainWindow/Layout/DockWindows", saveState());
    CConfig::inst()->setValue("/MainWindow/Layout/Geometry", saveGeometry());
    CConfig::inst()->setValue("/MainWindow/Layout/WindowMode", m_mdi->tabMode());

    if (m_add_dialog)
        CConfig::inst()->setValue("/MainWindow/AddItemDialog/Geometry", m_add_dialog->saveGeometry());

    s_inst = 0;
}

#if 0
void CFrameWork::dragMoveEvent(QDragMoveEvent *e)
{
    e->acceptAction(e->action() == QDropEvent::Copy);
}

void CFrameWork::dragEnterEvent(QDragEnterEvent *e)
{
    if (QUriDrag::canDecode(e))
        e->accept();
}

void CFrameWork::dropEvent(QDropEvent *e)
{
    QStringList sl;

    if (QUriDrag::decodeLocalFiles(e, sl)) {
        for (QStringList::Iterator it = sl.begin(); it != sl.end(); ++it) {
            openDocument(*it);
        }
    }
}
#endif

QAction *CFrameWork::findAction(const QString &name)
{
    return name.isEmpty() ? 0 : static_cast <QAction *>(findChild<QAction *>(name));
}

void CFrameWork::createStatusBar()
{
    m_errors = new QLabel(statusBar());
    statusBar()->addPermanentWidget(m_errors, 0);

    m_statistics = new QLabel(statusBar());
    statusBar()->addPermanentWidget(m_statistics, 0);

    m_modified = new QLabel(statusBar());
    statusBar()->addPermanentWidget(m_modified, 0);

// m_progress = new CMultiProgressBar ( statusBar ( ));
// m_progress->setFixedWidth ( fontMetrics ( ).height ( ) * 10 );
// m_progress->setFixedHeight ( fontMetrics ( ).height ( ));

// QPixmap p ( ":/status/stop" );
// if ( !p.isNull ( ))
//  m_progress->setStopPixmap ( p );

// m_progress->addItem ( QString ( ), PGI_PriceGuide );
// m_progress->addItem ( QString ( ), PGI_Picture    );

    //m_progress->setProgress ( -1, 100 );

// statusBar ( )->addPermanentWidget ( m_progress, 0 );

// connect ( m_progress, SIGNAL( stop ( )), this, SLOT( cancelAllTransfers ( )));

    statusBar()->hide();
}

QMenu *CFrameWork::createMenu(const QString &name, const QStringList &a_names)
{
    if (a_names.isEmpty())
        return 0;

    QMenu *m = new QMenu(this);
    m->menuAction()->setObjectName(name);

    foreach(const QString &an, a_names) {
        if (an == "-") {
            m->addSeparator();
        }
        else {
            QAction *a = findAction(an);

            if (a)
                m->addAction(a);
            else
                qWarning("Couldn't find action '%s'", qPrintable(an));
        }
    }
    return m;
}


QToolBar *CFrameWork::createToolBar(const QString &name, const QStringList &a_names)
{
    if (a_names.isEmpty())
        return 0;

    QToolBar *t = new QToolBar(this);
    t->setObjectName(name);

    foreach(const QString &an, a_names) {
        if (an == "-") {
            t->addSeparator();
        }
        else {
            QAction *a = findAction(an);

            if (a)
                t->addAction(a);
            else
                qWarning("Couldn't find action '%s'", qPrintable(an));
        }
    }

    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    spacer->setMinimumSize(4, 4);
    t->addWidget(spacer);

    m_spinner = new CSpinner();
    m_spinner->setToolTip(tr("Download activity indicator"));
    m_spinner->setPixmap(QPixmap(":/images/spinner"));
    t->addWidget(m_spinner);

    spacer = new QWidget();
    spacer->setFixedSize(4, 4);
    t->addWidget(spacer);

    t->hide();
    return t;
}

inline QAction *newQAction(QObject *parent, const char *name, bool toggle = false, QObject *receiver = 0, const char *slot = 0)
{
    QAction *a = new QAction(parent);
    a->setObjectName(name);
    if (toggle)
        a->setCheckable(true);
    if (receiver && slot)
        QObject::connect(a, toggle ? SIGNAL(toggled(bool)) : SIGNAL(triggered()), receiver, slot);
    return a;
}

inline QAction *newQActionSeparator(QObject *parent)
{
    QAction *a = new QAction(parent);
    a->setSeparator(true);
    return a;
}

inline QActionGroup *newQActionGroup(QObject *parent, const char *name, bool exclusive = false)
{
    QActionGroup *g = new QActionGroup(parent);
    g->setObjectName(name);
    g->setExclusive(exclusive);
    return g;
}

void CFrameWork::createActions()
{
    QAction *a;
    QActionGroup *g, *g2, *g3;


    a = newQAction(this, "file_new", false, this, SLOT(fileNew()));

    a = newQAction(this, "file_open", false, this, SLOT(fileOpen()));

    g = newQActionGroup(this, "file_open_recent");
    for (int i = 0; i < MaxRecentFiles; i++) {
        a = newQAction(this, "file_open_recent_%1", false, this, SLOT(fileOpenRecent()));
        a->setActionGroup(g);
    }

    (void) newQAction(this, "file_save");
    (void) newQAction(this, "file_saveas");
    (void) newQAction(this, "file_print");

    g = newQActionGroup(this, "file_import", false);
    (void) newQAction(g, "file_import_bl_inv", false, this, SLOT(fileImportBrickLinkInventory()));
    (void) newQAction(g, "file_import_bl_xml", false, this, SLOT(fileImportBrickLinkXML()));
    (void) newQAction(g, "file_import_bl_order", false, this, SLOT(fileImportBrickLinkOrder()));
    (void) newQAction(g, "file_import_bl_store_inv", false, this, SLOT(fileImportBrickLinkStore()));
    (void) newQAction(g, "file_import_bl_cart", false, this, SLOT(fileImportBrickLinkCart()));
    (void) newQAction(g, "file_import_peeron_inv", false, this, SLOT(fileImportPeeronInventory()));
    (void) newQAction(g, "file_import_ldraw_model", false, this, SLOT(fileImportLDrawModel()));
    (void) newQAction(g, "file_import_briktrak", false, this, SLOT(fileImportBrikTrakInventory()));

    g = newQActionGroup(this, "file_export", false);

    (void) newQAction(g, "file_export_bl_xml");
    (void) newQAction(g, "file_export_bl_xml_clip");
    (void) newQAction(g, "file_export_bl_update_clip");
    (void) newQAction(g, "file_export_bl_invreq_clip");
    (void) newQAction(g, "file_export_bl_wantedlist_clip");
    (void) newQAction(g, "file_export_briktrak");

    (void) newQAction(this, "file_close");

    a = newQAction(this, "file_exit", false, this, SLOT(close()));
    a->setMenuRole(QAction::QuitRole);

    m_undogroup->createUndoAction(this)->setObjectName("edit_undo");
    m_undogroup->createRedoAction(this)->setObjectName("edit_redo");

    (void) newQAction(this, "edit_cut");
    (void) newQAction(this, "edit_copy");
    (void) newQAction(this, "edit_paste");
    (void) newQAction(this, "edit_delete");
    a = newQAction(this, "edit_additems", true, this, SLOT(toggleAddItemDialog(bool)));
    a->setShortcutContext(Qt::ApplicationShortcut);

    (void) newQAction(this, "edit_subtractitems");
    (void) newQAction(this, "edit_mergeitems");
    (void) newQAction(this, "edit_partoutitems");
    (void) newQAction(this, "edit_reset_diffs");
    (void) newQAction(this, "edit_select_all");
    (void) newQAction(this, "edit_select_none");

    g2 = newQActionGroup(this, "edit_modify", false);
    g3 = newQActionGroup(this, "edit_modify_context", false);

    g = newQActionGroup(this, "edit_status", false);
    (newQAction(g, "edit_status_include", true))->setIcon(QIcon(":images/status_include"));
    (newQAction(g, "edit_status_exclude", true))->setIcon(QIcon(":images/status_exclude"));
    (newQAction(g, "edit_status_extra",   true))->setIcon(QIcon(":images/status_extra"));
    (void) newQActionSeparator(g);
    (void) newQAction(g, "edit_status_toggle");

    g = newQActionGroup(this, "edit_cond", false);
    (void) newQAction(g, "edit_cond_new", true);
    (void) newQAction(g, "edit_cond_used", true);
    (void) newQActionSeparator(g);
    (void) newQAction(g, "edit_cond_toggle");

    (void) newQAction(this, "edit_color");

    g = newQActionGroup(this, "edit_qty", false);
    (void) newQAction(g, "edit_qty_multiply");
    (void) newQAction(g, "edit_qty_divide");

    g = newQActionGroup(this, "edit_price", false);
    (void) newQAction(g, "edit_price_set");
    (void) newQAction(g, "edit_price_inc_dec");
    (void) newQAction(g, "edit_price_to_priceguide");
    (void) newQAction(g, "edit_price_round");

    (void) newQAction(this, "edit_bulk");
    (void) newQAction(this, "edit_sale");

    g = newQActionGroup(this, "edit_comment", false);
    (void) newQAction(g, "edit_comment_set");
    (void) newQActionSeparator(g);
    (void) newQAction(g, "edit_comment_add");
    (void) newQAction(g, "edit_comment_rem");

    g = newQActionGroup(this, "edit_remark", false);
    (void) newQAction(g, "edit_remark_set");
    (void) newQActionSeparator(g);
    (void) newQAction(g, "edit_remark_add");
    (void) newQAction(g, "edit_remark_rem");

    //tier

    g = newQActionGroup(this, "edit_retain", false);
    (void) newQAction(g, "edit_retain_yes", true);
    (void) newQAction(g, "edit_retain_no", true);
    (void) newQActionSeparator(g);
    (void) newQAction(g, "edit_retain_toggle");

    g = newQActionGroup(this, "edit_stockroom", false);
    (void) newQAction(g, "edit_stockroom_yes", true);
    (void) newQAction(g, "edit_stockroom_no", true);
    (void) newQActionSeparator(g);
    (void) newQAction(g2, "edit_stockroom_toggle");

    (void) newQAction(g, "edit_reserved");

    g = newQActionGroup(this, "edit_bl_info_group", false);
    (void) newQAction(g, "edit_bl_catalog");
    (void) newQAction(g, "edit_bl_priceguide");
    (void) newQAction(g, "edit_bl_lotsforsale");
    (void) newQAction(g, "edit_bl_myinventory");

    (void) newQAction(this, "view_fullscreen", true, this, SLOT(viewFullScreen(bool)));

    (void) newQAction(this, "view_simple_mode", true, CConfig::inst(), SLOT(setSimpleMode(bool)));

    (void) newQAction(this, "view_toolbar", true, this, SLOT(viewToolBar(bool)));

    (void) m_taskpanes->createItemVisibilityAction(this, "view_infobar");

    (void) newQAction(this, "view_statusbar", true, this, SLOT(viewStatusBar(bool)));

    (void) newQAction(this, "view_show_input_errors", true, CConfig::inst(), SLOT(setShowInputErrors(bool)));

    (void) newQAction(this, "view_difference_mode", true);

    (void) newQAction(this, "view_save_default_col");

    (void) newQAction(this, "extras_update_database", false, this, SLOT(updateDatabase()));

    a = newQAction(this, "extras_configure", false, this, SLOT(configure()));
    a->setMenuRole(QAction::PreferencesRole);

    g = newQActionGroup(this, "extras_net", true);
    connect(g, SIGNAL(triggered(QAction *)), this, SLOT(setOnlineStatus(QAction *)));
    (void) newQAction(g, "extras_net_online", true);
    (void) newQAction(g, "extras_net_offline", true);

    /*
     l = new CListAction ( true, this, "window_list" );
     l->setListProvider ( new WindowListProvider ( this ));
     connect ( l, SIGNAL( activated ( int )), this, SLOT( windowActivate ( int )));
    */

    (void) newQAction(this, "help_whatsthis", false, this, SLOT(whatsThis()));

    a = newQAction(this, "help_about", false, cApp, SLOT(about()));
    a->setMenuRole(QAction::AboutRole);

    a = newQAction(this, "help_updates", false, cApp, SLOT(checkForUpdates()));
    a->setMenuRole(QAction::ApplicationSpecificRole);

    a = newQAction(this, "help_registration", false, cApp, SLOT(registration()));
    a->setMenuRole(QAction::ApplicationSpecificRole);
    a->setVisible(CConfig::inst()->registration() != CConfig::OpenSource);

    // set all icons that have a pixmap corresponding to name()

    QList<QAction *> alist = findChildren<QAction *> ();
    foreach(QAction *a, alist) {
        if (!a->objectName().isEmpty()) {
            QString path = QLatin1String(":/images/22x22/") + a->objectName();

            // QIcon::isNull is useless in Qt4
            if (QFile::exists(path + ".png") || QFile::exists(path + ".svg"))
                a->setIcon(QIcon(path));
        }
    }
}

void CFrameWork::viewStatusBar(bool b)
{
    statusBar()->setShown(b);
}

void CFrameWork::viewToolBar(bool b)
{
    m_toolbar->setShown(b);
}

void CFrameWork::viewFullScreen(bool b)
{
    QFlags<Qt::WindowState> ws = windowState();
    setWindowState(b ? ws | Qt::WindowFullScreen : ws & ~Qt::WindowFullScreen);
}

void CFrameWork::openDocument(const QString &file)
{
    createWindow(CDocument::fileOpen(file));
}

void CFrameWork::fileNew()
{
    createWindow(CDocument::fileNew());
}

void CFrameWork::fileOpen()
{
    createWindow(CDocument::fileOpen());
}

void CFrameWork::fileOpenRecent(int i)
{
    if (i < int(m_recent_files.count())) {
        // we need to copy the string here, because we delete it later
        // (this seems to work on Linux, but XP crashes in m_recent_files.remove ( ...)
        QString tmp = m_recent_files [i];

        openDocument(tmp);
    }
}

void CFrameWork::fileImportPeeronInventory()
{
    createWindow(CDocument::fileImportPeeronInventory());
}

void CFrameWork::fileImportBrikTrakInventory()
{
    createWindow(CDocument::fileImportBrikTrakInventory());
}

void CFrameWork::fileImportBrickLinkInventory()
{
    fileImportBrickLinkInventory(0);
}


void CFrameWork::fileImportBrickLinkInventory(const BrickLink::Item *item)
{
    createWindow(CDocument::fileImportBrickLinkInventory(item));
}

bool CFrameWork::checkBrickLinkLogin()
{
    while (CConfig::inst()->blLoginUsername().isEmpty() ||
           CConfig::inst()->blLoginPassword().isEmpty()) {
        if (CMessageBox::question(this, tr("No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?"), CMessageBox::Yes, CMessageBox::No) == CMessageBox::Yes)
            configure("network");
        else
            return false;
    }
    return true;
}

void CFrameWork::fileImportBrickLinkOrder()
{
    if (!checkBrickLinkLogin())
        return;

    createWindow(CDocument::fileImportBrickLinkOrder());
}

void CFrameWork::fileImportBrickLinkStore()
{
    if (!checkBrickLinkLogin())
        return;

    createWindow(CDocument::fileImportBrickLinkStore());
}

void CFrameWork::fileImportBrickLinkXML()
{
    createWindow(CDocument::fileImportBrickLinkXML());
}

void CFrameWork::fileImportBrickLinkCart()
{
    createWindow(CDocument::fileImportBrickLinkCart());
}

void CFrameWork::fileImportLDrawModel()
{
    createWindow(CDocument::fileImportLDrawModel());
}

bool CFrameWork::createWindow(CDocument *doc)
{
    if (!doc)
        return false;

    QList <CWindow *> l = allWindows();

    foreach(CWindow *w, l) {
        if (w->document() == doc) {
            m_mdi->setCurrentWidget(w);
            w->setFocus();
            return true;
        }
    }

    m_mdi->addWindow(new CWindow(doc, m_mdi));
    return true;
}


bool CFrameWork::updateDatabase()
{
    if (closeAllWindows()) {
        delete m_add_dialog;

        CProgressDialog d(this);
        CUpdateDatabase update(&d);

        return d.exec();
    }
    return false;
}

void CFrameWork::windowActivate(int i)
{
    QList <CWindow *> l = allWindows();

    if ((i >= 0) && (i < l.count())) {
        QWidget *w = l.at(i);
        m_mdi->setCurrentWidget(w);
        w->setFocus();
    }
}

void CFrameWork::connectAction(bool do_connect, const char *name, CWindow *window, const char *windowslot, bool (CWindow::* is_on_func)() const)
{
    QAction *a = findAction(name);

    if (a) {
        a->setEnabled(do_connect);
        if (a->actionGroup())
            a->actionGroup()->setEnabled(do_connect);
    }

    if (a && window) {
        bool is_toggle = (::strstr(windowslot, "bool"));

        const char *actionsignal = is_toggle ? SIGNAL(toggled(bool)) : SIGNAL(triggered());

        if (do_connect)
            connect(a, actionsignal, window, windowslot);
        else
            disconnect(a, actionsignal, window, windowslot);

        if (is_toggle && do_connect && is_on_func)
            m_toggle_updates.insert(a, is_on_func);
    }
}

void CFrameWork::updateAllToggleActions(CWindow *window)
{
    for (QMap <QAction *, bool (CWindow::*)() const>::iterator it = m_toggle_updates.begin(); it != m_toggle_updates.end(); ++it) {
        it.key()->setChecked((window->* it.value())());
    }
}

void CFrameWork::connectAllActions(bool do_connect, CWindow *window)
{
    m_toggle_updates.clear();

    connectAction(do_connect, "file_save", window, SLOT(fileSave()));
    connectAction(do_connect, "file_saveas", window, SLOT(fileSaveAs()));
    connectAction(do_connect, "file_print", window, SLOT(filePrint()));
    connectAction(do_connect, "file_export", 0, 0);
    connectAction(do_connect, "file_export_briktrak", window, SLOT(fileExportBrikTrakInventory()));
    connectAction(do_connect, "file_export_bl_xml", window, SLOT(fileExportBrickLinkXML()));
    connectAction(do_connect, "file_export_bl_xml_clip", window, SLOT(fileExportBrickLinkXMLClipboard()));
    connectAction(do_connect, "file_export_bl_update_clip", window, SLOT(fileExportBrickLinkUpdateClipboard()));
    connectAction(do_connect, "file_export_bl_invreq_clip", window, SLOT(fileExportBrickLinkInvReqClipboard()));
    connectAction(do_connect, "file_export_bl_wantedlist_clip", window, SLOT(fileExportBrickLinkWantedListClipboard()));
    connectAction(do_connect, "file_close", window, SLOT(close()));

    connectAction(do_connect, "edit_cut", window, SLOT(editCut()));
    connectAction(do_connect, "edit_copy", window, SLOT(editCopy()));
    connectAction(do_connect, "edit_paste", window, SLOT(editPaste()));
    connectAction(do_connect, "edit_delete", window, SLOT(editDelete()));

    connectAction(do_connect, "edit_select_all", window, SLOT(selectAll()));
    connectAction(do_connect, "edit_select_none", window, SLOT(selectNone()));

    connectAction(do_connect, "edit_subtractitems", window, SLOT(editSubtractItems()));
    connectAction(do_connect, "edit_mergeitems", window, SLOT(editMergeItems()));
    connectAction(do_connect, "edit_partoutitems", window, SLOT(editPartOutItems()));
    connectAction(do_connect, "edit_reset_diffs", window, SLOT(editResetDifferences()));
    connectAction(do_connect, "edit_copyremarks", window, SLOT(editCopyRemarks()));

    connectAction(do_connect, "edit_status_include", window, SLOT(editStatusInclude()));
    connectAction(do_connect, "edit_status_exclude", window, SLOT(editStatusExclude()));
    connectAction(do_connect, "edit_status_extra", window, SLOT(editStatusExtra()));
    connectAction(do_connect, "edit_status_toggle", window, SLOT(editStatusToggle()));

    connectAction(do_connect, "edit_cond_new", window, SLOT(editConditionNew()));
    connectAction(do_connect, "edit_cond_used", window, SLOT(editConditionUsed()));
    connectAction(do_connect, "edit_cond_toggle", window, SLOT(editConditionToggle()));

    connectAction(do_connect, "edit_color", window, SLOT(editColor()));

    connectAction(do_connect, "edit_qty_multiply", window, SLOT(editQtyMultiply()));
    connectAction(do_connect, "edit_qty_divide", window, SLOT(editQtyDivide()));

    connectAction(do_connect, "edit_price_set", window, SLOT(editPrice()));
    connectAction(do_connect, "edit_price_round", window, SLOT(editPriceRound()));
    connectAction(do_connect, "edit_price_to_priceguide", window, SLOT(editPriceToPG()));
    connectAction(do_connect, "edit_price_inc_dec", window, SLOT(editPriceIncDec()));

    connectAction(do_connect, "edit_bulk", window, SLOT(editBulk()));
    connectAction(do_connect, "edit_sale", window, SLOT(editSale()));
    connectAction(do_connect, "edit_comment_set", window, SLOT(editComment()));
    connectAction(do_connect, "edit_comment_add", window, SLOT(addComment()));
    connectAction(do_connect, "edit_comment_rem", window, SLOT(removeComment()));
    connectAction(do_connect, "edit_remark_set", window, SLOT(editRemark()));
    connectAction(do_connect, "edit_remark_add", window, SLOT(addRemark()));
    connectAction(do_connect, "edit_remark_rem", window, SLOT(removeRemark()));

    connectAction(do_connect, "edit_retain_yes", window, SLOT(editRetainYes()));
    connectAction(do_connect, "edit_retain_no", window, SLOT(editRetainNo()));
    connectAction(do_connect, "edit_retain_toggle", window, SLOT(editRetainToggle()));

    connectAction(do_connect, "edit_stockroom_yes", window, SLOT(editStockroomYes()));
    connectAction(do_connect, "edit_stockroom_no", window, SLOT(editStockroomNo()));
    connectAction(do_connect, "edit_stockroom_toggle", window, SLOT(editStockroomToggle()));

    connectAction(do_connect, "edit_reserved", window, SLOT(editReserved()));

    connectAction(do_connect, "edit_bl_catalog", window, SLOT(showBLCatalog()));
    connectAction(do_connect, "edit_bl_priceguide", window, SLOT(showBLPriceGuide()));
    connectAction(do_connect, "edit_bl_lotsforsale", window, SLOT(showBLLotsForSale()));
    connectAction(do_connect, "edit_bl_myinventory", window, SLOT(showBLMyInventory()));

    connectAction(do_connect, "view_difference_mode", window, SLOT(setDifferenceMode(bool)), &CWindow::isDifferenceMode);
    connectAction(do_connect, "view_save_default_col", window, SLOT(saveDefaultColumnLayout()));

    updateAllToggleActions(window);
}

void CFrameWork::connectWindow(QWidget *w)
{
    if (w && (w == m_current_window))
        return;

    if (m_current_window) {
        CDocument *doc = m_current_window->document();

        connectAllActions(false, m_current_window);

        disconnect(doc, SIGNAL(statisticsChanged()), this, SLOT(statisticsUpdate()));
        disconnect(doc, SIGNAL(selectionChanged(const CDocument::ItemList &)), this, SLOT(selectionUpdate(const CDocument::ItemList &)));

        m_current_window = 0;
    }

    if (w && qobject_cast <CWindow *> (w)) {
        CWindow *window = static_cast <CWindow *>(w);
        CDocument *doc = window->document();

        connectAllActions(true, window);

        connect(doc, SIGNAL(statisticsChanged()), this, SLOT(statisticsUpdate()));
        connect(doc, SIGNAL(selectionChanged(const CDocument::ItemList &)), this, SLOT(selectionUpdate(const CDocument::ItemList &)));

        m_current_window = window;
    }

    if (m_add_dialog) {
        m_add_dialog->attach(m_current_window);
        if (!m_current_window)
            m_add_dialog->close();
    }
    findAction("edit_additems")->setEnabled((m_current_window));

    selectionUpdate(m_current_window ? m_current_window->document()->selection() : CDocument::ItemList());
    statisticsUpdate();
    modificationUpdate();

    emit windowActivated(m_current_window);
    emit documentActivated(m_current_window ? m_current_window->document() : 0);
}

void CFrameWork::selectionUpdate(const CDocument::ItemList &selection)
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

    uint cnt = selection.count();

    for (endisable_ptr = endisable_actions; endisable_ptr->m_name; endisable_ptr++) {
        QAction *a = findAction(endisable_ptr->m_name);

        if (a) {
            uint &mins = endisable_ptr->m_minsel;
            uint &maxs = endisable_ptr->m_maxsel;

            bool b = true;

            if (endisable_ptr->m_flags)
            {
                int f = endisable_ptr->m_flags;

                foreach(CDocument::Item *item, selection) {
                    if (f & NeedLotId)
                        b &= (item->lotId() != 0);
                    if (f & NeedInventory)
                        b &= (item->item() && item->item()->hasInventory());
                }
            }

            a->setEnabled(b && (mins ? (cnt >= mins) : true) && (maxs ? (cnt <= maxs) : true));
        }
    }

    int status    = -1;
    int condition = -1;
    int retain    = -1;
    int stockroom = -1;

    if (cnt) {
        status    = selection.front()->status();
        condition = selection.front()->condition();
        retain    = selection.front()->retain()    ? 1 : 0;
        stockroom = selection.front()->stockroom() ? 1 : 0;

        foreach(CDocument::Item *item, selection) {
            if ((status >= 0) && (status != item->status()))
                status = -1;
            if ((condition >= 0) && (condition != item->condition()))
                condition = -1;
            if ((retain >= 0) && (retain != (item->retain() ? 1 : 0)))
                retain = -1;
            if ((stockroom >= 0) && (stockroom != (item->stockroom() ? 1 : 0)))
                stockroom = -1;
        }
    }
    findAction("edit_status_include")->setChecked(status == BrickLink::Include);
    findAction("edit_status_exclude")->setChecked(status == BrickLink::Exclude);
    findAction("edit_status_extra")->setChecked(status == BrickLink::Extra);

    findAction("edit_cond_new")->setChecked(condition == BrickLink::New);
    findAction("edit_cond_used")->setChecked(condition == BrickLink::Used);

    findAction("edit_retain_yes")->setChecked(retain == 1);
    findAction("edit_retain_no")->setChecked(retain == 0);

    findAction("edit_stockroom_yes")->setChecked(stockroom == 1);
    findAction("edit_stockroom_no")->setChecked(stockroom == 0);
}

void CFrameWork::statisticsUpdate()
{
    QString ss, es;

    if (m_current_window)
    {
        CDocument::ItemList not_exclude;

        foreach(CDocument::Item *item, m_current_window->document()->items()) {
            if (item->status() != BrickLink::Exclude)
                not_exclude.append(item);
        }

        CDocument::Statistics stat = m_current_window->document()->statistics(not_exclude);
        QString valstr, wgtstr;

        if (stat.value() != stat.minValue()) {
            valstr = QString("%1 (%2 %3)").
                     arg(stat.value().toLocalizedString(true)).
                     arg(tr("min.")).
                     arg(stat.minValue().toLocalizedString(true));
        }
        else
            valstr = stat.value().toLocalizedString(true);

        if (stat.weight() == -DBL_MIN) {
            wgtstr = "-";
        }
        else {
            double weight = stat.weight();

            if (weight < 0) {
                weight = -weight;
                wgtstr = tr("min.") + " ";
            }

            wgtstr += CLocaleMeasurement::weightToString(weight, true, true);
        }

        ss = QString("  %1: %2   %3: %4   %5: %6   %7: %8  ").
             arg(tr("Lots")).arg(stat.lots()).
             arg(tr("Items")).arg(stat.items()).
             arg(tr("Value")).arg(valstr).
             arg(tr("Weight")).arg(wgtstr);

        if ((stat.errors() > 0) && CConfig::inst()->showInputErrors())
            es = QString("  %1: %2  ").arg(tr("Errors")).arg(stat.errors());
    }

    m_statistics->setText(ss);
    m_errors->setText(es);

    emit statisticsChanged(m_current_window);
}

void CFrameWork::modificationUpdate()
{
    bool b = m_undogroup->activeStack() ? m_undogroup->activeStack()->isClean() : true;

    QAction *a = findAction("file_save");
    if (a)
        a->setEnabled(!b);

    static QPixmap pnomod, pmod;

    if (pmod.isNull()) {
        int h = m_modified->height() - 2;

        pmod = QPixmap(h, h);
        pmod.fill(Qt::black);

        pnomod = pmod;
        pnomod.setMask(pnomod.createHeuristicMask());

        QPixmap p(":/images/status/modified");
        if (!p.isNull())
            pmod = p.scaled(h, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_modified->setPixmap(b ? pnomod : pmod);

    if (b)
        m_modified->setToolTip(QString());
    else
        m_modified->setToolTip(tr("Unsaved modifications"));
}

void CFrameWork::gotPictureProgress(int p, int t)
{
// m_progress->setItemProgress ( PGI_Picture, p, t );
}

void CFrameWork::gotPriceGuideProgress(int p, int t)
{
// m_progress->setItemProgress ( PGI_PriceGuide, p, t );
}

void CFrameWork::configure()
{
    configure(0);
}

void CFrameWork::configure(const char *page)
{
    /* DlgSettingsImpl dlg ( this );

     if ( page )
      dlg.setCurrentPage ( page );

     dlg.exec ( );*/
}

void CFrameWork::setWindowMode(QAction *act)
{
    bool below = (act == findAction("window_mode_tab_below"));

    m_mdi->setTabMode(below ? CWorkspace::BottomTabs : CWorkspace::TopTabs);

}

void CFrameWork::setOnlineStatus(QAction *act)
{
    bool b = (act == findAction("extras_net_online"));

    if (!b && m_running)
        cancelAllTransfers();

    CConfig::inst()->setOnlineStatus(b);
}

void CFrameWork::registrationUpdate()
{
    bool personal = (CConfig::inst()->registration() == CConfig::Personal);
    bool demo = (CConfig::inst()->registration() == CConfig::Demo);

    QAction *a = findAction("view_simple_mode");

    // personal ->always on
    // demo ->always off
    a->setChecked((CConfig::inst()->simpleMode() || personal) && !demo);
    a->setEnabled(!(demo || personal));

    setSimpleMode(a->isChecked());
}

void CFrameWork::setSimpleMode(bool b)
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

    for (const char **action_ptr = actions; *action_ptr; action_ptr++) {
        QAction *a = findAction(*action_ptr);

        if (a)
            a->setVisible(!b);
    }
}

void CFrameWork::showContextMenu(bool /*onitem*/, const QPoint &pos)
{
    m_contextmenu->popup(pos);
}

void CFrameWork::addToRecentFiles(const QString &s)
{
    QString name = QDir::convertSeparators(QFileInfo(s).absoluteFilePath());

    m_recent_files.removeAll(name);
    m_recent_files.prepend(name);

    while (m_recent_files.count() > MaxRecentFiles)
        m_recent_files.pop_back();
}

void CFrameWork::closeEvent(QCloseEvent *e)
{
    if (!closeAllWindows()) {
        e->ignore();
        return;
    }

    QMainWindow::closeEvent(e);
}


bool CFrameWork::closeAllWindows()
{
    QList <CWindow *> list = allWindows();

    foreach(CWindow *w, list) {
        if (!w->close())
            return false;
    }
    return true;
}

QList <CWindow *> CFrameWork::allWindows() const
{
    QList <CWindow *> res;

    foreach(QWidget *w, m_mdi->allWindows()) {
        CWindow *win = qobject_cast<CWindow *> (w);
        if (win)
            res << win;
    }
    return res;
}

void CFrameWork::cancelAllTransfers()
{
    if (CMessageBox::question(this, tr("Do you want to cancel all outstanding inventory, image and Price Guide transfers?"), CMessageBox::Yes | CMessageBox::No) == CMessageBox::Yes) {
        BrickLink::inst()->cancelPictureTransfers();
        BrickLink::inst()->cancelPriceGuideTransfers();
    }
}

void CFrameWork::toggleAddItemDialog(bool b)
{
    if (!m_add_dialog) {
        m_add_dialog = new DAddItem(this);
        m_add_dialog->setObjectName("additems");

        QByteArray ba = CConfig::inst()->value("/MainWindow/AddItemDialog/Geometry").toByteArray();

        if (ba.isEmpty() || !m_add_dialog->restoreGeometry(ba))
            ; // should the dialog be centered??

//  QShortcut *sc = new QShortcut ( m_add_dialog );

//  QAction *action = findAction ( "edit_additems" );
//  sc->connectItem ( acc->insertItem ( action->shortcut ( )), action, SLOT( toggle ( )));

        connect(m_add_dialog, SIGNAL(closed()), this, SLOT(closedAddItemDialog()));

        m_add_dialog->attach(m_current_window);
    }

    if (b) {
        if (m_add_dialog->isVisible()) {
            m_add_dialog->raise();
            m_add_dialog->activateWindow();
        }
        else {
            m_add_dialog->show();
        }
    }
    else
        m_add_dialog->close();
}

void CFrameWork::closedAddItemDialog()
{
    findAction("edit_additems")->setChecked(m_add_dialog && m_add_dialog->isVisible());
}

