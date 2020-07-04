/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QShortcut>
#include <QDockWidget>
#include <QSizeGrip>
#include <QLineEdit>
#include <QFont>
#if defined(Q_OS_WINDOWS)
#  include <QWinTaskbarButton>
#  include <QWinTaskbarProgress>
#endif

#include "application.h"
#include "messagebox.h"
#include "window.h"
#include "document.h"
#include "config.h"
#include "currency.h"
#include "progresscircle.h"
#include "undo.h"
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


enum ProgressItem {
    PGI_PriceGuide,
    PGI_Picture
};

enum {
    NeedLotId = 1,
    NeedInventory = 2,
    NeedSubCondition = 4,
    NeedNetwork = 8,
    NeedModification = 16,
    NeedDocument = 32
};


class NoFrameStatusBar : public QStatusBar
{
public:
    NoFrameStatusBar(QWidget *parent = nullptr)
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
    void paintEvent(QPaintEvent *) override;
};

void NoFrameStatusBar::paintEvent(QPaintEvent *)
{
    // nearly the same as QStatusBar::paintEvent(), minus those ugly frames
    QString msg = currentMessage();

    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelStatusBar, &opt, &p, this);

    if (!msg.isEmpty()) {
        p.setPen(palette().windowText().color());
        QRect msgr = rect().adjusted(6, 0, -6, 0);
        p.drawText(msgr, Qt::AlignLeading | Qt::AlignVCenter | Qt::TextSingleLine, msg);
    } else {
#ifdef Q_OS_MACX
        QColor lineColor(112, 112, 112);
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


class RecentMenu : public QMenu
{
    Q_OBJECT
public:
    RecentMenu(FrameWork *fw, QWidget *parent)
        : QMenu(parent), m_fw(fw)
    {
        connect(this, &QMenu::aboutToShow,
                this, &RecentMenu::buildMenu);
        connect(this, &QMenu::triggered,
                this, &RecentMenu::openRecentAction);
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

class FancyDockTitleBar : public QLabel
{
public:
    FancyDockTitleBar(QDockWidget *parent)
        : QLabel(parent)
        , m_dock(parent)
    {
        if (m_dock) {
            setText(m_dock->windowTitle());
            connect(m_dock, &QDockWidget::windowTitleChanged,
                    this, &QLabel::setText);
        }
        setAutoFillBackground(true);
        setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

        QPalette p = palette();
        QLinearGradient g(0, 0, 1, 0.5);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        g.setCoordinateMode(QGradient::ObjectMode); // TODO5
#endif
        g.setStops({ { 0, p.color(QPalette::Highlight) },
                     { .65, Utility::gradientColor(p.color(QPalette::Highlight), p.color(QPalette::Window), 0.5) },
                     { 1, p.color(QPalette::Window) } });
        p.setBrush(QPalette::Window, g);
        p.setColor(QPalette::WindowText, p.color(QPalette::HighlightedText));
        setPalette(p);
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    QDockWidget *m_dock;
};

void FancyDockTitleBar::mousePressEvent(QMouseEvent *ev)
{
    if (ev)
        ev->ignore();
}

void FancyDockTitleBar::mouseMoveEvent(QMouseEvent *ev)
{
    if (ev)
        ev->ignore();
}

void FancyDockTitleBar::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev)
        ev->ignore();
}


FrameWork *FrameWork::s_inst = nullptr;

FrameWork *FrameWork::inst()
{
    if (!s_inst)
        (new FrameWork())->setAttribute(Qt::WA_DeleteOnClose);

    return s_inst;
}



FrameWork::FrameWork(QWidget *parent)
    : QMainWindow(parent)
{
    s_inst = this;

    m_running = false;
    m_filter = nullptr;
    m_progress = nullptr;

    setUnifiedTitleAndToolBarOnMac(true);
    setDocumentMode(true);
    setAcceptDrops(true);

    m_undogroup = new UndoGroup(this);

    connect(Application::inst(), &Application::openDocument,
            this, &FrameWork::openDocument);

    m_recent_files = Config::inst()->value("/Files/Recent").toStringList();
    while (m_recent_files.count() > MaxRecentFiles)
        m_recent_files.pop_back();

    m_current_window = nullptr;

    m_workspace = new Workspace(this);
    connect(m_workspace, &Workspace::windowActivated,
            this, &FrameWork::connectWindow);

    setCentralWidget(m_workspace);

    m_task_info = new TaskInfoWidget(nullptr);
    m_task_info->setObjectName(QLatin1String("TaskInfo"));
    addDockWidget(Qt::LeftDockWidgetArea, createDock(m_task_info));

    m_task_appears = new TaskAppearsInWidget(nullptr);
    m_task_appears->setObjectName(QLatin1String("TaskAppears"));
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_appears), Qt::Vertical);

    m_task_priceguide = new TaskPriceGuideWidget(nullptr);
    m_task_priceguide->setObjectName(QLatin1String("TaskPriceGuide"));
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_priceguide), Qt::Vertical);

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName(QLatin1String("toolbar"));
    m_toolbar->setMovable(false);

    auto setIconSizeLambda = [this](const QSize &iconSize) {
        if (iconSize.isNull()) {
            int s = style()->pixelMetric(QStyle::PM_ToolBarIconSize, nullptr, this);
            m_toolbar->setIconSize(QSize(s, s));
        } else {
            m_toolbar->setIconSize(iconSize);
        }
    };
    connect(Config::inst(), &Config::iconSizeChanged, this, setIconSizeLambda);
    setIconSizeLambda(Config::inst()->iconSize());

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
//#if !defined(Q_OS_MACOS)
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
        << "view_docks"
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

    connect(Application::inst(), &Application::onlineStateChanged,
            this, &FrameWork::onlineStateChanged);
    onlineStateChanged(Application::inst()->isOnline());

    connect(Config::inst(), &Config::measurementSystemChanged,
            this, &FrameWork::statisticsUpdate);

    findAction("view_show_input_errors")->setChecked(Config::inst()->showInputErrors());

    connect(BrickLink::core(), &BrickLink::Core::transferJobProgress,
            this, &FrameWork::transferJobProgressUpdate);

    connect(m_undogroup, &QUndoGroup::cleanChanged,
            this, &FrameWork::modificationUpdate);

    bool dbok = BrickLink::core()->readDatabase();

    if (!dbok) {
        if (MessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />Should these files be updated now?"), MessageBox::Yes | MessageBox::No) == MessageBox::Yes)
            dbok = updateDatabase();
    }
    if (dbok)
        Application::inst()->enableEmitOpenDocument();
    else
        MessageBox::warning(this, tr("Could not load the BrickLink database files.<br /><br />The program is not functional without these files."));

    m_add_dialog = nullptr;
    //createAddItemDialog();

    m_running = true;

    updateActions();    // init enabled/disabled status of document actions
    connectWindow(nullptr);

    // we need to show now, since most X11 window managers and macOS with
    // unified-toolbar look won't get the position right otherwise
    // plus, on Windows, we need the window handle for the progress indicator
    show();

#if defined(Q_OS_WINDOWS)
    auto winTaskbarButton = new QWinTaskbarButton(this);
    winTaskbarButton->setWindow(windowHandle());

    connect(BrickLink::core(), &BrickLink::Core::transferJobProgress, this, [winTaskbarButton](int p, int t) {
        QWinTaskbarProgress *progress = winTaskbarButton->progress();
        if (p == t) {
            progress->reset();
        } else {
            if (progress->maximum() != t)
                progress->setMaximum(t);
            progress->setValue(p);
        }
        progress->setVisible(p != t);
    });
#endif

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
    }
    if (m_filter) {
        m_filter->setPlaceholderText(tr("Filter"));
        // DocumentProxyModel hasn't received the LanguageChange event yet,
        // so we need to skip this event loop round
        QMetaObject::invokeMethod(this, [this]() {
            if (m_current_window)
                m_filter->setToolTip(m_current_window->filterToolTip());
        }, Qt::QueuedConnection);
    }
    if (m_progress) {
        m_progress->setToolTipTemplates(tr("Offline"),
                                        tr("No outstanding jobs"),
                                        tr("Downloading...<br><b>%p%</b> finished<br>(%v of %m)"));
    }

    translateActions();

    statisticsUpdate();
}


void FrameWork::translateActions()
{
#if defined(Q_OS_MACOS)
    static bool onMac = true;
#else
    static bool onMac = false;
#endif

    struct ActionDefinition {
        const char *m_name;
        QString     m_text;
        QString     m_shortcut;
        QKeySequence::StandardKey m_standardKey;

        ActionDefinition(const char *n, const QString &t, const QString &s)
            : m_name(n), m_text(t), m_shortcut(s), m_standardKey(QKeySequence::UnknownKey) { }
        ActionDefinition(const char *n, const QString &t, QKeySequence::StandardKey k = QKeySequence::UnknownKey)
            : m_name(n), m_text(t), m_standardKey(k) { }
    } *atptr, actiontable [] = {
        { "file",                           tr("&File"),                              },
        { "file_new",                       tr("New", "File|New"),                    QKeySequence::New },
        { "file_open",                      tr("Open..."),                            QKeySequence::Open },
        { "file_open_recent",               tr("Open Recent"),                        },
        { "file_save",                      tr("Save"),                               QKeySequence::Save },
        { "file_saveas",                    tr("Save As..."),                         },
        { "file_print",                     tr("Print..."),                           QKeySequence::Print },
        { "file_print_pdf",                 tr("Print to PDF..."),                    },
        { "file_import",                    tr("Import"),                             },
        { "file_import_bl_inv",             tr("BrickLink Set Inventory..."),         tr("Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "file_import_bl_xml",             tr("BrickLink XML..."),                   tr("Ctrl+I,Ctrl+X", "File|Import BrickLink XML") },
        { "file_import_bl_order",           tr("BrickLink Order..."),                 tr("Ctrl+I,Ctrl+O", "File|Import BrickLink Order") },
        { "file_import_bl_store_inv",       tr("BrickLink Store Inventory..."),       tr("Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory") },
        { "file_import_bl_cart",            tr("BrickLink Shopping Cart..."),         tr("Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart") },
        { "file_import_ldraw_model",        tr("LDraw Model..."),                     tr("Ctrl+I,Ctrl+L", "File|Import LDraw Model") },
        { "file_export",                    tr("Export"),                             },
        { "file_export_bl_xml",             tr("BrickLink XML..."),                         tr("Ctrl+E,Ctrl+X", "File|Import BrickLink XML") },
        { "file_export_bl_xml_clip",        tr("BrickLink Mass-Upload XML to Clipboard"),   tr("Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload") },
        { "file_export_bl_update_clip",     tr("BrickLink Mass-Update XML to Clipboard"),   tr("Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update") },
        { "file_export_bl_invreq_clip",     tr("BrickLink Set Inventory XML to Clipboard"), tr("Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "file_export_bl_wantedlist_clip", tr("BrickLink Wanted List XML to Clipboard"),   tr("Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List") },
        { "file_close",                     tr("Close"),                              QKeySequence::Close },
        { "file_exit",                      tr("Exit"),                               QKeySequence::Quit },
        { "edit",                           tr("&Edit"),                              },
        { "edit_undo",                      nullptr,                                  QKeySequence::Undo },
        { "edit_redo",                      nullptr,                                  QKeySequence::Redo },
        { "edit_cut",                       tr("Cut"),                                QKeySequence::Cut },
        { "edit_copy",                      tr("Copy"),                               QKeySequence::Copy },
        { "edit_paste",                     tr("Paste"),                              QKeySequence::Paste },
        { "edit_delete",                    tr("Delete"),                             onMac ? tr("Backspace", "Edit|Delete (Mac)") : tr("Delete", "Edit|Delete (Win,Unix)") },
        { "edit_additems",                  tr("Add Items..."),                       tr("Insert", "Edit|AddItems") },
        { "edit_subtractitems",             tr("Subtract Items..."),                  },
        { "edit_mergeitems",                tr("Consolidate Items..."),               },
        { "edit_partoutitems",              tr("Part out Item..."),                   },
        { "edit_setmatch",                  tr("Match Items against Set Inventories...") },
        { "edit_reset_diffs",               tr("Reset Differences"),                  },
        { "edit_copyremarks",               tr("Copy Remarks from Document..."),      },
        { "edit_select_all",                tr("Select All"),                         QKeySequence::SelectAll },
        { "edit_select_none",               tr("Select None"),                        QKeySequence::Deselect },
        { "view",                           tr("&View"),                              },
        { "view_toolbar",                   tr("View Toolbar"),                       },
        { "view_docks",                     tr("View Info Docks"),                    },
        { "view_statusbar",                 tr("View Statusbar"),                     },
        { "view_fullscreen",                tr("Full Screen"),                        QKeySequence::FullScreen },
        { "view_show_input_errors",         tr("Show Input Errors"),                  },
        { "view_difference_mode",           tr("Difference Mode"),                    },
        { "view_save_default_col",          tr("Save Column Layout as Default"),      },
        { "extras",                         tr("E&xtras"),                            },
        { "extras_update_database",         tr("Update Database"),                    },
        { "extras_configure",               tr("Configure..."),                       },
        { "window",                         tr("&Windows"),                           },
        { "help",                           tr("&Help"),                              },
//        { "help_whatsthis",                 tr("What's this?"),                       tr("Shift+F1", "Help|WhatsThis") },
        { "help_about",                     tr("About..."),                           },
        { "help_updates",                   tr("Check for Program Updates..."),       },
        { "edit_status",                    tr("Status"),                             },
        { "edit_status_include",            tr("Include"),                            },
        { "edit_status_exclude",            tr("Exclude"),                            },
        { "edit_status_extra",              tr("Extra"),                              },
        { "edit_status_toggle",             tr("Toggle Include/Exclude"),             },
        { "edit_cond",                      tr("Condition"),                          },
        { "edit_cond_new",                  tr("New", "Cond|New"),                    },
        { "edit_cond_used",                 tr("Used"),                               },
        { "edit_cond_toggle",               tr("Toggle New/Used"),                    },
        { "edit_subcond_none",              tr("None", "SubCond|None"),               },
        { "edit_subcond_sealed",            tr("Sealed", "SubCond|Sealed"),           },
        { "edit_subcond_complete",          tr("Complete", "SubCond|Complete"),       },
        { "edit_subcond_incomplete",        tr("Incomplete", "SubCond|Incomplete"),   },
        { "edit_color",                     tr("Color..."),                           },
        { "edit_qty",                       tr("Quantity"),                           },
        { "edit_qty_set",                   tr("Set..."),                             },
        { "edit_qty_multiply",              tr("Multiply..."),                        tr("Ctrl+*", "Edit|Quantity|Multiply") },
        { "edit_qty_divide",                tr("Divide..."),                          tr("Ctrl+/", "Edit|Quantity|Divide") },
        { "edit_price",                     tr("Price"),                              },
        { "edit_price_round",               tr("Round to 2 Decimal Places"),          },
        { "edit_price_set",                 tr("Set..."),                             },
        { "edit_price_to_priceguide",       tr("Set to Price Guide..."),              tr("Ctrl+G", "Edit|Price|Set to PriceGuide") },
        { "edit_price_inc_dec",             tr("Inc- or Decrease..."),                tr("Ctrl++", "Edit|Price|Inc/Dec") },
        { "edit_bulk",                      tr("Bulk Quantity..."),                   },
        { "edit_sale",                      tr("Sale..."),                            tr("Ctrl+%", "Edit|Sale") },
        { "edit_comment",                   tr("Comment"),                            },
        { "edit_comment_set",               tr("Set..."),                             },
        { "edit_comment_add",               tr("Add to..."),                          },
        { "edit_comment_rem",               tr("Remove from..."),                     },
        { "edit_remark",                    tr("Remark"),                             },
        { "edit_remark_set",                tr("Set..."),                             },
        { "edit_remark_add",                tr("Add to..."),                          },
        { "edit_remark_rem",                tr("Remove from..."),                     },
        { "edit_retain",                    tr("Retain in Inventory"),                },
        { "edit_retain_yes",                tr("Yes"),                                },
        { "edit_retain_no",                 tr("No"),                                 },
        { "edit_retain_toggle",             tr("Toggle Yes/No"),                      },
        { "edit_stockroom",                 tr("Stockroom Item"),                     },
        { "edit_stockroom_no",              tr("No"),                                 },
        { "edit_stockroom_a",               tr("A"),                                  },
        { "edit_stockroom_b",               tr("B"),                                  },
        { "edit_stockroom_c",               tr("C"),                                  },
        { "edit_reserved",                  tr("Reserved for..."),                    },
        { "edit_bl_catalog",                tr("Show BrickLink Catalog Info..."),     },
        { "edit_bl_priceguide",             tr("Show BrickLink Price Guide Info..."), },
        { "edit_bl_lotsforsale",            tr("Show Lots for Sale on BrickLink..."), },
        { "edit_bl_myinventory",            tr("Show in my Store on BrickLink..."),   },

        { nullptr, nullptr }
    };

    for (atptr = actiontable; atptr->m_name; atptr++) {
        if (QAction *a = findAction(atptr->m_name)) {
            if (!atptr->m_text.isNull())
                a->setText(atptr->m_text);
            if (atptr->m_standardKey != QKeySequence::UnknownKey) {

                a->setShortcuts(atptr->m_standardKey);
            } else if (!atptr->m_shortcut.isNull()) {
                a->setShortcuts({ QKeySequence(atptr->m_shortcut) });
            }
            if (!a->shortcut().isEmpty()) {
                a->setToolTip(QString("%1 <span style=\"color: gray; font-size: small\">%2</span>")
                              .arg(a->text()).arg(a->shortcut().toString(QKeySequence::NativeText)));
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
    s_inst = nullptr;
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

void FrameWork::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QMainWindow::changeEvent(e);
}

QAction *FrameWork::findAction(const char *name)
{
    return !name || !name[0] ? nullptr : static_cast <QAction *>(findChild<QAction *>(QLatin1String(name)));
}

QDockWidget *FrameWork::createDock(QWidget *widget)
{
    QDockWidget *dock = new QDockWidget(QString(), this);
    dock->setObjectName(QLatin1String("Dock-") + widget->objectName());
    dock->setFeatures(QDockWidget::DockWidgetClosable);
    dock->setTitleBarWidget(new FancyDockTitleBar(dock));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setWidget(widget);
    m_dock_widgets.append(dock);
    return dock;
}

void FrameWork::createStatusBar()
{
    NoFrameStatusBar *st = new NoFrameStatusBar(this);
    setStatusBar(st);

    int margin = 2 * st->fontMetrics().horizontalAdvance(QLatin1Char(' '));

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

    connect(m_st_currency, &QToolButton::triggered,
            this, &FrameWork::changeDocumentCurrency);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &FrameWork::updateCurrencyRates);
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
        return nullptr;

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

                m_filter = new QLineEdit(this);
                m_filter->setClearButtonEnabled(true);
                m_filter->addAction(new QAction(QIcon(":/images/filter.png"), QString(), this),
                                    QLineEdit::LeadingPosition);
                t->addWidget(m_filter);
            } else if (an == "widget_progress") {
                if (m_progress) {
                    qWarning("Only one progress widget can be added to toolbars");
                    continue;
                }
                m_progress = new ProgressCircle();
                m_progress->setIcon(QIcon(":/images/brickstore.png"));
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

inline static QMenu *newQMenu(QWidget *parent, const char *name, quint32 flags = 0)
{
    QMenu *m = new QMenu(parent);
    m->menuAction()->setObjectName(QLatin1String(name));
    if (flags)
        m->menuAction()->setProperty("bsFlags", flags);
    return m;
}

static inline quint32 NeedSelection(quint8 minSel, quint8 maxSel = 0)
{
    return NeedDocument | (quint32(minSel) << 24) | (quint32(maxSel) << 16);
}

template <typename Func>
inline static QAction *newQAction(QObject *parent, const char *name, quint32 flags, bool toggle,
                                  const typename QtPrivate::FunctionPointer<Func>::Object *receiver,
                                  Func slot)
{
    QAction *a = new QAction(parent);
    a->setObjectName(QLatin1String(name));
    if (flags)
        a->setProperty("bsFlags", flags);
    if (toggle)
        a->setCheckable(true);
    if (receiver && slot) {
        if (toggle)
            QObject::connect(a, &QAction::toggled, receiver, slot);
        else
            QObject::connect(a, &QAction::triggered, receiver, slot);
//        QObject::connect(a, toggle ? SIGNAL(toggled(bool)) : SIGNAL(triggered()), receiver, slot);
    }
    return a;
}

inline static QAction *newQAction(QObject *parent, const char *name, quint32 flags = 0, bool toggle = false)
{
    return newQAction(parent, name, flags, toggle, static_cast<QObject *>(nullptr), &QObject::objectName);
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

    a = newQAction(this, "file_new", 0, false, this, &FrameWork::fileNew);
    a = newQAction(this, "file_open", 0, false, this, &FrameWork::fileOpen);

    auto rm = new RecentMenu(this, this);
    rm->menuAction()->setObjectName("file_open_recent");
    connect(rm, &RecentMenu::openRecent,
            this, &FrameWork::fileOpenRecent);

    (void) newQAction(this, "file_save", NeedDocument | NeedModification);
    (void) newQAction(this, "file_saveas", NeedDocument);
    (void) newQAction(this, "file_print", NeedDocument);
    (void) newQAction(this, "file_print_pdf", NeedDocument);

    m = newQMenu(this, "file_import");
    m->addAction(newQAction(this, "file_import_bl_inv", 0, false, this, QOverload<>::of(&FrameWork::fileImportBrickLinkInventory)));
    m->addAction(newQAction(this, "file_import_bl_xml", 0, false, this, &FrameWork::fileImportBrickLinkXML));
    m->addAction(newQAction(this, "file_import_bl_order", NeedNetwork, false, this, &FrameWork::fileImportBrickLinkOrder));
    m->addAction(newQAction(this, "file_import_bl_store_inv", NeedNetwork, false, this, &FrameWork::fileImportBrickLinkStore));
    m->addAction(newQAction(this, "file_import_bl_cart", NeedNetwork, false, this, &FrameWork::fileImportBrickLinkCart));
    m->addAction(newQAction(this, "file_import_ldraw_model", 0, false, this, &FrameWork::fileImportLDrawModel));

    m = newQMenu(this, "file_export");
    m->addAction(newQAction(this, "file_export_bl_xml", NeedDocument));
    m->addAction(newQAction(this, "file_export_bl_xml_clip", NeedDocument));
    m->addAction(newQAction(this, "file_export_bl_update_clip", NeedDocument));
    m->addAction(newQAction(this, "file_export_bl_invreq_clip", NeedDocument));
    m->addAction(newQAction(this, "file_export_bl_wantedlist_clip", NeedDocument));

    (void) newQAction(this, "file_close", NeedDocument);

    a = newQAction(this, "file_exit", 0, false, this, &FrameWork::close);
    a->setMenuRole(QAction::QuitRole);

    a = m_undogroup->createUndoAction(this);
    a->setObjectName("edit_undo");
    a = m_undogroup->createRedoAction(this);
    a->setObjectName("edit_redo");

    a = newQAction(this, "edit_cut", NeedSelection(1));
    connect(new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete), this, nullptr, nullptr, Qt::WindowShortcut),
            &QShortcut::activated, a, &QAction::trigger);
    a = newQAction(this, "edit_copy", NeedSelection(1));
    connect(new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Insert), this, nullptr, nullptr, Qt::WindowShortcut),
            &QShortcut::activated, a, &QAction::trigger);
    a = newQAction(this, "edit_paste", NeedDocument);
    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Insert), this, nullptr, nullptr, Qt::WindowShortcut),
            &QShortcut::activated, a, &QAction::trigger);
    (void) newQAction(this, "edit_delete", NeedSelection(1));

    a = newQAction(this, "edit_additems", NeedDocument, false, this, &FrameWork::showAddItemDialog);
    a->setShortcutContext(Qt::ApplicationShortcut);

    (void) newQAction(this, "edit_subtractitems", NeedDocument);
    (void) newQAction(this, "edit_mergeitems", NeedSelection(2));
    (void) newQAction(this, "edit_partoutitems", NeedInventory | NeedSelection(1));
    (void) newQAction(this, "edit_setmatch", NeedSelection(1));
    (void) newQAction(this, "edit_reset_diffs", NeedSelection(1));
    (void) newQAction(this, "edit_copyremarks", NeedDocument);
    (void) newQAction(this, "edit_select_all", NeedDocument);
    (void) newQAction(this, "edit_select_none", NeedDocument);

    m = newQMenu(this, "edit_status", NeedSelection(1));
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_status_include", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_status_exclude", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_status_extra",   NeedSelection(1), true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_status_toggle", NeedSelection(1)));

    m = newQMenu(this, "edit_cond", NeedSelection(1));
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_cond_new", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_cond_used", NeedSelection(1), true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_cond_toggle", NeedSelection(1)));
    m->addSeparator();
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_subcond_none", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_sealed", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_complete", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_incomplete", NeedSelection(1) | NeedSubCondition, true));

    (void) newQAction(this, "edit_color", NeedSelection(1));

    m = newQMenu(this, "edit_qty", NeedSelection(1));
    m->addAction(newQAction(this, "edit_qty_set", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_qty_multiply", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_qty_divide", NeedSelection(1)));

    m = newQMenu(this, "edit_price", NeedSelection(1));
    m->addAction(newQAction(this, "edit_price_set", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_price_inc_dec", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_price_to_priceguide", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_price_round", NeedSelection(1)));

    (void) newQAction(this, "edit_bulk", NeedSelection(1));
    (void) newQAction(this, "edit_sale", NeedSelection(1));

    m = newQMenu(this, "edit_comment", NeedSelection(1));
    m->addAction(newQAction(this, "edit_comment_set", NeedSelection(1)));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_comment_add", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_comment_rem", NeedSelection(1)));

    m = newQMenu(this, "edit_remark", NeedSelection(1));
    m->addAction(newQAction(this, "edit_remark_set", NeedSelection(1)));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_remark_add", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_remark_rem", NeedSelection(1)));

    m = newQMenu(this, "edit_retain", NeedSelection(1));
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_retain_yes", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_retain_no", NeedSelection(1), true));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_retain_toggle", NeedSelection(1)));

    m = newQMenu(this, "edit_stockroom", NeedSelection(1));
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_stockroom_no", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_stockroom_a", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_stockroom_b", NeedSelection(1), true));
    m->addAction(newQAction(g, "edit_stockroom_c", NeedSelection(1), true));

    (void) newQAction(this, "edit_reserved", NeedSelection(1));

    (void) newQAction(this, "edit_bl_catalog", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "edit_bl_priceguide", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "edit_bl_lotsforsale", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "edit_bl_myinventory", NeedSelection(1, 1) | NeedLotId | NeedNetwork);

    (void) newQAction(this, "view_fullscreen", 0, true, this, &FrameWork::viewFullScreen);

    m_toolbar->toggleViewAction()->setObjectName("view_toolbar");
    m = newQMenu(this, "view_docks");
    foreach (QDockWidget *dock, m_dock_widgets)
        m->addAction(dock->toggleViewAction());

    (void) newQAction(this, "view_statusbar", 0, true, this, &FrameWork::viewStatusBar);
    (void) newQAction(this, "view_show_input_errors", 0, true, Config::inst(), &Config::setShowInputErrors);
    (void) newQAction(this, "view_difference_mode", 0, true);
    (void) newQAction(this, "view_save_default_col");
    (void) newQAction(this, "extras_update_database", NeedNetwork, false, this, &FrameWork::updateDatabase);

    a = newQAction(this, "extras_configure", 0, false, this, QOverload<>::of(&FrameWork::configure));
    a->setMenuRole(QAction::PreferencesRole);

    //(void) newQAction(this, "help_whatsthis", 0, false, this, &FrameWork::whatsThis);

    a = newQAction(this, "help_about", 0, false, Application::inst(), &Application::about);
    a->setMenuRole(QAction::AboutRole);

    a = newQAction(this, "help_updates", NeedNetwork, false, Application::inst(), &Application::checkForUpdates);
    a->setMenuRole(QAction::ApplicationSpecificRole);

    // set all icons that have a pixmap corresponding to name()

    QList<QAction *> alist = findChildren<QAction *>();
    foreach (QAction *a, alist) {
        if (!a->objectName().isEmpty()) {
            QString path = QLatin1String(":/images/") + a->objectName() + QLatin1String(".png");

            if (QFile::exists(path))
                a->setIcon(QIcon(path));
        }
    }
}

void FrameWork::viewStatusBar(bool b)
{
    statusBar()->setVisible(b);
}

void FrameWork::viewToolBar(bool b)
{
    m_toolbar->setVisible(b);
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

void FrameWork::fileImportBrickLinkInventory()
{
    fileImportBrickLinkInventory(nullptr);
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
        ok = ok && createWindow(doc);
    return ok;
}



bool FrameWork::createWindow(Document *doc)
{
    if (!doc)
        return false;

    Window *window = nullptr;
    foreach(QWidget *w, m_workspace->windowList()) {
        Window *ww = qobject_cast<Window *>(w);

        if (ww && ww->document() == doc) {
            window = ww;
            break;
        }
    }
    if (!window) {
        m_undogroup->addStack(doc->undoStack());
        window = new Window(doc, nullptr);
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

        Transfer trans;
        ProgressDialog d(&trans, this);
        UpdateDatabase update(&d);

        return d.exec();
    }
    return false;
}

void FrameWork::connectAllActions(bool do_connect, Window *window)
{
    QMetaObject mo = Window::staticMetaObject;
    QObjectList list = findChildren<QObject *>();

    for (int i = 0; i < mo.methodCount(); ++i) {
        QByteArray slot = mo.method(i).methodSignature();
        if (slot.isEmpty() || !slot.startsWith("on_"))
            continue;
        bool foundIt = false;

        for (int j = 0; j < list.count(); ++j) {
            QObject *co = list.at(j);
            QByteArray objName = co->objectName().toLatin1();
            uint len = uint(objName.length());
            if (!len || qstrncmp(slot.constData() + 3, objName.data(), len) || slot[len+3] != '_')
                continue;
            const QMetaObject *smo = co->metaObject();
            int sigIndex = smo->indexOfMethod(slot.constData() + len + 4);
            if (sigIndex < 0) { // search for compatible signals
                uint slotlen = qstrlen(slot.constData() + len + 4) - 1;
                for (int k = 0; k < co->metaObject()->methodCount(); ++k) {
                    if (smo->method(k).methodType() != QMetaMethod::Signal)
                        continue;

                    if (!qstrncmp(smo->method(k).methodSignature().constData(), slot.constData() + len + 4, slotlen)) {
                        sigIndex = k;
                        break;
                    }
                }
            }
            if (sigIndex < 0)
                continue;

            if (do_connect && window && QMetaObject::connect(co, sigIndex, window, i)) {
                foundIt = true;
            } else if (!do_connect && window) {
                // ignore errors on disconnect
                QMetaObject::disconnect(co, sigIndex, window, i);
                foundIt = true;
            }

            if (foundIt)
                break;
        }
        if (foundIt) {
            // we found our slot, now skip all overloads
            while (mo.method(i + 1).attributes() & QMetaMethod::Cloned)
                  ++i;
        } else if (window && (!(mo.method(i).attributes() & QMetaMethod::Cloned))) {
            qWarning("FrameWork::connectAllActions: No matching signal for %s", slot.constData());
        }
    }
}

void FrameWork::connectWindow(QWidget *w)
{
    if (w && w == m_current_window)
        return;

    if (m_current_window) {
        Document *doc = m_current_window->document();

        connectAllActions(false, m_current_window);

        disconnect(m_current_window.data(), &Window::windowTitleChanged,
                   this, &FrameWork::titleUpdate);
        disconnect(doc, &Document::statisticsChanged,
                   this, &FrameWork::statisticsUpdate);
        disconnect(m_current_window.data(), &Window::selectionChanged,
                   this, &FrameWork::selectionUpdate);
        if (m_filter) {
            disconnect(m_filter, &QLineEdit::textChanged,
                       m_current_window.data(), &Window::setFilter);
            m_filter->setText(QString());
            m_filter->setToolTip(QString());
        }
        if (m_details) {
            disconnect(m_current_window.data(), &Window::currentChanged,
                       this, &FrameWork::setItemDetailHelper);
            setItemDetailHelper(nullptr);
        }
        m_undogroup->setActiveStack(nullptr);

        m_current_window = nullptr;
    }

    if (Window *window = qobject_cast<Window *>(w)) {
        Document *doc = window->document();

        connectAllActions(true, window);

        connect(window, &Window::windowTitleChanged,
                this, &FrameWork::titleUpdate);
        connect(doc, &Document::statisticsChanged,
                this, &FrameWork::statisticsUpdate);
        connect(window, &Window::selectionChanged,
                this, &FrameWork::selectionUpdate);
        if (m_filter) {
            m_filter->setText(window->filter());
            m_filter->setToolTip(window->filterToolTip());
            connect(m_filter, &QLineEdit::textChanged,
                    window, &Window::setFilter);
        }
        if (m_details) {
            setItemDetailHelper(window->current());
            connect(window, &Window::currentChanged,
                    this, &FrameWork::setItemDetailHelper);
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

void FrameWork::updateActions(const Document::ItemList &selection)
{
    int cnt = selection.count();
    bool isOnline = Application::inst()->isOnline();

    foreach (QAction *a, findChildren<QAction *>()) {
        quint32 flags = a->property("bsFlags").toUInt();

        if (!flags)
            continue;

        bool b = true;

        if (flags & NeedNetwork)
            b = b && isOnline;

        if (flags & NeedModification) {
            b = b && m_current_window && m_current_window->document()
                    && m_current_window->document()->isModified();
        }

        if (flags & NeedDocument) {
            b = b && m_current_window;

            quint8 minSelection = flags >> 24;
            quint8 maxSelection = (flags >> 16) & 0xff;

            if (minSelection)
                b = b && (cnt >= minSelection);
            if (maxSelection)
                b = b && (cnt <= maxSelection);
        }

        foreach (Document::Item *item, selection) {
            if (flags & NeedLotId)
                b = b && (item->lotId() != 0);
            if (flags & NeedInventory)
                b = b && (item->item() && item->item()->hasInventory());
            if (flags & NeedSubCondition)
                b = b && (item->item() && item->itemType() && item->item()->itemType()->hasSubConditions());

            if (!b)
                break;
        }
        a->setEnabled(b);
    }
}

void FrameWork::selectionUpdate(const Document::ItemList &selection)
{
    updateActions(selection);

    int cnt        = selection.count();
    int status     = -1;
    int condition  = -1;
    int scondition = -1;
    int retain     = -1;
    int stockroom  = -1;

    if (cnt) {
        status     = int(selection.front()->status());
        condition  = int(selection.front()->condition());
        scondition = int(selection.front()->subCondition());
        retain     = selection.front()->retain()    ? 1 : 0;
        stockroom  = int(selection.front()->stockroom());

        foreach (Document::Item *item, selection) {
            if ((status >= 0) && (status != int(item->status())))
                status = -1;
            if ((condition >= 0) && (condition != int(item->condition())))
                condition = -1;
            if ((scondition >= 0) && (scondition != int(item->subCondition())))
                scondition = -1;
            if ((retain >= 0) && (retain != (item->retain() ? 1 : 0)))
                retain = -1;
            if ((stockroom >= 0) && (stockroom != int(item->stockroom())))
                stockroom = -1;
        }
    }
    findAction("edit_status_include")->setChecked(status == int(BrickLink::Status::Include));
    findAction("edit_status_exclude")->setChecked(status == int(BrickLink::Status::Exclude));
    findAction("edit_status_extra")->setChecked(status == int(BrickLink::Status::Extra));

    findAction("edit_cond_new")->setChecked(condition == int(BrickLink::Condition::New));
    findAction("edit_cond_used")->setChecked(condition == int(BrickLink::Condition::Used));

    findAction("edit_subcond_none")->setChecked(scondition == int(BrickLink::SubCondition::None));
    findAction("edit_subcond_sealed")->setChecked(scondition == int(BrickLink::SubCondition::Sealed));
    findAction("edit_subcond_complete")->setChecked(scondition == int(BrickLink::SubCondition::Complete));
    findAction("edit_subcond_incomplete")->setChecked(scondition == int(BrickLink::SubCondition::Incomplete));

    findAction("edit_retain_yes")->setChecked(retain == 1);
    findAction("edit_retain_no")->setChecked(retain == 0);

    findAction("edit_stockroom_no")->setChecked(stockroom == int(BrickLink::Stockroom::None));
    findAction("edit_stockroom_a")->setChecked(stockroom == int(BrickLink::Stockroom::A));
    findAction("edit_stockroom_b")->setChecked(stockroom == int(BrickLink::Stockroom::B));
    findAction("edit_stockroom_c")->setChecked(stockroom == int(BrickLink::Stockroom::C));
}

void FrameWork::statisticsUpdate()
{
    QString lotstr, itmstr, errstr, valstr, wgtstr, ccode;

    if (m_current_window)
    {
        Document::ItemList not_exclude;

        foreach(Document::Item *item, m_current_window->document()->items()) {
            if (item->status() != BrickLink::Status::Exclude)
                not_exclude.append(item);
        }

        Document::Statistics stat = m_current_window->document()->statistics(not_exclude);
        ccode = m_current_window->document()->currencyCode();

        if (!qFuzzyCompare(stat.value(), stat.minValue())) {
            valstr = tr("Value: %1 (min. %2)").
                     arg(Currency::toString(stat.value(), ccode, Currency::NoSymbol)).
                     arg(Currency::toString(stat.minValue(), ccode, Currency::NoSymbol));
        } else {
            valstr = tr("Value: %1").arg(Currency::toString(stat.value(), ccode, Currency::NoSymbol));
        }

        if (qFuzzyCompare(stat.weight(), -DBL_MIN)) {
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
    configure(nullptr);
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

    if (m_current_window)
        updateActions(m_current_window->selection());
    else
        updateActions();
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
    QString name = QDir::toNativeSeparators(QFileInfo(s).absoluteFilePath());

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

        if (m_current_window) {
            connect(m_current_window.data(), &Window::currentChanged,
                    this, &FrameWork::setItemDetailHelper);
        }
    }

    if (!m_details->isVisible()) {
        m_details->show();
        setItemDetailHelper(m_current_window ? m_current_window->current() : nullptr);
    } else {
        m_details->hide();
        setItemDetailHelper(nullptr);
    }
}

void FrameWork::setItemDetailHelper(Document::Item *docitem)
{
    if (m_details) {
        if (!docitem)
            m_details->setItem(nullptr, nullptr);
        else if (m_details->isVisible())
            m_details->setItem(docitem->item(), docitem->color());
    }
}

#include "framework.moc"

#include "moc_framework.cpp"
