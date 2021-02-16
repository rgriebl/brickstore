/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <QCloseEvent>
#include <QMetaObject>
#include <QMetaMethod>
#include <QMenuBar>
#include <QToolBar>
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
#include <QPlainTextEdit>
#include <QFont>
#include <QCommandLinkButton>
#include <QStyle>
#include <QLinearGradient>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringBuilder>
#if defined(Q_OS_WINDOWS)
#  include <QWinTaskbarButton>
#  include <QWinTaskbarProgress>
#endif

#include "application.h"
#include "messagebox.h"
#include "window.h"
#include "document.h"
#include "documentio.h"
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
#include "welcomewidget.h"
#include "aboutdialog.h"
#include "managecolumnlayoutsdialog.h"
#include "framework.h"
#include "stopwatch.h"
#include "importinventorydialog.h"
#include "importorderdialog.h"
#include "importcartdialog.h"
#include "historylineedit.h"

#include "scriptmanager.h"
#include "script.h"
#include "exception.h"

using namespace std::chrono_literals;


enum {
    DockStateVersion = 1 // increase if you change the dock/toolbar setup
};

enum {
    NeedLotId        = 0x0001,
    NeedInventory    = 0x0002,
    NeedSubCondition = 0x0004,
    NeedQuantity     = 0x0008,

    NeedItemMask     = 0x000f,

    NeedDocument     = 0x0010,
    NeedModification = 0x0020,
    NeedItems        = 0x0040,
    NeedNetwork      = 0x0100,

    // the upper 16 bits (0xffff0000) are reserved for NeedSelection()
};


class RecentMenu : public QMenu
{
    Q_OBJECT
public:
    explicit RecentMenu(QWidget *parent)
        : QMenu(parent)
    {
        connect(this, &QMenu::aboutToShow, this, [this]() {
            clear();

            int cnt = 0;
            auto recent =  Config::inst()->recentFiles();
            for (const auto &f : recent) {
                QString s = f;
#if !defined(Q_OS_MACOS)
                if (++cnt < 10)
                    s.prepend(QString("&%1   ").arg(cnt));
#endif
                addAction(s)->setData(f);
            }
            if (!cnt) {
                addAction(tr("No recent files"))->setEnabled(false);
            } else {
                addSeparator();
                addAction(tr("Clear recent files"))->setData(QString());
            }
        });
        connect(this, &QMenu::triggered, this, [this](QAction *a) {
            auto f = a->data().toString();
            if (f.isEmpty())
                emit clearRecent();
            else
                emit openRecent(f);
        });
    }

signals:
    void openRecent(const QString &file);
    void clearRecent();
};

class LoadColumnLayoutMenu : public QMenu
{
    Q_OBJECT
public:
    explicit LoadColumnLayoutMenu(QWidget *parent)
        : QMenu(parent)
    {
        connect(this, &QMenu::aboutToShow, this, [this]() {
            clear();

            addAction(tr("User Default"))->setData(QLatin1String("user-default"));
            addAction(tr("BrickStore Default"))->setData(QLatin1String("default"));
            addAction(tr("Auto Resize Once"))->setData(QLatin1String("auto-resize"));

            auto ids = Config::inst()->columnLayoutIds();
            if (!ids.isEmpty()) {
                addSeparator();

                QMap<int, QString> pos;
                for (const auto &id : ids)
                    pos.insert(Config::inst()->columnLayoutOrder(id), id);

                for (const auto &id : qAsConst(pos))
                    addAction(Config::inst()->columnLayoutName(id))->setData(id);
            }
        });
        connect(this, &QMenu::triggered, this, [this](QAction *a) {
            if (a && !a->data().isNull())
                emit load(a->data().toString());
        });
    }

signals:
    void load(const QString &id);
};


class FancyDockTitleBar : public QLabel
{
public:
    explicit FancyDockTitleBar(QDockWidget *parent)
        : QLabel(parent)
        , m_gradient(0, 0, 1, 0)
        , m_dock(parent)
    {
        if (m_dock) {
            setText(m_dock->windowTitle());
            connect(m_dock, &QDockWidget::windowTitleChanged,
                    this, &QLabel::setText);
        }
        setAutoFillBackground(true);
        setIndent(style()->pixelMetric(QStyle::PM_LayoutLeftMargin));
        m_gradient.setCoordinateMode(QGradient::ObjectMode);

        QFont f = font();
        f.setBold(true);
        setFont(f);
    }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        QPalette p = palette();
        QPalette ivp = qApp->palette("QAbstractItemView");
        QColor high = ivp.color(p.currentColorGroup(), QPalette::Highlight);
        QColor text = ivp.color(p.currentColorGroup(), QPalette::HighlightedText);
        QColor win = QApplication::palette(this).color(QPalette::Window);

        if (m_gradient.stops().count() != 3
                || m_gradient.stops().at(0).second != high
                || m_gradient.stops().at(2).second != win) {
            m_gradient.setStops({ { 0, high },
                         { .6, Utility::gradientColor(high, win, 0.5) },
                         { 1, win } });
            p.setBrush(QPalette::Window, m_gradient);
            p.setColor(QPalette::WindowText, text);
            setPalette(p);
        }
        QLabel::paintEvent(e);
    }

    QSize sizeHint() const override
    {
        QSize s = QLabel::sizeHint();
        s.rheight() += s.height() / 2;
        return s;
    }

    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    QLinearGradient m_gradient;
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

#if defined(QT_NO_OPENGL)
    // QTBUG-39781
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    setDocumentMode(true);
    setAcceptDrops(true);

    m_undogroup = new UndoGroup(this);

    connect(Application::inst(), &Application::openDocument,
            this, &FrameWork::openDocument);

    m_current_window = nullptr;

    m_workspace = new Workspace(this);
    connect(m_workspace, &Workspace::windowActivated,
            this, &FrameWork::connectWindow);

    connect(m_workspace, &Workspace::windowCountChanged,
            this, &FrameWork::windowListChanged);

    connect(m_workspace, &Workspace::welcomeWidgetVisible,
            this, [this]() { connectWindow(nullptr); });

    setCentralWidget(m_workspace);

    // vvv increase DockStateVersion if you change the dock/toolbar setup

    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    m_task_info = new TaskInfoWidget(nullptr);
    m_task_info->setObjectName(QLatin1String("TaskInfo"));
    addDockWidget(Qt::LeftDockWidgetArea, createDock(m_task_info));

    m_task_appears = new TaskAppearsInWidget(nullptr);
    m_task_appears->setObjectName(QLatin1String("TaskAppears"));
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_appears), Qt::Vertical);

    m_task_priceguide = new TaskPriceGuideWidget(nullptr);
    m_task_priceguide->setObjectName(QLatin1String("TaskPriceGuide"));
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_priceguide), Qt::Vertical);

    auto logDock = createDock(Application::inst()->logWidget());
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    logDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, logDock, Qt::Horizontal);

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName(QLatin1String("toolbar"));
    m_toolbar->setMovable(false);

    // ^^^ increase DockStateVersion if you change the dock/toolbar setup

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

    menuBar()->addMenu(createMenu("menu_file", {
                                      "document_new",
                                      "document_open",
                                      "document_open_recent",
                                      "-",
                                      "document_save",
                                      "document_save_as",
                                      "-",
                                      "document_import",
                                      "document_export",
                                      "-",
                                      "document_print",
                                      //#if !defined(Q_OS_MACOS)
                                      "document_print_pdf",
                                      //#endif
                                      "-",
                                      "document_close",
                                      "-",
                                      "application_exit"
                                  }));

    menuBar()->addMenu(createMenu("menu_edit", {
                                      "edit_undo",
                                      "edit_redo",
                                      "-",
                                      "edit_cut",
                                      "edit_copy",
                                      "edit_paste",
                                      "edit_delete",
                                      "-",
                                      "edit_select_all",
                                      "edit_select_none",
                                      "-",
                                      "edit_additems",
                                      "edit_subtractitems",
                                      "edit_mergeitems",
                                      "edit_partoutitems",
                                      //"edit_setmatch",
                                      "-",
                                      "edit_status",
                                      "edit_cond",
                                      "edit_color",
                                      "edit_qty",
                                      "edit_price",
                                      "edit_cost",
                                      "edit_bulk",
                                      "edit_sale",
                                      "edit_comment",
                                      "edit_remark",
                                      "edit_retain",
                                      "edit_stockroom",
                                      "edit_reserved",
                                      "-",
                                      "edit_copyremarks",
                                      "-",
                                      "bricklink_catalog",
                                      "bricklink_priceguide",
                                      "bricklink_lotsforsale",
                                      "bricklink_myinventory"
                                  }));

    menuBar()->addMenu(createMenu("menu_view", {
                                      "view_toolbar",
                                      "view_docks",
                                      "-",
                                      "view_fullscreen",
                                      "-",
                                      "view_simple_mode",
                                      "view_show_input_errors",
                                      "-",
                                      "view_column_layout_save",
                                      "view_column_layout_manage",
                                      "view_column_layout_load"
                                  }));

    menuBar()->addMenu(createMenu("menu_extras", {
                                      "update_database",
                                      "-",
                                      "configure"
                                  }));

    QMenu *m = m_workspace->windowMenu(true, this);
    m->menuAction()->setObjectName(QLatin1String("menu_window"));
    menuBar()->addMenu(m);

    menuBar()->addMenu(createMenu("menu_help", {
                                      "check_for_updates",
                                      "-",
                                      "help_about"
                                  }));

    m_contextmenu = createMenu("menu_context", {
                                   "edit_cut",
                                   "edit_copy",
                                   "edit_paste",
                                   "edit_delete",
                                   "-",
                                   "edit_select_all",
                                   "edit_select_none",
                                   "-",
                                   "edit_filter_from_selection",
                                   "-",
                                   "edit_mergeitems",
                                   "edit_partoutitems",
                                   "-",
                                   "edit_status",
                                   "edit_cond",
                                   "edit_color",
                                   "edit_qty",
                                   "edit_price",
                                   "edit_cost",
                                   "edit_comment",
                                   "edit_remark",
                                   "-",
                                   "bricklink_catalog",
                                   "bricklink_priceguide",
                                   "bricklink_lotsforsale",
                                   "bricklink_myinventory"
                               });

    setupToolBar(m_toolbar, {
                     "document_new",
                     "document_open",
                     "document_save",
                     "-",
                     "document_import",
                     "document_export",
                     "-",
                     "edit_undo",
                     "edit_redo",
                     "-",
                     "edit_cut",
                     "edit_copy",
                     "edit_paste",
                     "edit_delete",
                     "-",
                     "edit_additems",
                     "edit_subtractitems",
                     "edit_mergeitems",
                     "edit_partoutitems",
                     "-",
                     "edit_price_to_priceguide",
                     "edit_price_inc_dec",
                     "-",
                     "view_column_layout_load",
                     "|",
                     "widget_filter",
                     "|",
                     "widget_progress",
                     "|"
                 });

    addToolBar(m_toolbar);

    m_workspace->setWelcomeWidget(new WelcomeWidget());

    languageChange();


    connect(Application::inst(), &Application::onlineStateChanged,
            this, &FrameWork::onlineStateChanged);
    onlineStateChanged(Application::inst()->isOnline());

    findAction("view_simple_mode")->setChecked(Config::inst()->simpleMode());
    findAction("view_show_input_errors")->setChecked(Config::inst()->showInputErrors());

    connect(BrickLink::core(), &BrickLink::Core::transferJobProgress,
            this, &FrameWork::transferJobProgressUpdate);

    bool dbok = BrickLink::core()->readDatabase();

    if (!dbok || BrickLink::core()->isDatabaseUpdateNeeded())
        dbok = updateDatabase(true /* forceSync */);

    if (dbok) {
        Application::inst()->enableEmitOpenDocument();
    } else {
        MessageBox::warning(nullptr, { }, tr("Could not load the BrickLink database files.<br /><br />The program is not functional without these files."));
    }

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
    restoreGeometry(ba);

    ba = Config::inst()->value(QLatin1String("/MainWindow/Layout/State")).toByteArray();
    if (ba.isEmpty() || !restoreState(ba, DockStateVersion))
        m_toolbar->show();

    if (m_filter) {
        ba = Config::inst()->value(QLatin1String("/MainWindow/Filter")).toByteArray();
        if (!ba.isEmpty()) {
            m_filter->restoreState(ba);
            m_filter->clear();
        }
        m_filter->setEnabled(false);
    }


    findAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

    Currency::inst()->updateRates();
    auto *currencyUpdateTimer = new QTimer(this);
    currencyUpdateTimer->setInterval(4h); // 4 hours
    currencyUpdateTimer->start();
    connect(currencyUpdateTimer, &QTimer::timeout,
            Currency::inst(), []() { Currency::inst()->updateRates(true /*silent*/); });

    setupScripts();

    checkForUpdates(true /* silent */);

    restoreWindowsFromAutosave();
}

void FrameWork::setupScripts()
{
    if (!ScriptManager::inst()->initialize(BrickLink::core())) {
        MessageBox::warning(nullptr, { }, tr("Could not initialize the JavaScript scripting environment."));
        return;
    }

    const auto scripts = ScriptManager::inst()->extensionScripts();

    auto extrasMenu = findChild<QAction *>("menu_extras")->menu();
    auto contextMenu = findChild<QAction *>("menu_context")->menu();

    bool addedToExtras = false;
    bool addedToContext = false;

    for (auto script : scripts) {
        const auto extensionActions = script->extensionActions();
        for (auto extensionAction : extensionActions) {
            bool c = (extensionAction->location() == ExtensionScriptAction::Location::ContextMenu);
            QMenu *menu = c ? contextMenu : extrasMenu;
            bool &addedTo = c ? addedToContext : addedToExtras;

            if (!addedTo) {
                addedTo = true;
                menu->addSeparator();
            }
            menu->addAction(extensionAction->text(), extensionAction,
                            [extensionAction]() {
                try {
                    extensionAction->executeAction();
                } catch (const Exception &e) {
                    MessageBox::warning(nullptr, { }, e.error());
                }
            });
        }
    }
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
        if (name == QLatin1String("Dock-LogWidget"))
            dock->setWindowTitle(tr("Error Log"));
    }
    if (m_filter) {
        m_filter->setPlaceholderText(tr("Filter"));
        // DocumentProxyModel hasn't received the LanguageChange event yet,
        // so we need to skip this event loop round
        QMetaObject::invokeMethod(this, [this]() {
            QString tt = findAction("edit_filter_focus")->toolTip();
            if (m_current_window)
                tt.append(m_current_window->document()->filterToolTip());
            m_filter->setToolTip(tt);
        }, Qt::QueuedConnection);
    }
    if (m_progress) {
        m_progress->setToolTipTemplates(tr("Offline"),
                                        tr("No outstanding jobs"),
                                        tr("Downloading...<br><b>%p%</b> finished<br>(%v of %m)"));
    }

    translateActions();
}


void FrameWork::translateActions()
{
    struct ActionDefinition {
        const char *name;
        QString     text;
        QString     shortcut;
        QKeySequence::StandardKey standardKey;

        ActionDefinition(const char *n, const QString &t, const QString &s)
            : name(n), text(t), shortcut(s), standardKey(QKeySequence::UnknownKey) { }
        ActionDefinition(const char *n, const QString &t, QKeySequence::StandardKey k = QKeySequence::UnknownKey)
            : name(n), text(t), standardKey(k) { }
    };

    // recreating this whole table on each invocation is far from ideal, but QT_TR_NOOP doesn't
    // support the disambiguate parameter, which we need (e.g. New)

    const ActionDefinition actiontable[] = {
        { "menu_file",                      tr("&File"),                              },
        { "document_new",                   tr("New", "File|New"),                    QKeySequence::New },
        { "document_open",                  tr("Open..."),                            QKeySequence::Open },
        { "document_open_recent",           tr("Open Recent"),                        },
        { "document_save",                  tr("Save"),                               QKeySequence::Save },
        { "document_save_as",               tr("Save As..."),                         },
        { "document_print",                 tr("Print..."),                           QKeySequence::Print },
        { "document_print_pdf",             tr("Print to PDF..."),                    },
        { "document_import",                tr("Import"),                             },
        { "document_import_bl_inv",         tr("BrickLink Set Inventory..."),         tr("Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "document_import_bl_xml",         tr("BrickLink XML..."),                   tr("Ctrl+I,Ctrl+X", "File|Import BrickLink XML") },
        { "document_import_bl_order",       tr("BrickLink Order..."),                 tr("Ctrl+I,Ctrl+O", "File|Import BrickLink Order") },
        { "document_import_bl_store_inv",   tr("BrickLink Store Inventory..."),       tr("Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory") },
        { "document_import_bl_cart",        tr("BrickLink Shopping Cart..."),         tr("Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart") },
        { "document_import_ldraw_model",    tr("LDraw Model..."),                     tr("Ctrl+I,Ctrl+L", "File|Import LDraw Model") },
        { "document_export",                tr("Export"),                             },
        { "document_export_bl_xml",         tr("BrickLink XML..."),                         tr("Ctrl+E,Ctrl+X", "File|Import BrickLink XML") },
        { "document_export_bl_xml_clip",    tr("BrickLink Mass-Upload XML to Clipboard"),   tr("Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload") },
        { "document_export_bl_update_clip", tr("BrickLink Mass-Update XML to Clipboard"),   tr("Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update") },
        { "document_export_bl_invreq_clip", tr("BrickLink Set Inventory XML to Clipboard"), tr("Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "document_export_bl_wantedlist_clip", tr("BrickLink Wanted List XML to Clipboard"),   tr("Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List") },
        { "document_close",                 tr("Close"),                              QKeySequence::Close },
        { "application_exit",               tr("Exit"),                               QKeySequence::Quit },
        { "menu_edit",                      tr("&Edit"),                              },
        { "edit_undo",                      nullptr,                                  QKeySequence::Undo },
        { "edit_redo",                      nullptr,                                  QKeySequence::Redo },
        { "edit_cut",                       tr("Cut"),                                QKeySequence::Cut },
        { "edit_copy",                      tr("Copy"),                               QKeySequence::Copy },
        { "edit_paste",                     tr("Paste"),                              QKeySequence::Paste },
        { "edit_delete",                    tr("Delete"),                             QKeySequence::Delete },
        { "edit_additems",                  tr("Add Items..."),                       tr("Insert", "Edit|AddItems") },
        { "edit_subtractitems",             tr("Subtract Items..."),                  },
        { "edit_mergeitems",                tr("Consolidate Items..."),               tr("Ctrl+L", "Edit|Consolidate Items") },
        { "edit_partoutitems",              tr("Part out Item..."),                   },
        { "edit_setmatch",                  tr("Match Items against Set Inventories...") },
        { "edit_copyremarks",               tr("Copy Remarks from Document..."),      },
        { "edit_select_all",                tr("Select All"),                         QKeySequence::SelectAll },
        { "edit_select_none",               tr("Select None"),                        tr("Ctrl+Shift+A") },
        //                                                   QKeySequence::Deselect is only mapped on Linux
        { "edit_filter_from_selection",     tr("Create a Filter from the Selection"), },
        { "edit_filter_focus",              tr("Filter the Item List"),               QKeySequence::Find },
        { "menu_view",                      tr("&View"),                              },
        { "view_toolbar",                   tr("View Toolbar"),                       },
        { "view_docks",                     tr("View Info Docks"),                    },
        { "view_fullscreen",                tr("Full Screen"),                        QKeySequence::FullScreen },
        { "view_simple_mode",               tr("Buyer/Collector Mode"),               },
        { "view_show_input_errors",         tr("Show Input Errors"),                  },
        { "view_column_layout_save",        tr("Save Column Layout..."),              },
        { "view_column_layout_manage",      tr("Manage Column Layouts..."),           },
        { "view_column_layout_load",        tr("Load Column Layout"),                 },
        { "menu_extras",                    tr("E&xtras"),                            },
        { "update_database",                tr("Update Database"),                    },
        { "configure",                      tr("Settings..."),                        },
        { "menu_window",                    tr("&Windows"),                           },
        { "menu_help",                      tr("&Help"),                              },
        { "help_about",                     tr("About..."),                           },
        { "check_for_updates",              tr("Check for Program Updates..."),       },
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
        { "edit_cost",                      tr("Cost"),                               },
        { "edit_cost_round",                tr("Round to 2 Decimal Places"),          },
        { "edit_cost_set",                  tr("Set..."),                             },
        { "edit_cost_inc_dec",              tr("Inc- or Decrease..."),                },
        { "edit_cost_spread",               tr("Spread Cost Amount..."),              },
        { "edit_bulk",                      tr("Bulk Quantity..."),                   },
        { "edit_sale",                      tr("Sale..."),                            tr("Ctrl+%", "Edit|Sale") },
        { "edit_comment",                   tr("Comment"),                            },
        { "edit_comment_set",               tr("Set..."),                             },
        { "edit_comment_add",               tr("Add to..."),                          },
        { "edit_comment_rem",               tr("Remove from..."),                     },
        { "edit_comment_clear",             tr("Clear"),                              },
        { "edit_remark",                    tr("Remark"),                             },
        { "edit_remark_set",                tr("Set..."),                             },
        { "edit_remark_add",                tr("Add to..."),                          },
        { "edit_remark_rem",                tr("Remove from..."),                     },
        { "edit_remark_clear",              tr("Clear"),                              },
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
        { "bricklink_catalog",              tr("Show BrickLink Catalog Info..."),     tr("Ctrl+B,Ctrl+C", "Edit|Show BL Catalog Info") },
        { "bricklink_priceguide",           tr("Show BrickLink Price Guide Info..."), tr("Ctrl+B,Ctrl+P", "Edit|Show BL Price Guide") },
        { "bricklink_lotsforsale",          tr("Show Lots for Sale on BrickLink..."), tr("Ctrl+B,Ctrl+L", "Edit|Show BL Lots for Sale") },
        { "bricklink_myinventory",          tr("Show in my Store on BrickLink..."),   tr("Ctrl+B,Ctrl+I", "Edit|Show BL my Inventory") },
    };

    static const QMap<QByteArray, QByteArray> iconalias = {
        { "update_database", "view_refresh" },
        { "check_for_updates", "update_none" },
        { "edit_status_include", "vcs-normal" },
        { "edit_status_exclude", "vcs-removed" },
        { "edit_status_extra", "vcs-added" },
    };

    for (auto &at : actiontable) {
        if (QAction *a = findAction(at.name)) {
            if (!at.text.isNull())
                a->setText(at.text);
            if (at.standardKey != QKeySequence::UnknownKey)
                a->setShortcuts(at.standardKey);
            else if (!at.shortcut.isNull())
                a->setShortcuts({ QKeySequence(at.shortcut) });

            if (!a->shortcut().isEmpty())
                a->setToolTip(Utility::toolTipLabel(a->text(), a->shortcut()));

            QString iconName = QString::fromLatin1(iconalias.value(at.name, at.name));
            iconName.replace("_", "-");

            QIcon icon = QIcon::fromTheme(iconName);
            if (!icon.isNull())
                a->setIcon(icon);
        }
    }
}

FrameWork::~FrameWork()
{
    Config::inst()->setValue("/MainWindow/Layout/State", saveState(DockStateVersion));
    Config::inst()->setValue("/MainWindow/Layout/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/Filter", m_filter->saveState());

    delete m_add_dialog.data();
    delete m_importinventory_dialog.data();
    delete m_importorder_dialog.data();
    delete m_importcart_dialog.data();

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

void FrameWork::manageLayouts()
{
    ManageColumnLayoutsDialog d(this);
    d.exec();
}

QMenu *FrameWork::createMenu(const QByteArray &name, const QVector<QByteArray> &a_names)
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
            qWarning("Couldn't find action '%s' when creating menu '%s'", an.constData(), name.constData());
    }
    return m;
}


bool FrameWork::setupToolBar(QToolBar *t, const QVector<QByteArray> &a_names)
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

                m_filter = new HistoryLineEdit(Config::MaxFilterHistory, this);
                m_filter->setClearButtonEnabled(true);
                m_filter_delay = new QTimer(this);
                m_filter_delay->setInterval(800ms);
                m_filter_delay->setSingleShot(true);

                // a bit of a hack, but the action needs to be "used" for the shortcut to be
                // active, but we don't want an icon in the filter edit.
                m_filter->QWidget::addAction(findAction("edit_filter_focus"));

                connect(m_filter, &QLineEdit::textChanged, [this]() {
                    m_filter_delay->start();
                });
                connect(m_filter_delay, &QTimer::timeout, [this]() {
                    if (m_filter)
                        emit filterChanged(m_filter->text());
                });
                connect(Config::inst(), &Config::filtersInFavoritesModeChanged,
                        m_filter, &HistoryLineEdit::setToFavoritesMode);
                m_filter->setToFavoritesMode(Config::inst()->areFiltersInFavoritesMode());

                t->addWidget(m_filter);
            } else if (an == "widget_progress") {
                if (m_progress) {
                    qWarning("Only one progress widget can be added to toolbars");
                    continue;
                }
                m_progress = new ProgressCircle();
                m_progress->setIcon(QIcon(":/images/brickstore_icon.png"));
                m_progress->setColor("#4ba2d8");
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

template <typename M = QMenu>
inline static QMenu *newQMenu(QWidget *parent, const char *name, quint32 flags = 0)
{
    M *m = new M(parent);
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
    if (receiver && slot)
        QObject::connect(a, toggle ? &QAction::toggled : &QAction::triggered, receiver, slot);
    return a;
}

inline static QAction *newQAction(QObject *parent, const char *name, quint32 flags = 0, bool toggle = false)
{
    return newQAction(parent, name, flags, toggle, static_cast<QObject *>(nullptr), &QObject::objectName);
}

template <typename Func>
inline static typename std::enable_if<!QtPrivate::FunctionPointer<Func>::IsPointerToMemberFunction &&
                                      !std::is_same<const char*, Func>::value, QAction *>::type
newQAction(QObject *parent, const char *name, quint32 flags, bool toggle, const QObject *context,
           Func slot)
{
    if (auto a = newQAction(parent, name, flags, toggle)) {
        QObject::connect(a, toggle ? &QAction::toggled : &QAction::triggered, context, slot);
        return a;
    }
    return nullptr;
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

    (void) newQAction(this, "document_new", 0, false, this, [this]() {
        createWindow(DocumentIO::create());
    });
    (void) newQAction(this, "document_open", 0, false, this, [this]() {
        createWindow(DocumentIO::open());
    });

    auto rm = new RecentMenu(this);
    rm->menuAction()->setObjectName("document_open_recent");
    connect(rm, &RecentMenu::openRecent,
            this, &FrameWork::openDocument);
    connect(rm, &RecentMenu::clearRecent,
            this, []() { Config::inst()->setRecentFiles({ }); });

    (void) newQAction(this, "document_save", NeedDocument | NeedModification);
    (void) newQAction(this, "document_save_as", NeedDocument);
    (void) newQAction(this, "document_print", NeedDocument | NeedItems);
    (void) newQAction(this, "document_print_pdf", NeedDocument | NeedItems);

    m = newQMenu(this, "document_import");
    m->addAction(newQAction(this, "document_import_bl_inv", 0, false, this, [this]() {
        fileImportBrickLinkInventory(nullptr);
    }));
    m->addAction(newQAction(this, "document_import_bl_xml", 0, false, this, [this]() {
        createWindow(DocumentIO::importBrickLinkXML());
    }));
    m->addAction(newQAction(this, "document_import_bl_order", NeedNetwork, false, this, [this]() {
        if (checkBrickLinkLogin()) {
            if (!m_importorder_dialog)
                m_importorder_dialog = new ImportOrderDialog(this);
            m_importorder_dialog->exec();
        }
    }));
    m->addAction(newQAction(this, "document_import_bl_store_inv", NeedNetwork, false, this, [this]() {
        if (checkBrickLinkLogin())
            createWindow(DocumentIO::importBrickLinkStore());
    }));
    m->addAction(newQAction(this, "document_import_bl_cart", NeedNetwork, false, this, [this]() {
        if (checkBrickLinkLogin()) {
            if (!m_importcart_dialog)
                m_importcart_dialog = new ImportCartDialog(this);
            m_importcart_dialog->exec();
        }
    }));
    m->addAction(newQAction(this, "document_import_ldraw_model", 0, false, this, [this]() {
        createWindow(DocumentIO::importLDrawModel());
    }));


    m = newQMenu(this, "document_export");
    m->addAction(newQAction(this, "document_export_bl_xml", NeedDocument | NeedItems));
    m->addAction(newQAction(this, "document_export_bl_xml_clip", NeedDocument | NeedItems));
    m->addAction(newQAction(this, "document_export_bl_update_clip", NeedDocument | NeedItems));
    m->addAction(newQAction(this, "document_export_bl_invreq_clip", NeedDocument | NeedItems));
    m->addAction(newQAction(this, "document_export_bl_wantedlist_clip", NeedDocument | NeedItems));

    (void) newQAction(this, "document_close", NeedDocument);

    a = newQAction(this, "application_exit", 0, false, this, &FrameWork::close);
    a->setMenuRole(QAction::QuitRole);

    a = m_undogroup->createUndoAction(this);
    a->setObjectName("edit_undo");
    a = m_undogroup->createRedoAction(this);
    a->setObjectName("edit_redo");

    a = newQAction(this, "edit_cut", NeedSelection(1));
    connect(new QShortcut(QKeySequence(int(Qt::ShiftModifier) + int(Qt::Key_Delete)), this),
            &QShortcut::activated, a, &QAction::trigger);
    a = newQAction(this, "edit_copy", NeedSelection(1));
    connect(new QShortcut(QKeySequence(int(Qt::ShiftModifier) + int(Qt::Key_Insert)), this),
            &QShortcut::activated, a, &QAction::trigger);
    a = newQAction(this, "edit_paste", NeedDocument);
    connect(new QShortcut(QKeySequence(int(Qt::ShiftModifier) + int(Qt::Key_Insert)), this),
            &QShortcut::activated, a, &QAction::trigger);
    (void) newQAction(this, "edit_delete", NeedSelection(1));

    a = newQAction(this, "edit_additems", NeedDocument, false, this, &FrameWork::showAddItemDialog);
    a->setShortcutContext(Qt::ApplicationShortcut);

    (void) newQAction(this, "edit_subtractitems", NeedDocument);
    (void) newQAction(this, "edit_mergeitems", NeedSelection(2));
    (void) newQAction(this, "edit_partoutitems", NeedInventory | NeedSelection(1) | NeedQuantity);
    (void) newQAction(this, "edit_setmatch", NeedDocument);
//    (void) newQAction(this, "edit_reset_diffs", NeedSelection(1));
    (void) newQAction(this, "edit_copyremarks", NeedDocument | NeedItems);
    (void) newQAction(this, "edit_select_all", NeedDocument | NeedItems);
    (void) newQAction(this, "edit_select_none", NeedDocument | NeedItems);
    (void) newQAction(this, "edit_filter_from_selection", NeedSelection(1));
    (void) newQAction(this, "edit_filter_focus", NeedDocument, false, this, [this]() {
        m_filter->setFocus();
    });

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

    m = newQMenu(this, "edit_cost", NeedSelection(1));
    m->addAction(newQAction(this, "edit_cost_set", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_cost_inc_dec", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_cost_round", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_cost_spread", NeedSelection(2)));

    (void) newQAction(this, "edit_bulk", NeedSelection(1));
    (void) newQAction(this, "edit_sale", NeedSelection(1));

    m = newQMenu(this, "edit_comment", NeedSelection(1));
    m->addAction(newQAction(this, "edit_comment_set", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_comment_clear", NeedSelection(1)));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_comment_add", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_comment_rem", NeedSelection(1)));

    m = newQMenu(this, "edit_remark", NeedSelection(1));
    m->addAction(newQAction(this, "edit_remark_set", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_remark_clear", NeedSelection(1)));
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

    (void) newQAction(this, "bricklink_catalog", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_priceguide", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_lotsforsale", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_myinventory", NeedSelection(1, 1) | NeedNetwork);

    (void) newQAction(this, "view_fullscreen", 0, true, this, [this](bool fullScreen) {
        setWindowState(windowState().setFlag(Qt::WindowFullScreen, fullScreen));
    });

    m_toolbar->toggleViewAction()->setObjectName("view_toolbar");
    m = newQMenu(this, "view_docks");
    foreach (QDockWidget *dock, m_dock_widgets)
        m->addAction(dock->toggleViewAction());

    (void) newQAction(this, "view_simple_mode", 0, true, Config::inst(), &Config::setSimpleMode);
    (void) newQAction(this, "view_show_input_errors", 0, true, Config::inst(), &Config::setShowInputErrors);
    (void) newQAction(this, "view_column_layout_save", NeedDocument, false);
    (void) newQAction(this, "view_column_layout_manage", 0, false, this, &FrameWork::manageLayouts);
    auto lclm = newQMenu<LoadColumnLayoutMenu>(this, "view_column_layout_load", NeedDocument);
    lclm->menuAction()->setIcon(QIcon::fromTheme("object-columns"));
    lclm->setObjectName("view_column_layout_list");

    (void) newQAction(this, "update_database", NeedNetwork, false, this, &FrameWork::updateDatabase);

    a = newQAction(this, "configure", 0, false, this, [this]() { showSettings(); });
    a->setMenuRole(QAction::PreferencesRole);

    //(void) newQAction(this, "help_whatsthis", 0, false, this, &FrameWork::whatsThis);

    a = newQAction(this, "help_about", 0, false, Application::inst(), [this]() {
        AboutDialog(this).exec();
    });
    a->setMenuRole(QAction::AboutRole);

    a = newQAction(this, "check_for_updates", NeedNetwork, false, this, &FrameWork::checkForUpdates);
    a->setMenuRole(QAction::ApplicationSpecificRole);
}


void FrameWork::openDocument(const QString &file)
{
    createWindow(DocumentIO::open(file));
}

void FrameWork::fileImportBrickLinkInventory(const BrickLink::Item *item, int quantity,
                                             BrickLink::Condition condition)
{
    bool instructions = false;
    BrickLink::Status extraParts = BrickLink::Status::Extra;

    if (!item) {
        if (!m_importinventory_dialog)
            m_importinventory_dialog = new ImportInventoryDialog(this);

        if (m_importinventory_dialog->exec() == QDialog::Accepted) {
            item = m_importinventory_dialog->item();
            quantity = m_importinventory_dialog->quantity();
            condition = m_importinventory_dialog->condition();
            instructions = m_importinventory_dialog->includeInstructions();
            extraParts = m_importinventory_dialog->extraParts();
        }
    }

    createWindow(DocumentIO::importBrickLinkInventory(item, quantity, condition, extraParts,
                                                      instructions));
}

bool FrameWork::checkBrickLinkLogin()
{
    forever {
        QPair<QString, QString> auth = Config::inst()->loginForBrickLink();

        if (!auth.first.isEmpty() && !auth.second.isEmpty())
            return true;

        if (MessageBox::question(this, { },
                                 tr("No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?")
                                 ) == MessageBox::Yes)
            showSettings("bricklink");
        else
            return false;
    }
}

void FrameWork::restoreWindowsFromAutosave()
{
    int restorable = Window::restorableAutosaves();
    if (restorable > 0) {
        bool b = (MessageBox::question(this, tr("Restore Documents"), tr("It seems like BrickStore crashed while %n document(s) had unsaved modifications.", nullptr, restorable)
                                       % u"<br><br>" % tr("Should these documents be restored from their last available auto-save state?"),
                                       MessageBox::Yes | MessageBox::No, MessageBox::Yes)
                  == MessageBox::Yes);

        auto restoredWindows = Window::processAutosaves(b ? Window::AutosaveAction::Restore
                                                          : Window::AutosaveAction::Delete);
        for (auto window : restoredWindows) {
            m_undogroup->addStack(window->document()->undoStack());
            m_workspace->addWindow(window);
            setActiveWindow(window);
        }
    }
}

Window *FrameWork::createWindow(Document *doc)
{
    if (!doc)
        return nullptr;

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

        if (doc->legacyCurrencyCode()
                && (Config::inst()->defaultCurrencyCode() != QLatin1String("USD"))) {
            QMetaObject::invokeMethod(this, []() {
                MessageBox::information(nullptr, { }, tr("You have loaded an old style document that does not have any currency information attached. You can convert this document to include this information by using the currency code selector in the top right corner."));
            }, Qt::QueuedConnection);
        }
    }

    setActiveWindow(window);
    return window;
}

Window *FrameWork::activeWindow() const
{
    return m_current_window;
}

void FrameWork::setActiveWindow(Window *window)
{
    m_workspace->setActiveWindow(window);
    window->setFocus();
}

bool FrameWork::updateDatabase(bool forceSync)
{
    bool noWindows = m_workspace->windowList().isEmpty();

    if (!noWindows && forceSync) {
        qWarning() << "Cannot force a DB update, if windows are open";
        return false;
    }

    if (noWindows || closeAllWindows()) {
        delete m_add_dialog;

        auto doUpdate = [this]() -> bool {
            if (!m_workspace->windowList().isEmpty())
                return false;

            Transfer trans;
            ProgressDialog d(tr("Update Database"), &trans, this);
            UpdateDatabase update(&d);
            return d.exec();
        };

        if (forceSync || noWindows) {
            return doUpdate();
        } else {
            // the windows are WA_DeleteOnClose, but this triggers only a deleteLater:
            // the current window doesn't change until a return to the event loop

            QMetaObject::invokeMethod(this, doUpdate, Qt::QueuedConnection);
            return true;
        }
    }
    return false;
}


void FrameWork::checkForUpdates(bool silent)
{
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);

    QString url = Application::inst()->applicationUrl();
    url.replace("github.com", "https://api.github.com/repos");
    url.append("/releases/latest");
    QNetworkReply *reply = nam->get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        reply->deleteLater();
        reply->manager()->deleteLater();

        QByteArray data = reply->readAll();
        auto currentVersion = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        bool succeeded = false;
        bool hasUpdate = false;
        QString message;

        QJsonParseError jsonError;
        auto doc = QJsonDocument::fromJson(data, &jsonError);
        if (doc.isNull()) {
            qWarning() << data.constData();
            qWarning() << "\nCould not parse GitHub JSON reply:" << jsonError.errorString();
            message = tr("Could not parse server response.");
        } else {
            QString tag = doc["tag_name"].toString();
            if (tag.startsWith('v'))
                tag.remove(0, 1);
            QVersionNumber version = QVersionNumber::fromString(tag);
            if (version.isNull()) {
                qWarning() << "Cannot parse GitHub's latest tag_name:" << tag;
                message = tr("Version information is not available.");
            } else {
                if (version <= currentVersion) {
                    message = tr("Your currently installed version is up-to-date.");
                } else {
                    message = tr("A newer version than the one currently installed is available:");
                    message += QString::fromLatin1(R"(<br/><br/><strong>%1</strong> <a href="https://%2/releases/tag/v%3">%4</a><br/>)")
                            .arg(version.toString(), Application::inst()->applicationUrl(),
                                 version.toString(), tr("Download"));
                    hasUpdate = true;
                }
                succeeded = true;
            }
        }

        if (!message.isEmpty() && (!silent || hasUpdate)) {
            QMetaObject::invokeMethod(this, [succeeded, message]() {
                auto title = FrameWork::tr("Program Update");
                if (succeeded)
                    MessageBox::information(FrameWork::inst(), title, message);
                else
                    MessageBox::warning(FrameWork::inst(), title, message);
            });
        }
    });
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

    QString filterToolTip;

    if (m_current_window) {
        Document *doc = m_current_window->document();

        connectAllActions(false, m_current_window);

        disconnect(m_current_window.data(), &Window::windowTitleChanged,
                   this, &FrameWork::titleUpdate);
        disconnect(doc, &Document::modificationChanged,
                   this, &FrameWork::modificationUpdate);
        disconnect(m_current_window.data(), &Window::selectionChanged,
                   this, &FrameWork::selectionUpdate);
        if (m_filter) {
            disconnect(this, &FrameWork::filterChanged,
                       doc, &Document::setFilter);
            disconnect(doc, &Document::filterChanged,
                       this, &FrameWork::setFilter);

            m_filter->setText(QString());
            m_filter->setEnabled(false);
        }
        m_undogroup->setActiveStack(nullptr);

        m_current_window = nullptr;
    }

    if (Window *window = qobject_cast<Window *>(w)) {
        Document *doc = window->document();

        connectAllActions(true, window);

        connect(window, &Window::windowTitleChanged,
                this, &FrameWork::titleUpdate);
        connect(doc, &Document::modificationChanged,
                this, &FrameWork::modificationUpdate);
        connect(window, &Window::selectionChanged,
                this, &FrameWork::selectionUpdate);

        if (m_filter) {
            m_filter->setText(doc->filter());
            filterToolTip = doc->filterToolTip();
            connect(this, &FrameWork::filterChanged,
                    doc, &Document::setFilter);
            connect(doc, &Document::filterChanged,
                    this, &FrameWork::setFilter);

            if (auto a = findAction("edit_filter_focus"))
                m_filter->setToolTip(Utility::toolTipLabel(a->text(), a->shortcut(),
                                                           filterToolTip
                                                           + m_filter->instructionToolTip()));
            m_filter->setEnabled(true);
        }

        m_undogroup->setActiveStack(doc->undoStack());

        // update per-document action states

        //findAction("view_difference_mode")->setChecked(window->isDifferenceMode());
        m_current_window = window;
    }

    if (m_add_dialog) {
        m_add_dialog->attach(m_current_window);
        if (!m_current_window)
            m_add_dialog->close();
    }

    findAction("edit_additems")->setEnabled((m_current_window));

    selectionUpdate(m_current_window ? m_current_window->selection() : Document::ItemList());
    titleUpdate();
    modificationUpdate();

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

            if (b) {
                if (flags & NeedItems)
                    b = b && (m_current_window->document()->rowCount() > 0);

                quint8 minSelection = (flags >> 24) & 0xff;
                quint8 maxSelection = (flags >> 16) & 0xff;

                if (minSelection)
                    b = b && (cnt >= minSelection);
                if (maxSelection)
                    b = b && (cnt <= maxSelection);
            }
        }

        if (flags & NeedItemMask) {
            foreach (Document::Item *item, selection) {
                if (flags & NeedLotId)
                    b = b && (item->lotId() != 0);
                if (flags & NeedInventory)
                    b = b && (item->item() && item->item()->hasInventory());
                if (flags & NeedQuantity)
                    b = b && (item->quantity() != 0);
                if (flags & NeedSubCondition)
                    b = b && (item->item() && item->itemType() && item->item()->itemType()->hasSubConditions());

                if (!b)
                    break;
            }
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

void FrameWork::titleUpdate()
{
    QString title = QApplication::applicationName();
    QString file;

    if (m_current_window) {
        title = m_current_window->windowTitle() % u" \u2014 " % title;
        file = m_current_window->document()->fileName();
    }
    setWindowTitle(title);
    setWindowFilePath(file);
}

void FrameWork::modificationUpdate()
{
    bool modified = m_current_window ? m_current_window->isWindowModified() : false;
    setWindowModified(modified);
    if (QAction *a = findAction("document_save"))
        a->setEnabled(modified);
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

void FrameWork::showSettings(const char *page)
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

void FrameWork::setFilter(const QString &filter)
{
    if (m_filter)
        m_filter->setText(filter);
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

QVector<Window *> FrameWork::allWindows() const
{
    QVector<Window *> all;
    foreach(QWidget *w, m_workspace->windowList())
        all << static_cast<Window *>(w);
    return all;
}

void FrameWork::cancelAllTransfers(bool force)
{
    if (force || (MessageBox::question(nullptr, { },
                                       tr("Do you want to cancel all outstanding inventory, image and Price Guide transfers?")
                                       ) == MessageBox::Yes)) {
        BrickLink::core()->cancelPictureTransfers();
        BrickLink::core()->cancelPriceGuideTransfers();
    }
}

void FrameWork::showAddItemDialog()
{
    if (!m_add_dialog) {
        m_add_dialog = new AddItemDialog();
        m_add_dialog->setObjectName(QLatin1String("additems"));
        m_add_dialog->attach(m_current_window);

        connect(m_add_dialog, &AddItemDialog::closed,
                this, [this]() {
            show();
            raise();
            activateWindow();
        });
    }

    if (m_add_dialog->isVisible()) {
        m_add_dialog->raise();
        m_add_dialog->activateWindow();
    } else {
        m_add_dialog->show();
    }
}

#include "framework.moc"

#include "moc_framework.cpp"
