/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <cfloat>

#include <QUndoStack>
#include <QAction>
#include <QDesktopWidget>
#include <QCloseEvent>
#include <QMetaObject>
#include <QMetaMethod>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QPainter>
#include <QTimer>
#include <QLabel>
#include <QBitmap>
#include <QDir>
#include <QFileInfo>
#include <QToolButton>
#include <QToolTip>
#include <QCursor>
#include <QStyleFactory>
#include <QShortcut>
#include <QDockWidget>
#include <QSizeGrip>

#include "application.h"
#include "messagebox.h"
#include "window.h"
#include "document.h"
#include "config.h"
#include "currency.h"
#include "progresscircle.h"
#include "undo.h"
#include "filteredit.h"
#include "workspace.h"
#include "taskwidgets.h"
#include "progressdialog.h"
#include "updatedatabase.h"
#include "utility.h"
#include "additemdialog.h"
#include "settingsdialog.h"
#include "itemdetailpopup.h"
#include "changecurrencydialog.h"

#include "framework.h"


namespace {

enum ProgressItem {
    PGI_PriceGuide,
    PGI_Picture
};

} // namespace


class NoFrameStatusBar : public QStatusBar {
public:
    NoFrameStatusBar(QWidget *parent = 0)
        : QStatusBar(parent)
    { }

    void addPermanentWidget(QWidget *w, int stretch, int margin = 6)
    {
        QWidget *wrapper = new QWidget();
        QBoxLayout *l = new QHBoxLayout(wrapper);
        l->setContentsMargins(margin, 0, margin, 0);
        l->setSpacing(0);
        l->addWidget(w);
        QStatusBar::addPermanentWidget(wrapper, stretch);
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        // nearly the same as QStatusBar::paintEvent(), minus those ugly frames
        QString msg = currentMessage();

        QPainter p(this);
        QStyleOption opt;
        opt.initFrom(this);
        style()->drawPrimitive(QStyle::PE_PanelStatusBar, &opt, &p, this);

        if (!msg.isEmpty()) {
            p.setPen(palette().foreground().color());
            QRect msgr = rect().adjusted(6, 0, -6, 0);
            p.drawText(msgr, Qt::AlignLeading | Qt::AlignVCenter | Qt::TextSingleLine, msg);
        } else {
#ifdef Q_OS_MACX
            QColor lineColor(112, 112, 112));
            int offset = 0;
#else
            QColor lineColor = palette().color(QPalette::Midlight);
            int offset = 2;
#endif
            p.setPen(lineColor);

            foreach (QWidget *w, findChildren<QWidget *>()) {
                if (qobject_cast<QSizeGrip *>(w))
                    continue;
                QRect r = w->geometry();
                p.drawLine(r.left() - 3, offset, r.left() - 3, height() - offset - 1);
            }
        }
    }
};

class RecentMenu : public QMenu {
    Q_OBJECT
public:
    RecentMenu(FrameWork *fw, QWidget *parent)
        : QMenu(parent), m_fw(fw)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
        connect(this, SIGNAL(triggered(QAction *)), this, SLOT(openRecentAction(QAction *)));
    }

signals:
    void openRecent(int);

private slots:
    void buildMenu()
    {
        clear();

        int i = 0;
        foreach (QString s, m_fw->recentFiles()) {
            if (i < 10)
                s.prepend(QString("&%1   ").arg((i+1)%10));
            addAction(s)->setData(i++);
        }
        if (!i)
            addAction(tr("No recent files"))->setEnabled(false);
    }

    void openRecentAction(QAction *a)
    {
        if (a && !a->data().isNull()) {
            int idx = qvariant_cast<int>(a->data());
            emit openRecent(idx);
        }
    }

private:
    FrameWork *m_fw;
};





FrameWork *FrameWork::s_inst = 0;

FrameWork *FrameWork::inst()
{
    if (!s_inst)
        (new FrameWork())->setAttribute(Qt::WA_DeleteOnClose);

    return s_inst;
}



FrameWork::FrameWork(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
{
    s_inst = this;

    m_running = false;
    m_filter = 0;
    m_progress = 0;

    setUnifiedTitleAndToolBarOnMac(true);
    setDocumentMode(true);
    setAcceptDrops(true);

    m_undogroup = new UndoGroup(this);

    connect(Application::inst(), SIGNAL(openDocument(const QString &)), this, SLOT(openDocument(const QString &)));

    m_recent_files = Config::inst()->value("/Files/Recent").toStringList();
    while (m_recent_files.count() > MaxRecentFiles)
        m_recent_files.pop_back();

    m_current_window = 0;

    m_workspace = new Workspace(this);
    connect(m_workspace, SIGNAL(windowActivated(QWidget *)), this, SLOT(connectWindow(QWidget *)));

    setCentralWidget(m_workspace);

    m_task_info = new TaskInfoWidget(0);
    m_task_info->setObjectName(QLatin1String("TaskInfo"));
    addDockWidget(Qt::LeftDockWidgetArea, createDock(m_task_info));

    m_task_priceguide = new TaskPriceGuideWidget(0);
    m_task_priceguide->setObjectName(QLatin1String("TaskPriceGuide"));
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_priceguide), Qt::Vertical);

    m_task_appears = new TaskAppearsInWidget(0);
    m_task_appears->setObjectName(QLatin1String("TaskAppears"));
    tabifyDockWidget(m_dock_widgets.at(1), createDock(m_task_appears));

    m_task_links = new TaskLinksWidget(0);
    m_task_links->setObjectName(QLatin1String("TaskLinks"));
    tabifyDockWidget(m_dock_widgets.first(), createDock(m_task_links));

    m_dock_widgets.at(0)->raise();
    m_dock_widgets.at(1)->raise();

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName(QLatin1String("toolbar"));

    createActions();

    menuBar()->addMenu(createMenu("file", QList<QByteArray>()
        << "file_new"
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
//#if !defined(Q_OS_MAC)
        << "file_print_pdf"
//#endif
        << "-"
        << "file_close"
        << "-"
        << "file_exit"
    ));

    menuBar()->addMenu(createMenu("edit", QList<QByteArray>()
        << "edit_undo"
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
        << "edit_setmatch"
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
        << "edit_bl_myinventory"
    ));


    menuBar()->addMenu(createMenu("view", QList<QByteArray>()
        << "view_toolbar"
        << "view_infobar"
        << "view_statusbar"
        << "-"
        << "view_fullscreen"
        << "-"
        << "view_show_input_errors"
        << "view_difference_mode"
        << "-"
        << "view_save_default_col"
    ));

    menuBar()->addMenu(createMenu("extras", QList<QByteArray>()
        << "extras_update_database"
        << "-"
        << "extras_configure"
    ));

    QMenu *m = m_workspace->windowMenu(true, this);
    m->menuAction()->setObjectName(QLatin1String("window"));
    menuBar()->addMenu(m);

    menuBar()->addMenu(createMenu("help", QList<QByteArray>()
        << "help_updates"
        << "-"
        << "help_about"
    ));

    m_contextmenu = createMenu("context", QList<QByteArray>()
        << "edit_cut"
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
        << "edit_bl_myinventory"
    );

    setupToolBar(m_toolbar, QList<QByteArray>()
        << "file_new"
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
        << "|"
        << "widget_filter"
        << "|"
        << "widget_progress"
        << "|"
    );

    addToolBar(m_toolbar);

    createStatusBar();
    findAction("view_statusbar")->setChecked(Config::inst()->value(QLatin1String("/MainWindow/Statusbar/Visible"), true).toBool());

    languageChange();

    BrickLink::Core *bl = BrickLink::core();

    connect(Application::inst(), SIGNAL(onlineStateChanged(bool)), this, SLOT(onlineStateChanged(bool)));
    connect(Config::inst(), SIGNAL(updateIntervalsChanged(QMap<QByteArray,int>)), bl, SLOT(setUpdateIntervals(QMap<QByteArray,int>)));
    connect(Config::inst(), SIGNAL(proxyChanged(QNetworkProxy)), bl->transfer(), SLOT(setProxy(QNetworkProxy)));
    connect(Config::inst(), SIGNAL(measurementSystemChanged(QLocale::MeasurementSystem)), this, SLOT(statisticsUpdate()));

    findAction("view_show_input_errors")->setChecked(Config::inst()->showInputErrors());

    onlineStateChanged(Application::inst()->isOnline());

    bl->setUpdateIntervals(Config::inst()->updateIntervals());

    connect(bl, SIGNAL(transferJobProgress(int,int)), this, SLOT(transferJobProgressUpdate(int,int)));

    connect(m_undogroup, SIGNAL(cleanChanged(bool)), this, SLOT(modificationUpdate()));

    bool dbok = BrickLink::core()->readDatabase();

    if (!dbok) {
        if (MessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />Should these files be updated now?"), MessageBox::Yes | MessageBox::No) == MessageBox::Yes)
            dbok = updateDatabase();
    }

    if (dbok)
        Application::inst()->enableEmitOpenDocument();
    else
        MessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />The program is not functional without these files."));

    m_add_dialog = 0;
    //createAddItemDialog();

    m_running = true;

    connectAllActions(false, 0);    // init enabled/disabled status of document actions
    connectWindow(0);

    // we need to show now, since most X11 window managers and Mac OS X with
    // unified-toolbar look won't get the position right otherwise
    show();

    QByteArray ba;

    ba = Config::inst()->value(QLatin1String("/MainWindow/Layout/Geometry")).toByteArray();
    if (ba.isEmpty() || !restoreGeometry(ba)) {
        float dw = qApp->desktop()->width() / 10.f;
        float dh = qApp->desktop()->height() / 10.f;

        setGeometry(int(dw), int(dh), int (8 * dw), int(8 * dh));
        setWindowState(Qt::WindowMaximized);
    }
    ba = Config::inst()->value(QLatin1String("/MainWindow/Layout/State")).toByteArray();
    if (ba.isEmpty() || !restoreState(ba))
        m_toolbar->show();

    findAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

    QDateTime rateUpdate = Currency::inst()->lastUpdate();
    //if (!rateUpdate.isValid() || rateUpdate.daysTo(QDateTime::currentDateTime()) >= 1)
        Currency::inst()->updateRates();
}

void FrameWork::languageChange()
{
    m_toolbar->setWindowTitle(tr("Toolbar"));

    foreach (QDockWidget *dock, m_dock_widgets) {
        QString name = dock->objectName();

        if (name == QLatin1String("Dock-TaskInfo"))
            dock->setWindowTitle(tr("Info"));
        if (name == QLatin1String("Dock-TaskPriceGuide"))
            dock->setWindowTitle(tr("Price Guide"));
        if (name == QLatin1String("Dock-TaskAppears"))
            dock->setWindowTitle(tr("Appears In Sets"));
        if (name == QLatin1String("Dock-TaskLinks"))
            dock->setWindowTitle(tr("Links"));
    }
    if (m_filter) {
        m_filter->setToolTip(tr("Filter the list using this pattern (wildcards allowed: * ? [])"));
        m_filter->setPlaceholderText(tr("Filter"));
    }
    if (m_progress) {
        m_progress->setToolTipTemplates(tr("Offline"),
                                        tr("No outstanding jobs"),
                                        tr("Downloading...<br><b>%p%</b> finished<br>(%v of %m)"));
    }
/*
    foreach (QAction *a, m_filter->menu()->actions()) {
        QString s;
        int i = qvariant_cast<int>(a->data());

        switch(i) {
        case Window::All       : s = tr("All"); break;
        case Window::Prices    : s = tr("All Prices"); break;
        case Window::Texts     : s = tr("All Texts"); break;
        case Window::Quantities: s = tr("All Quantities"); break;
        default                 : s = Document::headerDataForDisplayRole(static_cast<Document::Field>(i)); break;
        }
        a->setText(s);
    }

*/

    translateActions();

    statisticsUpdate();
}


void FrameWork::translateActions()
{
#if defined(Q_OS_MAC)
    static bool onMac = true;
#else
    static bool onMac = false;
#endif

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
        { "file_print_pdf",                 tr("Print to PDF..."),                    0 },
        { "file_import",                    tr("Import"),                             0 },
        { "file_import_bl_inv",             tr("BrickLink Set Inventory..."),         tr("Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "file_import_bl_xml",             tr("BrickLink XML..."),                   tr("Ctrl+I,Ctrl+X", "File|Import BrickLink XML") },
        { "file_import_bl_order",           tr("BrickLink Order..."),                 tr("Ctrl+I,Ctrl+O", "File|Import BrickLink Order") },
        { "file_import_bl_store_inv",       tr("BrickLink Store Inventory..."),       tr("Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory") },
        { "file_import_bl_cart",            tr("BrickLink Shopping Cart..."),         tr("Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart") },
        { "file_import_peeron_inv",         tr("Peeron Inventory..."),                tr("Ctrl+I,Ctrl+P", "File|Import Peeron Inventory") },
        { "file_import_ldraw_model",        tr("LDraw Model..."),                     tr("Ctrl+I,Ctrl+L", "File|Import LDraw Model") },
        { "file_export",                    tr("Export"),                             0 },
        { "file_export_bl_xml",             tr("BrickLink XML..."),                         tr("Ctrl+E,Ctrl+X", "File|Import BrickLink XML") },
        { "file_export_bl_xml_clip",        tr("BrickLink Mass-Upload XML to Clipboard"),   tr("Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload") },
        { "file_export_bl_update_clip",     tr("BrickLink Mass-Update XML to Clipboard"),   tr("Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update") },
        { "file_export_bl_invreq_clip",     tr("BrickLink Set Inventory XML to Clipboard"), tr("Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "file_export_bl_wantedlist_clip", tr("BrickLink Wanted List XML to Clipboard"),   tr("Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List") },
        { "file_close",                     tr("Close"),                              tr("Ctrl+W", "File|Close") },
        { "file_exit",                      tr("Exit"),                               tr("Ctrl+Q", "File|Quit") },
        { "edit",                           tr("&Edit"),                              0 },
        { "edit_undo",                      0,                                          tr("Ctrl+Z", "Edit|Undo") },
        { "edit_redo",                      0,                                          tr("Ctrl+Y", "Edit|Redo") },
        { "edit_cut",                       tr("Cut"),                                tr("Ctrl+X", "Edit|Cut") },
        { "edit_copy",                      tr("Copy"),                               tr("Ctrl+C", "Edit|Copy") },
        { "edit_paste",                     tr("Paste"),                              tr("Ctrl+V", "Edit|Paste") },
        { "edit_delete",                    tr("Delete"),                             onMac ? tr("Backspace", "Edit|Delete (Mac)") : tr("Delete", "Edit|Delete (Win,Unix)") },
        { "edit_additems",                  tr("Add Items..."),                       tr("Insert", "Edit|AddItems") },
        { "edit_subtractitems",             tr("Subtract Items..."),                  0 },
        { "edit_mergeitems",                tr("Consolidate Items..."),               0 },
        { "edit_partoutitems",              tr("Part out Item..."),                   0 },
        { "edit_setmatch",                  tr("Match Items against Set Inventories..."),                   0 },
        { "edit_reset_diffs",               tr("Reset Differences"),                  0 },
        { "edit_copyremarks",               tr("Copy Remarks from Document..."),      0 },
        { "edit_select_all",                tr("Select All"),                         tr("Ctrl+A", "Edit|SelectAll") },
        { "edit_select_none",               tr("Select None"),                        tr("Ctrl+Shift+A", "Edit|SelectNone") },
        { "view",                           tr("&View"),                              0 },
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
        { "window",                         tr("&Windows"),                           0 },
        { "help",                           tr("&Help"),                              0 },
//        { "help_whatsthis",                 tr("What's this?"),                       tr("Shift+F1", "Help|WhatsThis") },
        { "help_about",                     tr("About..."),                           0 },
        { "help_updates",                   tr("Check for Program Updates..."),       0 },
        { "edit_status",                    tr("Status"),                             0 },
        { "edit_status_include",            tr("Include"),                            0 },
        { "edit_status_exclude",            tr("Exclude"),                            0 },
        { "edit_status_extra",              tr("Extra"),                              0 },
        { "edit_status_toggle",             tr("Toggle Include/Exclude"),             0 },
        { "edit_cond",                      tr("Condition"),                          0 },
        { "edit_cond_new",                  tr("New", "Cond|New"),                    0 },
        { "edit_cond_used",                 tr("Used"),                               0 },
        { "edit_cond_toggle",               tr("Toggle New/Used"),                    0 },
        { "edit_subcond_none",              tr("None", "SubCond|None"),               0 },
        { "edit_subcond_misb",              tr("MISB", "SubCond|MISB"),               0 },
        { "edit_subcond_complete",          tr("Complete", "SubCond|Complete"),       0 },
        { "edit_subcond_incomplete",        tr("Incomplete", "SubCond|Incomplete"),   0 },
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
            if (!atptr->m_shortcut.isNull()) {
                a->setShortcut(QKeySequence(atptr->m_shortcut));
                a->setToolTip(QString("%1 <span style=\"color: gray; font-size: small\">%2</span>")
                              .arg(a->toolTip()).arg(a->shortcut().toString(QKeySequence::NativeText)));
            }
        }
    }
}

FrameWork::~FrameWork()
{
    Config::inst()->setValue("/Files/Recent", m_recent_files);

    Config::inst()->setValue("/MainWindow/Statusbar/Visible", statusBar()->isVisibleTo(this));
    Config::inst()->setValue("/MainWindow/Layout/State", saveState());
    Config::inst()->setValue("/MainWindow/Layout/Geometry", saveGeometry());

    if (m_add_dialog)
        Config::inst()->setValue("/MainWindow/AddItemDialog/Geometry", m_add_dialog->saveGeometry());

    delete m_workspace;
    s_inst = 0;
}


void FrameWork::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
    }
}

void FrameWork::dropEvent(QDropEvent *e)
{
    foreach (QUrl u, e->mimeData()->urls())
        openDocument(u.toLocalFile());

    e->setDropAction(Qt::CopyAction);
    e->accept();
}

QAction *FrameWork::findAction(const char *name)
{
    return !name || !name[0] ? 0 : static_cast <QAction *>(findChild<QAction *>(QLatin1String(name)));
}

QDockWidget *FrameWork::createDock(QWidget *widget)
{
    QDockWidget *dock = new QDockWidget(QString(), this);
    dock->setObjectName(QLatin1String("Dock-") + widget->objectName());
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    dock->setWidget(widget);
    m_dock_widgets.append(dock);
    return dock;
}

void FrameWork::createStatusBar()
{
    NoFrameStatusBar *st = new NoFrameStatusBar(this);
    setStatusBar(st);

    int margin = 2 * st->fontMetrics().width(QLatin1Char(' '));

    m_st_errors = new QLabel();
    m_st_lots = new QLabel();
    m_st_items = new QLabel();
    m_st_weight = new QLabel();
    m_st_value = new QLabel();
    m_st_currency = new QToolButton();
    m_st_currency->setPopupMode(QToolButton::InstantPopup);
    m_st_currency->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_st_currency->setAutoRaise(true);
    QWidget *st_valcur = new QWidget();
    QHBoxLayout *l = new QHBoxLayout(st_valcur);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    l->addWidget(m_st_value);
    l->addWidget(m_st_currency);

    connect(m_st_currency, SIGNAL(triggered(QAction*)), this, SLOT(changeDocumentCurrency(QAction *)));
    connect(Currency::inst(), SIGNAL(ratesChanged()), this, SLOT(updateCurrencyRates()));
    updateCurrencyRates();

    st->addPermanentWidget(m_st_errors, 0, margin);
    st->addPermanentWidget(m_st_weight, 0, margin);
    st->addPermanentWidget(m_st_lots, 0, margin);
    st->addPermanentWidget(m_st_items, 0, margin);
    st->addPermanentWidget(st_valcur, 0, margin);

    statusBar()->hide();
}

void FrameWork::changeDocumentCurrency(QAction *a)
{
    if (m_current_window) {
        ChangeCurrencyDialog d(m_current_window->document()->currencyCode(), a->text(), this);
        if (d.exec() == QDialog::Accepted) {
            double rate = d.exchangeRate();

            if (rate > 0) {
                m_current_window->document()->setCurrencyCode(a->text(), rate);
            }
        }
    }
}

void FrameWork::updateCurrencyRates()
{
    foreach (QAction *a, m_st_currency->actions()) {
        m_st_currency->removeAction(a);
        delete a;
    }

    foreach (const QString &c, Currency::inst()->currencyCodes())
        m_st_currency->addAction(new QAction(c, m_st_currency));
}

QMenu *FrameWork::createMenu(const QByteArray &name, const QList<QByteArray> &a_names)
{
    if (a_names.isEmpty())
        return 0;

    QMenu *m = new QMenu(this);
    m->menuAction()->setObjectName(QLatin1String(name.constData()));

    foreach(const QByteArray &an, a_names) {
        if (an == "-")
            m->addSeparator();
        else if (QAction *a = findAction(an))
            m->addAction(a);
        else
            qWarning("Couldn't find action '%s'", an.constData());
    }
    return m;
}


bool FrameWork::setupToolBar(QToolBar *t, const QList<QByteArray> &a_names)
{
    if (!t || a_names.isEmpty())
        return false;

    foreach(const QByteArray &an, a_names) {
        if (an == "-") {
            t->addSeparator();
        } else if (an == "|") {
            QWidget *spacer = new QWidget();
            int sp = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setFixedSize(sp, sp);
            t->addWidget(spacer);
        } else if (an == "<>") {
            QWidget *spacer = new QWidget();
            int sp = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setMinimumSize(sp, sp);
            spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            t->addWidget(spacer);
        } else if (an.startsWith("widget_")) {
            if (an == "widget_filter") {
                if (m_filter) {
                    qWarning("Only one filter widget can be added to toolbars");
                    continue;
                }

                m_filter = new FilterEdit();

                QMenu *m = new QMenu(this);
             //   QActionGroup *ag = new QActionGroup(m);
             /*   for (int i = 0; i < (Window::FilterCountSpecial + Document::FieldCount); i++) {
                    QAction *a = new QAction(ag);
                    a->setCheckable(true);

                    if (i < Window::FilterCountSpecial)
                        a->setData(-i - 1);
                    else
                        a->setData(i - Window::FilterCountSpecial);

                    if (i == Window::FilterCountSpecial) */
                        m->addSeparator(); /*
                    else if (i == 0)
                        a->setChecked(true);

                    m->addAction(a);
                }
                */
                m_filter->setMenu(m);
                t->addWidget(m_filter);
            } else if (an == "widget_progress") {
                if (m_progress) {
                    qWarning("Only one progress widget can be added to toolbars");
                    continue;
                }
                m_progress = new ProgressCircle();
                m_progress->setIcon(QIcon(":/images/icon.png"));
                t->addWidget(m_progress);
            }
        } else if (QAction *a = findAction(an)) {
            t->addAction(a);

            // workaround for Qt4 bug: can't set the popup mode on a QAction
            QToolButton *tb;
            if (a->menu() && (tb = qobject_cast<QToolButton *>(t->widgetForAction(a))))
                tb->setPopupMode(QToolButton::InstantPopup);
        } else {
            qWarning("Couldn't find action '%s'", an.constData());
        }
    }
    return true;
}

inline static QMenu *newQMenu(QWidget *parent, const char *name)
{
    QMenu *m = new QMenu(parent);
    m->menuAction()->setObjectName(QLatin1String(name));
    return m;
}

inline static QAction *newQAction(QObject *parent, const char *name, bool toggle = false, QObject *receiver = 0, const char *slot = 0)
{
    QAction *a = new QAction(parent);
    a->setObjectName(QLatin1String(name));
    if (toggle)
        a->setCheckable(true);
    if (receiver && slot)
        QObject::connect(a, toggle ? SIGNAL(toggled(bool)) : SIGNAL(triggered()), receiver, slot);
    return a;
}

inline static QActionGroup *newQActionGroup(QObject *parent, const char *name, bool exclusive = false)
{
    QActionGroup *g = new QActionGroup(parent);
    g->setObjectName(QLatin1String(name));
    g->setExclusive(exclusive);
    return g;
}

void FrameWork::createActions()
{
    QAction *a;
    QActionGroup *g;
    QMenu *m;


    a = newQAction(this, "file_new", false, this, SLOT(fileNew()));
    a = newQAction(this, "file_open", false, this, SLOT(fileOpen()));

    m = new RecentMenu(this, this);
    m->menuAction()->setObjectName("file_open_recent");
    connect(m, SIGNAL(openRecent(int)), this, SLOT(fileOpenRecent(int)));

    (void) newQAction(this, "file_save");
    (void) newQAction(this, "file_saveas");
    (void) newQAction(this, "file_print");
    (void) newQAction(this, "file_print_pdf");

    m = newQMenu(this, "file_import");
    m->addAction(newQAction(this, "file_import_bl_inv", false, this, SLOT(fileImportBrickLinkInventory())));
    m->addAction(newQAction(this, "file_import_bl_xml", false, this, SLOT(fileImportBrickLinkXML())));
    m->addAction(newQAction(this, "file_import_bl_order", false, this, SLOT(fileImportBrickLinkOrder())));
    m->addAction(newQAction(this, "file_import_bl_store_inv", false, this, SLOT(fileImportBrickLinkStore())));
    m->addAction(newQAction(this, "file_import_bl_cart", false, this, SLOT(fileImportBrickLinkCart())));
    m->addAction(newQAction(this, "file_import_peeron_inv", false, this, SLOT(fileImportPeeronInventory())));
    m->addAction(newQAction(this, "file_import_ldraw_model", false, this, SLOT(fileImportLDrawModel())));

    m = newQMenu(this, "file_export");
    m->addAction(newQAction(this, "file_export_bl_xml"));
    m->addAction(newQAction(this, "file_export_bl_xml_clip"));
    m->addAction(newQAction(this, "file_export_bl_update_clip"));
    m->addAction(newQAction(this, "file_export_bl_invreq_clip"));
    m->addAction(newQAction(this, "file_export_bl_wantedlist_clip"));

    (void) newQAction(this, "file_close");

    a = newQAction(this, "file_exit", false, this, SLOT(close()));
    a->setMenuRole(QAction::QuitRole);

    m_undogroup->createUndoAction(this)->setObjectName("edit_undo");
    m_undogroup->createRedoAction(this)->setObjectName("edit_redo");

    a = newQAction(this, "edit_cut");
    connect(new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete), this, 0, 0, Qt::WindowShortcut), SIGNAL(activated()), a, SLOT(trigger()));
    a = newQAction(this, "edit_copy");
    connect(new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Insert), this, 0, 0, Qt::WindowShortcut), SIGNAL(activated()), a, SLOT(trigger()));
    a = newQAction(this, "edit_paste");
    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Insert), this, 0, 0, Qt::WindowShortcut), SIGNAL(activated()), a, SLOT(trigger()));
    (void) newQAction(this, "edit_delete");

    a = newQAction(this, "edit_additems", false, this, SLOT(showAddItemDialog()));
    a->setShortcutContext(Qt::ApplicationShortcut);

    (void) newQAction(this, "edit_subtractitems");
    (void) newQAction(this, "edit_mergeitems");
    (void) newQAction(this, "edit_partoutitems");
    (void) newQAction(this, "edit_setmatch");
    (void) newQAction(this, "edit_reset_diffs");
    (void) newQAction(this, "edit_copyremarks");
    (void) newQAction(this, "edit_select_all");
    (void) newQAction(this, "edit_select_none");

    m = newQMenu(this, "edit_status");
    g = newQActionGroup(this, 0, true);
    m->addAction(newQAction(g, "edit_status_include", true));
    m->addAction(newQAction(g, "edit_status_exclude", true));
    m->addAction(newQAction(g, "edit_status_extra",   true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_status_toggle"));

    m = newQMenu(this, "edit_cond");
    g = newQActionGroup(this, 0, true);
    m->addAction(newQAction(g, "edit_cond_new", true));
    m->addAction(newQAction(g, "edit_cond_used", true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_cond_toggle"));
    m->addSeparator();
    g = newQActionGroup(this, 0, true);
    m->addAction(newQAction(g, "edit_subcond_none", true));
    m->addAction(newQAction(g, "edit_subcond_misb", true));
    m->addAction(newQAction(g, "edit_subcond_complete", true));
    m->addAction(newQAction(g, "edit_subcond_incomplete", true));

    (void) newQAction(this, "edit_color");

    m = newQMenu(this, "edit_qty");
    m->addAction(newQAction(this, "edit_qty_multiply"));
    m->addAction(newQAction(this, "edit_qty_divide"));

    m = newQMenu(this, "edit_price");
    m->addAction(newQAction(this, "edit_price_set"));
    m->addAction(newQAction(this, "edit_price_inc_dec"));
    m->addAction(newQAction(this, "edit_price_to_priceguide"));
    m->addAction(newQAction(this, "edit_price_round"));

    (void) newQAction(this, "edit_bulk");
    (void) newQAction(this, "edit_sale");

    m = newQMenu(this, "edit_comment");
    m->addAction(newQAction(this, "edit_comment_set"));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_comment_add"));
    m->addAction(newQAction(this, "edit_comment_rem"));

    m = newQMenu(this, "edit_remark");
    m->addAction(newQAction(this, "edit_remark_set"));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_remark_add"));
    m->addAction(newQAction(this, "edit_remark_rem"));

    m = newQMenu(this, "edit_retain");
    g = newQActionGroup(this, 0, true);
    m->addAction(newQAction(g, "edit_retain_yes", true));
    m->addAction(newQAction(g, "edit_retain_no", true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_retain_toggle"));

    m = newQMenu(this, "edit_stockroom");
    g = newQActionGroup(this, 0, true);
    m->addAction(newQAction(g, "edit_stockroom_yes", true));
    m->addAction(newQAction(g, "edit_stockroom_no", true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_stockroom_toggle"));

    (void) newQAction(this, "edit_reserved");

    (void) newQAction(this, "edit_bl_catalog");
    (void) newQAction(this, "edit_bl_priceguide");
    (void) newQAction(this, "edit_bl_lotsforsale");
    (void) newQAction(this, "edit_bl_myinventory");

    (void) newQAction(this, "view_fullscreen", true, this, SLOT(viewFullScreen(bool)));

    m_toolbar->toggleViewAction()->setObjectName("view_toolbar");
    m = newQMenu(this, "view_infobar");
    foreach (QDockWidget *dock, m_dock_widgets)
        m->addAction(dock->toggleViewAction());

    (void) newQAction(this, "view_statusbar", true, this, SLOT(viewStatusBar(bool)));
    (void) newQAction(this, "view_show_input_errors", true, Config::inst(), SLOT(setShowInputErrors(bool)));
    (void) newQAction(this, "view_difference_mode", true);
    (void) newQAction(this, "view_save_default_col");
    (void) newQAction(this, "extras_update_database", false, this, SLOT(updateDatabase()));

    a = newQAction(this, "extras_configure", false, this, SLOT(configure()));
    a->setMenuRole(QAction::PreferencesRole);

    //(void) newQAction(this, "help_whatsthis", false, this, SLOT(whatsThis()));

    a = newQAction(this, "help_about", false, Application::inst(), SLOT(about()));
    a->setMenuRole(QAction::AboutRole);

    a = newQAction(this, "help_updates", false, Application::inst(), SLOT(checkForUpdates()));
    a->setMenuRole(QAction::ApplicationSpecificRole);

    // set all icons that have a pixmap corresponding to name()

    QList<QAction *> alist = findChildren<QAction *>();
    foreach(QAction *a, alist) {
        if (!a->objectName().isEmpty()) {
            QString path = QLatin1String(":/images/") + a->objectName() + QLatin1String(".png");

            if (QFile::exists(path))
                a->setIcon(QIcon(path));
        }
    }
}

void FrameWork::viewStatusBar(bool b)
{
    statusBar()->setShown(b);
}

void FrameWork::viewToolBar(bool b)
{
    m_toolbar->setShown(b);
}

void FrameWork::viewFullScreen(bool b)
{
    QFlags<Qt::WindowState> ws = windowState();
    setWindowState(b ? ws | Qt::WindowFullScreen : ws & ~Qt::WindowFullScreen);
}

void FrameWork::openDocument(const QString &file)
{
    createWindow(Document::fileOpen(file));
}

void FrameWork::fileNew()
{
    createWindow(Document::fileNew());
}

void FrameWork::fileOpen()
{
    createWindow(Document::fileOpen());
}

void FrameWork::fileOpenRecent(int i)
{
    if (i < int(m_recent_files.count())) {
        // we need to copy the string here, because we delete it later
        // (this seems to work on Linux, but XP crashes in m_recent_files.remove ( ...)
        QString tmp = m_recent_files [i];

        openDocument(tmp);
    }
}

void FrameWork::fileImportPeeronInventory()
{
    createWindow(Document::fileImportPeeronInventory());
}

void FrameWork::fileImportBrickLinkInventory()
{
    fileImportBrickLinkInventory(0);
}


void FrameWork::fileImportBrickLinkInventory(const BrickLink::Item *item)
{
    createWindow(Document::fileImportBrickLinkInventory(item));
}

bool FrameWork::checkBrickLinkLogin()
{
    forever {
        QPair<QString, QString> auth = Config::inst()->loginForBrickLink();

        if (!auth.first.isEmpty() && !auth.second.isEmpty())
            return true;

        if (MessageBox::question(this, tr("No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?"), MessageBox::Yes | MessageBox::No) == MessageBox::Yes)
            configure("network");
        else
            return false;
    }
}

void FrameWork::fileImportBrickLinkOrder()
{
    if (!checkBrickLinkLogin())
        return;

    createWindows(Document::fileImportBrickLinkOrders());
}

void FrameWork::fileImportBrickLinkStore()
{
    if (!checkBrickLinkLogin())
        return;

    createWindow(Document::fileImportBrickLinkStore());
}

void FrameWork::fileImportBrickLinkXML()
{
    createWindow(Document::fileImportBrickLinkXML());
}

void FrameWork::fileImportBrickLinkCart()
{
    createWindow(Document::fileImportBrickLinkCart());
}

void FrameWork::fileImportLDrawModel()
{
    createWindow(Document::fileImportLDrawModel());
}

bool FrameWork::createWindows(const QList<Document *> &docs)
{
    bool ok = true;

    foreach (Document *doc, docs)
        ok &= createWindow(doc);
    return ok;
}



bool FrameWork::createWindow(Document *doc)
{
    if (!doc)
        return false;

    Window *window = 0;
    foreach(QWidget *w, m_workspace->windowList()) {
        Window *ww = qobject_cast<Window *>(w);

        if (ww && ww->document() == doc) {
            window = ww;
            break;
        }
    }
    if (!window) {
        m_undogroup->addStack(doc->undoStack());
        window = new Window(doc, 0);
        m_workspace->addWindow(window);
    }

    m_workspace->setActiveWindow(window);
    window->setFocus();
    return true;
}


bool FrameWork::updateDatabase()
{
    if (closeAllWindows()) {
        delete m_add_dialog;

        Transfer trans(1);
        trans.setProxy(Config::inst()->proxy());

        ProgressDialog d(&trans, this);
        UpdateDatabase update(&d);

        return d.exec();
    }
    return false;
}

void FrameWork::connectAction(bool do_connect, const char *name, Window *window, const char *windowslot, bool (Window::* is_on_func)() const)
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

void FrameWork::updateAllToggleActions(Window *window)
{
    for (QMap <QAction *, bool (Window::*)() const>::iterator it = m_toggle_updates.begin(); it != m_toggle_updates.end(); ++it) {
        it.key()->setChecked((window->* it.value())());
    }
}

void FrameWork::connectAllActions(bool do_connect, Window *window)
{
    QMetaObject mo = Window::staticMetaObject;
    QObjectList list = qFindChildren<QObject *>(this, QString());

    for (int i = 0; i < mo.methodCount(); ++i) {
        const char *slot = mo.method(i).signature();
        if (!slot || slot[0] != 'o' || slot[1] != 'n' || slot[2] != '_')
            continue;
        bool foundIt = false;


        for(int j = 0; j < list.count(); ++j) {
            QObject *co = list.at(j);
            QByteArray objName = co->objectName().toAscii();
            int len = objName.length();
            if (!len || qstrncmp(slot + 3, objName.data(), len) || slot[len+3] != '_')
                continue;
            const QMetaObject *smo = co->metaObject();
            int sigIndex = smo->indexOfMethod(slot + len + 4);
            if (sigIndex < 0) { // search for compatible signals
                int slotlen = qstrlen(slot + len + 4) - 1;
                for (int k = 0; k < co->metaObject()->methodCount(); ++k) {
                    if (smo->method(k).methodType() != QMetaMethod::Signal)
                        continue;

                    if (!qstrncmp(smo->method(k).signature(), slot + len + 4, slotlen)) {
                        sigIndex = k;
                        break;
                    }
                }
            }
            if (sigIndex < 0)
                continue;
            if (do_connect && window && QMetaObject::connect(co, sigIndex, window, i)) {
                foundIt = true;
            }
            else if (!do_connect && window) {
                // ignore errors on disconnect
                QMetaObject::disconnect(co, sigIndex, window, i);
                foundIt = true;
            }
            if (QAction *act = qobject_cast<QAction *>(co))
                act->setEnabled(do_connect);

            if (foundIt)
                break;
        }
        if (foundIt) {
            // we found our slot, now skip all overloads
            while (mo.method(i + 1).attributes() & QMetaMethod::Cloned)
                  ++i;
        }
        else if (window && (!(mo.method(i).attributes() & QMetaMethod::Cloned))) {
            qWarning("FrameWork::connectAllActions: No matching signal for %s", slot);
        }
    }
}

void FrameWork::connectWindowMdiArea(QMdiSubWindow *sw)
{
    connectWindow(sw ? sw->widget() : 0);
    if (sw && sw->widget() && (sw->widget() == m_current_window))
        return;
}

void FrameWork::connectWindow(QWidget *w)
{
    if (w && w == m_current_window)
        return;

    if (m_current_window) {
        Document *doc = m_current_window->document();

        connectAllActions(false, m_current_window);

        disconnect(doc, SIGNAL(statisticsChanged()), this, SLOT(statisticsUpdate()));
//        disconnect(doc, SIGNAL(currencyCodeChanged(QString)), this, SLOT(updateCurrencyButton()));
        disconnect(m_current_window, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        if (m_filter) {
            disconnect(m_filter, SIGNAL(textChanged(const QString &)), m_current_window, SLOT(setFilter(const QString &)));
            m_filter->setText(QString());
            m_filter->setToolTip(QString());
        }
        if (m_details) {
            disconnect(m_current_window, SIGNAL(currentChanged(Document::Item *)), this, SLOT(setItemDetailHelper(Document::Item *)));
            setItemDetailHelper(0);
        }
        m_undogroup->setActiveStack(0);

        m_current_window = 0;
    }

    if (Window *window = qobject_cast<Window *>(w)) {
        Document *doc = window->document();

        connectAllActions(true, window);

        connect(doc, SIGNAL(statisticsChanged()), this, SLOT(statisticsUpdate()));
//        connect(doc, SIGNAL(currencyCodeChanged(QString)), this, SLOT(updateCurrencyButton()));
        connect(window, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        if (m_filter) {
            m_filter->setText(window->filter());
            m_filter->setToolTip(window->filterToolTip());
            connect(m_filter, SIGNAL(textChanged(const QString &)), window, SLOT(setFilter(const QString &)));
        }
        if (m_details) {
            setItemDetailHelper(window->current());
            connect(window, SIGNAL(currentChanged(Document::Item *)), this, SLOT(setItemDetailHelper(Document::Item *)));
        }

        m_undogroup->setActiveStack(doc->undoStack());

        m_current_window = window;
    }

    if (m_add_dialog) {
        m_add_dialog->attach(m_current_window);
        if (!m_current_window)
            m_add_dialog->close();
    }

    findAction("edit_additems")->setEnabled((m_current_window));

    selectionUpdate(m_current_window ? m_current_window->selection() : Document::ItemList());
    statisticsUpdate();
    modificationUpdate();
    titleUpdate();
    //updateCurrencyButton();

    emit windowActivated(m_current_window);
}

void FrameWork::selectionUpdate(const Document::ItemList &selection)
{
    static const int NeedLotId = 1;
    static const int NeedInventory = 2;
    static const int NeedSubCondition = 4;

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
        { "edit_subcond_none",        1, 0, NeedSubCondition },
        { "edit_subcond_misb",        1, 0, NeedSubCondition },
        { "edit_subcond_complete",    1, 0, NeedSubCondition },
        { "edit_subcond_incomplete",  1, 0, NeedSubCondition },

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

                foreach(Document::Item *item, selection) {
                    if (f & NeedLotId)
                        b &= (item->lotId() != 0);
                    if (f & NeedInventory)
                        b &= (item->item() && item->item()->hasInventory());
                    if (f & NeedSubCondition)
                        b &= (item->item() && item->itemType() && item->item()->itemType()->hasSubConditions());
                }
            }

            a->setEnabled(b && (mins ? (cnt >= mins) : true) && (maxs ? (cnt <= maxs) : true));
        }
    }

    int status     = -1;
    int condition  = -1;
    int scondition = -1;
    int retain     = -1;
    int stockroom  = -1;

    if (cnt) {
        status     = selection.front()->status();
        condition  = selection.front()->condition();
        scondition = selection.front()->subCondition();
        retain     = selection.front()->retain()    ? 1 : 0;
        stockroom  = selection.front()->stockroom() ? 1 : 0;

        foreach(Document::Item *item, selection) {
            if ((status >= 0) && (status != item->status()))
                status = -1;
            if ((condition >= 0) && (condition != item->condition()))
                condition = -1;
            if ((scondition >= 0) && (scondition != item->subCondition()))
                scondition = -1;
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

    findAction("edit_subcond_none")->setChecked(scondition == BrickLink::None);
    findAction("edit_subcond_misb")->setChecked(scondition == BrickLink::MISB);
    findAction("edit_subcond_complete")->setChecked(scondition == BrickLink::Complete);
    findAction("edit_subcond_incomplete")->setChecked(scondition == BrickLink::Incomplete);

    findAction("edit_retain_yes")->setChecked(retain == 1);
    findAction("edit_retain_no")->setChecked(retain == 0);

    findAction("edit_stockroom_yes")->setChecked(stockroom == 1);
    findAction("edit_stockroom_no")->setChecked(stockroom == 0);
}

void FrameWork::statisticsUpdate()
{
    QString lotstr, itmstr, errstr, valstr, wgtstr, ccode;

    if (m_current_window)
    {
        Document::ItemList not_exclude;

        foreach(Document::Item *item, m_current_window->document()->items()) {
            if (item->status() != BrickLink::Exclude)
                not_exclude.append(item);
        }

        Document::Statistics stat = m_current_window->document()->statistics(not_exclude);
        ccode = m_current_window->document()->currencyCode();

        if (stat.value() != stat.minValue()) {
            valstr = tr("Value: %1 (min. %2)").
                     arg(Currency::toString(stat.value(), ccode, Currency::NoSymbol)).
                     arg(Currency::toString(stat.minValue(), ccode, Currency::NoSymbol));
        } else {
            valstr = tr("Value: %1").arg(Currency::toString(stat.value(), ccode, Currency::NoSymbol));
        }

        if (stat.weight() == -DBL_MIN) {
            wgtstr = tr("Weight: -");
        } else {
            double weight = stat.weight();

            if (weight < 0) {
                weight = -weight;
                wgtstr = tr("Weight: min. %1");
            } else {
                wgtstr = tr("Weight: %1");
            }

            wgtstr = wgtstr.arg(Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true));
        }

        lotstr = tr("Lots: %1").arg(stat.lots());
        itmstr = tr("Items: %1").arg(stat.items());

        if ((stat.errors() > 0) && Config::inst()->showInputErrors())
            errstr = tr("Errors: %1").arg(stat.errors());
    }

    m_st_lots->setText(lotstr);
    m_st_items->setText(itmstr);
    m_st_weight->setText(wgtstr);
    m_st_value->setText(valstr);
    m_st_currency->setEnabled(m_current_window);
    m_st_currency->setText(ccode + QLatin1String("  "));
    m_st_errors->setText(errstr);
}

void FrameWork::titleUpdate()
{
/*    QString t = Application::inst()->applicationName();

    if (m_current_window) {
        t.append(QString(" - %1 [*]").arg(m_current_window->windowTitle()));
    }
    setWindowTitle(t); */

    QString title = QApplication::applicationName();
    QString file;

    if (m_current_window) {
        QChar separator[] = { 0x0020, 0x2014, 0x0020 };

        title = m_current_window->windowTitle() + QString(separator, 3) + title;
        file = m_current_window->document()->fileName();
    }
    setWindowTitle(title);
    setWindowFilePath(file);
}

void FrameWork::modificationUpdate()
{
    bool modified = m_undogroup->activeStack() ? !m_undogroup->activeStack()->isClean() : false;

    if (QAction *a = findAction("file_save"))
        a->setEnabled(modified);

    setWindowModified(modified);
}

void FrameWork::transferJobProgressUpdate(int p, int t)
{
    if (p == t) {
        m_progress->reset();
    } else {
        if (m_progress->maximum() != t)
            m_progress->setMaximum(t);
        m_progress->setValue(p);
    }
}


void FrameWork::configure()
{
    configure(0);
}

void FrameWork::configure(const char *page)
{
    SettingsDialog d(page, this);
    d.exec();
}

void FrameWork::onlineStateChanged(bool isOnline)
{
    BrickLink::core()->setOnlineStatus(isOnline);
    if (m_progress)
        m_progress->setOnlineState(isOnline);
    if (!isOnline)
        cancelAllTransfers(true);
}

void FrameWork::showContextMenu(bool /*onitem*/, const QPoint &pos)
{
    m_contextmenu->popup(pos);
}

QStringList FrameWork::recentFiles() const
{
    return m_recent_files;
}

void FrameWork::addToRecentFiles(const QString &s)
{
    QString name = QDir::convertSeparators(QFileInfo(s).absoluteFilePath());

    m_recent_files.removeAll(name);
    m_recent_files.prepend(name);

    while (m_recent_files.count() > MaxRecentFiles)
        m_recent_files.pop_back();
}

void FrameWork::closeEvent(QCloseEvent *e)
{
    if (!closeAllWindows()) {
        e->ignore();
        return;
    }

    QMainWindow::closeEvent(e);
}


bool FrameWork::closeAllWindows()
{
    foreach(QWidget *w, m_workspace->windowList()) {
        if (!w->close())
            return false;
    }
    return true;
}

void FrameWork::cancelAllTransfers(bool force)
{
    if (force || MessageBox::question(this, tr("Do you want to cancel all outstanding inventory, image and Price Guide transfers?"), MessageBox::Yes | MessageBox::No) == MessageBox::Yes) {
        BrickLink::core()->cancelPictureTransfers();
        BrickLink::core()->cancelPriceGuideTransfers();
    }
}

void FrameWork::createAddItemDialog()
{
    if (!m_add_dialog) {
        m_add_dialog = new AddItemDialog();
        m_add_dialog->setObjectName(QLatin1String("additems"));

        QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/AddItemDialog/Geometry")).toByteArray();

        if (!ba.isEmpty())
            m_add_dialog->restoreGeometry(ba);

        m_add_dialog->attach(m_current_window);
    }
}

void FrameWork::showAddItemDialog()
{
    createAddItemDialog();

    if (m_details && m_details->isVisible())
        toggleItemDetailPopup();

    if (m_add_dialog->isVisible()) {
        m_add_dialog->raise();
        m_add_dialog->activateWindow();
    } else {
        m_add_dialog->show();
    }
}

void FrameWork::toggleItemDetailPopup()
{
    if (!m_details) {
        m_details = new ItemDetailPopup(this);

        if (m_current_window)
            connect(m_current_window, SIGNAL(currentChanged(Document::Item *)), this, SLOT(setItemDetailHelper(Document::Item *)));
    }

    if (!m_details->isVisible()) {
        m_details->show();
        setItemDetailHelper(m_current_window ? m_current_window->current() : 0);
    } else {
        m_details->hide();
        setItemDetailHelper(0);
    }
}

void FrameWork::setItemDetailHelper(Document::Item *docitem)
{
    if (m_details) {
        if (!docitem)
            m_details->setItem(0, 0);
        else if (m_details->isVisible())
            m_details->setItem(docitem->item(), docitem->color());
    }
}

#include "framework.moc"
