/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QActionGroup>
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
#include <QStringBuilder>
#include <QDesktopServices>
#include <QWidgetAction>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QProgressDialog>
#include <QDialogButtonBox>
#include <QStackedLayout>
#include <QSplitter>
#if defined(Q_OS_WINDOWS)
#  if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#    include <QWinTaskbarButton>
#    include <QWinTaskbarProgress>
#    include <QtPlatformHeaders/QWindowsWindowFunctions>
#  else
#    include <qwintaskbarbutton.h>
#    include <qwintaskbarprogress.h>
#    include <QtGui/qpa/qplatformwindow.h>
#    include <QtGui/qpa/qplatformwindow_p.h>
#  endif
#endif

#include "bricklink/store.h"
#include "common/actionmanager.h"
#include "common/announcements.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/onlinestate.h"
#include "qcoro/core/qcorosignal.h"
#include "utility/currency.h"
#include "utility/exception.h"
#include "common/uihelpers.h"
#include "utility/stopwatch.h"
#include "utility/undo.h"
#include "utility/utility.h"
#include "checkforupdates.h"
#include "desktopuihelpers.h"
#include "developerconsole.h"
#include "mainwindow.h"
#include "mainwindow_p.h"
#include "historylineedit.h"
#include "progresscircle.h"
#include "script.h"
#include "scriptmanager.h"
#include "taskwidgets.h"
#include "view.h"
#include "viewpane.h"
#include "welcomewidget.h"

#include "aboutdialog.h"
#include "additemdialog.h"
#include "announcementsdialog.h"
#include "importcartdialog.h"
#include "importinventorydialog.h"
#include "importorderdialog.h"
#include "managecolumnlayoutsdialog.h"
#include "settingsdialog.h"
#include "systeminfodialog.h"

using namespace std::chrono_literals;


enum {
    DockStateVersion = 3 // increase if you change the dock/toolbar setup
};


MainWindow *MainWindow::s_inst = nullptr;

MainWindow *MainWindow::inst()
{
    if (s_inst == reinterpret_cast<MainWindow *>(-1)) // destructed
        return nullptr;

    if (!s_inst)
        (new MainWindow())->setAttribute(Qt::WA_DeleteOnClose);
    return s_inst;
}



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    s_inst = this;

    DesktopUIHelpers::setDefaultParent(this);

    m_progress = nullptr;

    m_devConsole = new DeveloperConsole();
    Application::inst()->setUILoggingHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        if (MainWindow::inst())
            MainWindow::inst()->m_devConsole->messageHandler(type, ctx, msg);
    });
    connect(m_devConsole, &DeveloperConsole::execute,
            this, [](const QString &command, bool *successful) {
        *successful = ScriptManager::inst()->executeString(command);
    });

    // keep QTBUG-39781 in mind: we cannot use QOpenGLWidget directly
    setUnifiedTitleAndToolBarOnMac(true);

    setDocumentMode(true);
    setAcceptDrops(true);

    connect(Application::inst(), &Application::showSettings,
            this, &MainWindow::showSettings);

    // vvv increase DockStateVersion if you change the dock/toolbar setup

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName("toolbar"_l1);
    m_toolbar->setMovable(false);

    connect(m_toolbar, &QToolBar::visibilityChanged,
            this, [this]() {
        if (m_welcomeWidget->isVisible())
            repositionHomeWidget();
    });

    auto setIconSizeLambda = [this](Config::IconSize iconSize) {
        static const QMap<Config::IconSize, QStyle::PixelMetric> map = {
            { Config::IconSize::System, QStyle::PM_ToolBarIconSize },
            { Config::IconSize::Small, QStyle::PM_SmallIconSize },
            { Config::IconSize::Large, QStyle::PM_LargeIconSize },
        };
        auto pm = map.value(iconSize, QStyle::PM_ToolBarIconSize);
        int s = style()->pixelMetric(pm, nullptr, this);
        m_toolbar->setIconSize(QSize(s, s));
    };
    connect(Config::inst(), &Config::iconSizeChanged, this, setIconSizeLambda);
    setIconSizeLambda(Config::inst()->iconSize());

    createActions();
    setupMenuBar();
    setupToolBar();
    addToolBar(m_toolbar);
    setupDockWidgets();

    createCentralWidget();

    languageChange();

    connect(OnlineState::inst(), &OnlineState::onlineStateChanged,
            m_progress, &ProgressCircle::setOnlineState);
    m_progress->setOnlineState(OnlineState::inst()->isOnline());

    bool showInputErrors = Config::inst()->showInputErrors();
    ActionManager::inst()->qAction("view_show_input_errors")->setChecked(showInputErrors);
    ActionManager::inst()->qAction("view_goto_next_input_error")->setEnabled(showInputErrors);
    bool showDiffIndicators = Config::inst()->showDifferenceIndicators();
    ActionManager::inst()->qAction("view_show_diff_indicators")->setChecked(showDiffIndicators);
    ActionManager::inst()->qAction("view_goto_next_diff")->setEnabled(showDiffIndicators);

    connect(BrickLink::core(), &BrickLink::Core::transferProgress,
            this, &MainWindow::transferProgressUpdate);

    connectView(nullptr);

    // we need to show now, since most X11 window managers and macOS with
    // unified-toolbar look won't get the position right otherwise
    // plus, on Windows, we need the window handle for the progress indicator
    show();

#if defined(Q_OS_WINDOWS)
    auto winTaskbarButton = new QWinTaskbarButton(this);
    winTaskbarButton->setWindow(windowHandle());

    connect(BrickLink::core(), &BrickLink::Core::transferProgress, this, [winTaskbarButton](int p, int t) {
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
    // workaround for QOpenGLWidget messing with fullscreen mode
#  if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QWindowsWindowFunctions::setHasBorderInFullScreen(windowHandle(), true);
#  else
    dynamic_cast<QNativeInterface::Private::QWindowsWindow *>(windowHandle()->handle())->setHasBorderInFullScreen(true);
#  endif
#endif

    QByteArray ba;

    ba = Config::inst()->value("/MainWindow/Layout/Geometry"_l1).toByteArray();
    restoreGeometry(ba);

    ba = Config::inst()->value("/MainWindow/Layout/State"_l1).toByteArray();
    if (ba.isEmpty() || !restoreState(ba, DockStateVersion))
        m_toolbar->show();
    menuBar()->show();

    ActionManager::inst()->qAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

    connect(Currency::inst(), &Currency::updateRatesFailed,
            this, [](const QString &errorString) {
        UIHelpers::warning(errorString);
    });

    Currency::inst()->updateRates();
    auto *currencyUpdateTimer = new QTimer(this);
    currencyUpdateTimer->start(4h);
    currencyUpdateTimer->callOnTimeout(Currency::inst(),
                                       []() { Currency::inst()->updateRates(true /*silent*/); });

    m_checkForUpdates = new CheckForUpdates(Application::inst()->gitHubUrl(), this);
    connect(Application::inst()->announcements(), &Announcements::newAnnouncements,
            this, [this]() {
        AnnouncementsDialog::showNewAnnouncements(Application::inst()->announcements(), this);
    });

    m_checkForUpdates->check(true /*silent*/);
    auto checkTimer = new QTimer(this);
    checkTimer->callOnTimeout(this, [this]() { m_checkForUpdates->check(true /*silent*/); });
    checkTimer->start(24h);

    setupScripts();

    connect(DocumentList::inst(), &DocumentList::documentAdded,
            this, [this](Document *document) {
        // active pane? if not -> create one
        Q_ASSERT(m_activeViewPane);
        goHome(false);
        m_activeViewPane->activateDocument(document);

        connect(document, &Document::requestActivation,
                this, [this]() {
            Q_ASSERT(m_activeViewPane);
            if (auto *doc = qobject_cast<Document *>(sender())) {
                goHome(false);
                m_activeViewPane->activateDocument(doc);
            }
        });
    });
    connect(DocumentList::inst(), &DocumentList::documentRemoved,
            this, [this](Document *) {
        if (DocumentList::inst()->count() == 0)
            goHome(true);
    });

    connect(Config::inst(), &Config::toolBarActionsChanged,
            this, &MainWindow::setupToolBar);
}

void MainWindow::setupScripts()
{
    auto reloadScripts = [this](const QVector<Script *> &scripts) {
        QList<QAction *> extrasActions;

        for (auto script : scripts) {
            const auto extensionActions = script->extensionActions();
            for (auto extensionAction : extensionActions) {
                auto action = new QAction(extensionAction->text(), extensionAction);
                if (extensionAction->location() == ExtensionScriptAction::Location::ContextMenu)
                    m_extensionContextActions.append(action);
                else
                    extrasActions.append(action);

                connect(action, &QAction::triggered, extensionAction, [extensionAction]() {
                    try {
                        extensionAction->executeAction();
                    } catch (const Exception &e) {
                        UIHelpers::warning(e.error());
                    }
                });
            }
            const auto printingActions = script->printingActions();
            for (auto printingAction : printingActions) {
                auto action = new QAction(printingAction->text(), printingAction);
                extrasActions.append(action);

                connect(action, &QAction::triggered, printingAction, [this, printingAction]() {
                    if (m_activeView)
                        m_activeView->printScriptAction(printingAction);
                });
            }
        }

        if (!extrasActions.isEmpty()) {
            auto actions = m_extrasMenu->actions();
            QAction *position = actions.at(actions.size() - 2);
            m_extrasMenu->insertActions(position, extrasActions);
        }
    };

    connect(ScriptManager::inst(), &ScriptManager::aboutToReload,
            this, [this]() {
        auto actions = m_extrasMenu->actions();
        bool deleteRest = false;
        for (QAction *a : qAsConst(actions)) {
            if (a->objectName() == "scripts-start"_l1)
                deleteRest = true;
            else if (a->objectName() == "scripts-end"_l1)
                break;
            else if (deleteRest)
                m_extrasMenu->removeAction(a);
        }
        m_extensionContextActions.clear();
    });
    connect(ScriptManager::inst(), &ScriptManager::reloaded,
            this, [reloadScripts]() {
        reloadScripts(ScriptManager::inst()->scripts());
    });
    reloadScripts(ScriptManager::inst()->scripts());
}

void MainWindow::languageChange()
{
    m_toolbar->setWindowTitle(tr("Toolbar"));

    foreach (QDockWidget *dock, m_dock_widgets) {
        QString name = dock->objectName();

        if (name == "dock_info"_l1)
            dock->setWindowTitle(tr("Info"));
        if (name == "dock_priceguide"_l1)
            dock->setWindowTitle(tr("Price Guide"));
        if (name == "dock_appearsin"_l1)
            dock->setWindowTitle(tr("Appears In Sets"));
        if (name == "dock_opendocuments"_l1)
            dock->setWindowTitle(tr("Open Documents"));
        if (name == "dock_recentdocuments"_l1)
            dock->setWindowTitle(tr("Recent Documents"));
        if (name == "dock_errorlog"_l1)
            dock->setWindowTitle(tr("Error Log"));
    }
    if (m_progress) {
        m_progress->setToolTipTemplates(tr("Offline"),
                                        tr("No outstanding jobs"),
                                        tr("Downloading...<br><b>%p%</b> finished<br>(%v of %m)"));
    }
}

MainWindow::~MainWindow()
{
    Config::inst()->setValue("/MainWindow/Layout/State"_l1, saveState(DockStateVersion));
    Config::inst()->setValue("/MainWindow/Layout/Geometry"_l1, saveGeometry());

    delete m_add_dialog.data();
    delete m_importinventory_dialog.data();
    delete m_importorder_dialog.data();
    delete m_importcart_dialog.data();

    s_inst = reinterpret_cast<MainWindow *>(-1);
}

void MainWindow::forEachViewPane(std::function<bool(ViewPane *)> callback)
{
    if (!callback)
        return;

    QWidget *w = centralWidget();

    std::function<bool(QWidget *)> recurse = [&callback, &recurse](QWidget *w) {
        if (auto *splitter = qobject_cast<QSplitter *>(w)) {
            for (int i = 0; i < splitter->count(); ++i) {
                if (recurse(splitter->widget(i)))
                    return true;
            }
        } else if (auto *vp = qobject_cast<ViewPane *>(w)) {
            return callback(vp);
        }
        return false;
    };
    recurse(w);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    foreach (QUrl u, e->mimeData()->urls())
        Document::load(u.toLocalFile());

    e->setDropAction(Qt::CopyAction);
    e->accept();
}

void MainWindow::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QMainWindow::changeEvent(e);
}

void MainWindow::createCentralWidget()
{
    m_welcomeWidget = new WelcomeWidget(this);
    m_welcomeWidget->setAutoFillBackground(true);
    repositionHomeWidget();

    auto *rootSplitter = new QSplitter();
    rootSplitter->setObjectName("RootSplitter"_l1);
    auto *vp = createViewPane(nullptr);
    rootSplitter->addWidget(vp);
    setCentralWidget(rootSplitter);
    setActiveViewPane(vp);

    connect(qApp, &QApplication::focusChanged,
            this, [this](QWidget *, QWidget *now) {
        for ( ; now; now = now->parentWidget()) {
            if (auto vp = qobject_cast<ViewPane *>(now)) {
                if (m_goHome->isChecked())
                    goHome(false);
                setActiveViewPane(vp);
                break;
            }
        }
    });
    repositionHomeWidget();
}

void MainWindow::setActiveViewPane(ViewPane *newActive)
{
    if (m_activeViewPane == newActive)
        return;

    if (m_activeViewPane)
        m_activeViewPane->setActive(false);
    m_activeViewPane = newActive;
    if (m_activeViewPane)
        m_activeViewPane->setActive(true);
}

ViewPane *MainWindow::createViewPane(Document *activeDocument)
{
    auto vp = new ViewPane([this](Document *doc) { return createViewPane(doc); },
            [this](ViewPane *vp) { deleteViewPane(vp); },
            activeDocument);

    connect(vp, &ViewPane::viewActivated,
            this, [this, vp](View *view) {
        setActiveViewPane(vp);
        connectView(view);
    });

    setActiveViewPane(vp);
    return vp;
}

void MainWindow::deleteViewPane(ViewPane *viewPane)
{
    if (m_activeViewPane == viewPane) {
        ViewPane *newActive = nullptr;

        // we need to find another pane to transfer the active state
        forEachViewPane([viewPane, &newActive](ViewPane *vp) {
            if (vp != viewPane) {
                newActive = vp;
                return true;
            }
            return false;
        });

        Q_ASSERT(viewPane);
        setActiveViewPane(newActive);
    }

    viewPane->deleteLater();
}

QDockWidget *MainWindow::createDock(QWidget *widget, const char *name)
{
    QDockWidget *dock = new QDockWidget(QString(), this);
    dock->setObjectName(QLatin1String(name));
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    dock->setTitleBarWidget(new FancyDockTitleBar(dock));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setWidget(widget);
    m_dock_widgets.append(dock);
    return dock;
}

void MainWindow::goHome(bool home)
{
    if (home) {
        repositionHomeWidget();
        m_welcomeWidget->show();
        m_welcomeWidget->setFocus();
        connectView(nullptr);
    } else {
        m_welcomeWidget->hide();
        if (m_activeViewPane && m_activeViewPane->activeView())
            m_activeViewPane->activeView()->setFocus();
    }
    m_goHome->setIcon(QIcon::fromTheme(home ? "go-previous"_l1 : "go-home"_l1));
    m_goHome->setChecked(home);
    m_goHome->setEnabled(DocumentList::inst()->count() > 0);
}

void MainWindow::repositionHomeWidget()
{
    QRect r = rect();
    if (menuBar() && menuBar()->isVisible())
        r.setTop(r.top() + menuBar()->height());
    if (m_toolbar->isVisible())
        r.setTop(r.top() + m_toolbar->height());

    m_welcomeWidget->setGeometry(r);
    m_welcomeWidget->raise();
}

void MainWindow::setupMenuBar()
{
    ActionManager *am = ActionManager::inst();

    auto *viewDocksMenu = am->qAction("view_docks")->menu();
    connect(viewDocksMenu, &QMenu::aboutToShow,
            this, [this, viewDocksMenu]() {
        viewDocksMenu->clear();
        for (QDockWidget *dock : qAsConst(m_dock_widgets))
            viewDocksMenu->addAction(dock->toggleViewAction());
    });

    setupMenu("document_import", {
                  "document_import_bl_inv",
                  "document_import_bl_xml",
                  "document_import_bl_order",
                  "document_import_bl_store_inv",
                  "document_import_bl_cart",
                  "document_import_ldraw_model",
              });

    setupMenu("document_export", {
                  "document_export_bl_xml",
                  "document_export_bl_xml_clip",
                  "document_export_bl_update_clip",
                  "document_export_bl_invreq_clip",
                  "document_export_bl_wantedlist_clip",
              });

    setupMenu("edit_status", {
                  "edit_status_include",
                  "edit_status_exclude",
                  "edit_status_extra",
                  "-",
                  "edit_status_toggle",
              });

    setupMenu("edit_cond", {
                  "edit_cond_new",
                  "edit_cond_used",
                  "-",
                  "edit_subcond_none",
                  "edit_subcond_sealed",
                  "edit_subcond_complete",
                  "edit_subcond_incomplete",
              });

    setupMenu("edit_qty", {
                  "edit_qty_set",
                  "edit_qty_multiply",
                  "edit_qty_divide",
              });

    setupMenu("edit_price", {
                  "edit_price_set",
                  "edit_price_inc_dec",
                  "edit_price_to_priceguide",
                  "edit_price_round",
              });

    setupMenu("edit_cost", {
                  "edit_cost_set",
                  "edit_cost_inc_dec",
                  "edit_cost_round",
                  "edit_cost_spread_price",
                  "edit_cost_spread_weight",
              });

    setupMenu("edit_comment", {
                  "edit_comment_set",
                  "edit_comment_clear",
                  "-",
                  "edit_comment_add",
                  "edit_comment_rem",
              });

    setupMenu("edit_remark", {
                  "edit_remark_set",
                  "edit_remark_clear",
                  "-",
                  "edit_remark_add",
                  "edit_remark_rem",
              });

    setupMenu("edit_retain", {
                  "edit_retain_yes",
                  "edit_retain_no",
                  "-",
                  "edit_retain_toggle",
              });

    setupMenu("edit_stockroom", {
                  "edit_stockroom_no",
                  "edit_stockroom_a",
                  "edit_stockroom_b",
                  "edit_stockroom_c",
              });

    setupMenu("edit_marker", {
                  "edit_marker_text",
                  "edit_marker_color",
                  "-",
                  "edit_marker_clear",
              });

    setupMenu("menu_file", {
                  "document_new",
                  "document_open",
                  "document_open_recent",
                  "-",
                  "document_save",
                  "document_save_as",
                  "document_save_all",
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
              });

    setupMenu("menu_edit", {
                  "edit_undo",
                  "edit_redo",
                  "-",
                  "edit_cut",
                  "edit_copy",
                  "edit_paste",
                  "edit_paste_silent",
                  "edit_duplicate",
                  "edit_delete",
                  "-",
                  "edit_select_all",
                  "edit_select_none",
                  "-",
                  "edit_additems",
                  "edit_subtractitems",
                  "edit_mergeitems",
                  "edit_partoutitems",
                  "-",
                  "edit_item",
                  "edit_color",
                  "edit_status",
                  "edit_cond",
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
                  "edit_marker",
                  "-",
                  "edit_copy_fields",
                  "-",
                  "bricklink_catalog",
                  "bricklink_priceguide",
                  "bricklink_lotsforsale",
                  "bricklink_myinventory"
              });

    setupMenu("menu_view", {
                  "view_toolbar",
                  "view_docks",
                  "-",
                  "view_fullscreen",
                  "-",
                  "view_show_input_errors",
                  "view_goto_next_input_error",
                  "-",
                  "view_show_diff_indicators",
                  "view_reset_diff_mode",
                  "view_goto_next_diff",
                  "-",
                  "view_filter",
                  "-",
                  "view_column_layout_save",
                  "view_column_layout_manage",
                  "view_column_layout_load"
              });

    m_extrasMenu = setupMenu("menu_extras", {
                                 "update_database",
                                 "-",
                                 "configure",
                                 "-scripts-start",
                                 "-scripts-end",
                                 "reload_scripts",
                             });
    setupMenu("menu_help", {
                  "help_extensions",
                  "-",
                  "check_for_updates",
                  "-",
                  "help_reportbug",
                  "help_announcements",
                  "help_releasenotes",
                  "-",
                  "help_systeminfo",
#if !defined(Q_OS_MACOS) // this should be collapsed, but isn't
                  "-",
#endif
                  "help_about"
              });

    auto loadColumnLayoutMenuAction = am->qAction("view_column_layout_load");
    m_loadColumnLayoutMenu = new LoadColumnLayoutMenuAdapter(loadColumnLayoutMenuAction->menu());

    menuBar()->addAction(am->qAction("menu_file"));
    menuBar()->addAction(am->qAction("menu_edit"));

    menuBar()->addAction(am->qAction("menu_view"));
    menuBar()->addAction(am->qAction("menu_extras"));

    auto windowMenuAction = am->qAction("menu_window");
    (void) new WindowMenuAdapter(this, windowMenuAction->menu(), true);
    menuBar()->addAction(windowMenuAction);

    menuBar()->addAction(am->qAction("menu_help"));
}

bool MainWindow::setupToolBar()
{
    if (!m_toolbar)
        return false;
    m_toolbar->clear();

    QStringList actionNames = toolBarActionNames();
    if (actionNames.isEmpty())
        actionNames = defaultToolBarActionNames();

    actionNames = QStringList { "go_home"_l1, "-"_l1 } + actionNames
            + QStringList { "<>"_l1, "widget_progress"_l1, "|"_l1 };

    for (const QString &an : qAsConst(actionNames)) {
        if (an == "-"_l1) {
            m_toolbar->addSeparator()->setObjectName(an);
        } else if (an == "|"_l1) {
            QWidget *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = 2 * style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setFixedSize(sp, sp);
            m_toolbar->addWidget(spacer);
        } else if (an == "<>"_l1) {
            QWidget *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = 2 * style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setMinimumSize(sp, sp);
            spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            m_toolbar->addWidget(spacer);
        } else if (an == "edit_filter_focus"_l1) {
            // the old filter control - ignore
        } else if (an == "widget_progress"_l1) {
            m_toolbar->addAction(m_progressAction);
        } else if (QAction *a = ActionManager::inst()->qAction(an.toLatin1().constData())) {
            m_toolbar->addAction(a);

            // workaround for Qt bug: can't set the popup mode on a QAction
            QToolButton *tb;
            if (a->menu() && (tb = qobject_cast<QToolButton *>(m_toolbar->widgetForAction(a))))
                tb->setPopupMode(QToolButton::InstantPopup);
        } else {
            qWarning() << "Couldn't find action" << an;
        }
    }
    return true;
}

void MainWindow::setupDockWidgets()
{
    // vvv increase DockStateVersion if you change the dock/toolbar setup

    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);

    auto dockInfo = createDock(new TaskInfoWidget, "dock_info");
    auto dockOpen = createDock(new TaskOpenDocumentsWidget, "dock_opendocuments");
    auto dockRecent = createDock(new TaskRecentDocumentsWidget, "dock_recentdocuments");
    auto dockAppearsIn = createDock(new TaskAppearsInWidget, "dock_appearsin");
    auto dockPriceGuide = createDock(new TaskPriceGuideWidget, "dock_priceguide");

    addDockWidget(Qt::LeftDockWidgetArea, dockInfo);
    addDockWidget(Qt::LeftDockWidgetArea, dockOpen);
    addDockWidget(Qt::LeftDockWidgetArea, dockRecent);
    addDockWidget(Qt::LeftDockWidgetArea, dockAppearsIn);
    addDockWidget(Qt::LeftDockWidgetArea, dockPriceGuide);
    tabifyDockWidget(dockRecent, dockOpen);
    tabifyDockWidget(dockOpen, dockInfo);
    tabifyDockWidget(dockAppearsIn, dockPriceGuide);

    auto logDock = createDock(m_devConsole, "dock_errorlog");
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    logDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, logDock, Qt::Horizontal);
}

QMenu *MainWindow::setupMenu(const QByteArray &name, const QVector<QByteArray> &a_names)
{
    if (a_names.isEmpty())
        return nullptr;

    QAction *a = ActionManager::inst()->qAction(name);
    QMenu *m = a ? a->menu() : nullptr;
    if (!a || !m)
        return nullptr;

    for (const QByteArray &an : a_names) {
        if (an.startsWith("-")) {
            auto *a = m->addSeparator();
            if (an.length() > 1)
                a->setObjectName(QString::fromLatin1(an.mid(1)));
        } else if (QAction *a = ActionManager::inst()->qAction(an.constData())) {
            m->addAction(a);
        } else  {
            qWarning("Couldn't find action '%s' when creating menu '%s'",
                     an.constData(), name.constData());
        }
    }
    return m;
}

void MainWindow::createActions()
{
    ActionManager *am = ActionManager::inst();

    am->createAll([this](const ActionManager::Action *aa) -> QAction * {
        if (aa->isRecentFiles()) {
            return (new RecentMenu(this))->menuAction();
        } else if (aa->isMenu()) {
            auto m = new QMenu(this);
            return m->menuAction();
        } else if (aa->isUndo()) {
            return Application::inst()->undoGroup()->createUndoAction(this);
        } else if (aa->isRedo()) {
            return Application::inst()->undoGroup()->createRedoAction(this);
        } else if (aa->name() == QByteArray("view_toolbar")) {
            return m_toolbar->toggleViewAction();
        } else {
            return new QAction(this);
        }
    });

    ActionManager::ActionTable actionTable = {
        { "go_home", [this](bool b) { goHome(b); } },
        { "document_save_all", [this](auto) {
              auto oldView = m_activeView;
              auto oldViewPane = m_activeViewPane;
              const auto docs = DocumentList::inst()->documents();

              for (const auto doc : docs) {
                  if (doc->model()->isModified()) {
                      if (doc->filePath().isEmpty()) {
                          QVector<ViewPane *> possibleVPs;
                          forEachViewPane([doc, &possibleVPs](ViewPane *vp) {
                              if (vp->viewForDocument(doc))
                                  possibleVPs << vp;
                              return false;
                          });
                          if (!possibleVPs.contains(m_activeViewPane)) {
                              Q_ASSERT(!possibleVPs.isEmpty());
                              setActiveViewPane(possibleVPs.constFirst());
                          }
                          m_activeViewPane->activateDocument(doc);
                      }

                      doc->save(false /* saveas */);

                      if (m_activeViewPane != oldViewPane) {
                          setActiveViewPane(oldViewPane);
                      }
                  }
              }
              if (m_activeView != oldView)
                  setActiveView(oldView);
        } },
        { "document_import_bl_inv", [this](auto) {
              if (!m_importinventory_dialog)
                  m_importinventory_dialog = new ImportInventoryDialog(this);
              m_importinventory_dialog->show();
          } },
        { "document_import_bl_order", [this](auto) -> QCoro::Task<> {
              if (!co_await Application::inst()->checkBrickLinkLogin())
                  co_return;
              if (!m_importorder_dialog)
                  m_importorder_dialog = new ImportOrderDialog(this);
              m_importorder_dialog->show();
          } },
        { "document_import_bl_cart", [this](auto) -> QCoro::Task<> {
              if (!co_await Application::inst()->checkBrickLinkLogin())
                  co_return;

              if (!m_importcart_dialog)
                  m_importcart_dialog = new ImportCartDialog(this);
              m_importcart_dialog->show();
          } },
        { "application_exit", [this](auto) { close(); } },
        { "edit_additems", [this](auto) {
              if (!m_add_dialog) {
                  m_add_dialog = new AddItemDialog();
                  m_add_dialog->setObjectName("additems"_l1);
                  m_add_dialog->attach(m_activeView);

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
          } },
        { "view_fullscreen", [this](bool fullScreen) {
              setWindowState(windowState().setFlag(Qt::WindowFullScreen, fullScreen));
          } },
        { "update_database", [](auto) { Application::inst()->updateDatabase(); } },
        { "help_about", [this](auto) { AboutDialog(this).exec(); } },
        { "help_systeminfo", [this](auto) { SystemInfoDialog(this).exec(); } },
        { "check_for_updates", [this](auto) { m_checkForUpdates->check(false /*not silent*/); } },
        { "view_filter", [this](auto) {
              if (m_activeViewPane)
                  m_activeViewPane->focusFilter();
          } },
        { "view_column_layout_manage", [this](auto) {
              ManageColumnLayoutsDialog d(this);
              d.exec();
          } },
        { "reload_scripts", [](auto) { ScriptManager::inst()->reload(); } },
    };

    am->connectActionTable(actionTable);

    m_goHome = am->qAction("go_home");
    m_goHome->setEnabled(false);
    am->qAction("edit_additems")->setShortcutContext(Qt::ApplicationShortcut);

    m_progress = new ProgressCircle();
    m_progress->setIcon(QIcon(":/assets/generated-app-icons/brickstore.png"_l1));
    m_progress->setColor("#4ba2d8");

    connect(m_progress, &ProgressCircle::cancelAll,
            BrickLink::core(), &BrickLink::Core::cancelTransfers);

    m_progressAction = new QWidgetAction(this);
    m_progressAction->setDefaultWidget(m_progress);
}

QList<QAction *> MainWindow::contextMenuActions() const
{
    static const char *contextActions[] =  {
        "edit_cut",
        "edit_copy",
        "edit_paste",
        "edit_paste_silent",
        "edit_duplicate",
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
        "bricklink_catalog",
        "bricklink_priceguide",
        "bricklink_lotsforsale",
        "bricklink_myinventory"
    };
    static QList<QAction *> actions;
    if (actions.isEmpty()) {
        for (const auto &an : contextActions)
            actions << ((an == QByteArray("-")) ? nullptr : ActionManager::inst()->qAction(an));
    }

    QList<QAction *> result = actions;
    if (!m_extensionContextActions.isEmpty()) {
        result.append(nullptr);
        result.append(m_extensionContextActions);
    }

    return result;
}

QStringList MainWindow::toolBarActionNames() const
{
    const QStringList actionNames = Config::inst()->toolBarActions();
    return actionNames.isEmpty() ? defaultToolBarActionNames() : actionNames;
}

QStringList MainWindow::defaultToolBarActionNames() const
{
    static const QStringList actionNames = {
        "document_new"_l1,
        "document_open"_l1,
        "document_save"_l1,
        "document_print"_l1,
        "-"_l1,
        "document_import"_l1,
        "document_export"_l1,
        "-"_l1,
        "edit_undo"_l1,
        "edit_redo"_l1,
        "-"_l1,
        "edit_cut"_l1,
        "edit_copy"_l1,
        "edit_paste"_l1,
        "edit_delete"_l1,
        "-"_l1,
        "edit_additems"_l1,
        "edit_subtractitems"_l1,
        "edit_mergeitems"_l1,
        "edit_partoutitems"_l1,
        "-"_l1,
        "edit_price_to_priceguide"_l1,
        "edit_price_inc_dec"_l1,
        "-"_l1,
        "view_column_layout_load"_l1,
    };
    return actionNames;
}

View *MainWindow::activeView() const
{
    return m_activeView;
}

void MainWindow::setActiveView(View *view)
{
    if (!m_activeViewPane || !view || !view->document())
        return;
    m_activeViewPane->activateDocument(view->document());
}

void MainWindow::connectView(View *view)
{
    if (view && view == m_activeView)
        return;

    if (m_activeView) {
        Document *document = m_activeView->document();

        m_activeView->setActive(false);

        disconnect(m_activeView.data(), &View::windowTitleChanged,
                   this, &MainWindow::titleUpdate);
        disconnect(document, &Document::modificationChanged,
                   this, &QWidget::setWindowModified);
        disconnect(document, &Document::blockingOperationActiveChanged,
                   this, &MainWindow::blockUpdate);
        disconnect(m_loadColumnLayoutMenu, &LoadColumnLayoutMenuAdapter::load,
                   document, &Document::setColumnLayoutFromId);

        m_activeView = nullptr;
    }

    if (view) {
        Document *document = view->document();

        view->setActive(true);

        connect(view, &View::windowTitleChanged,
                this, &MainWindow::titleUpdate);
        connect(document, &Document::modificationChanged,
                this, &QWidget::setWindowModified);
        connect(document, &Document::blockingOperationActiveChanged,
                this, &MainWindow::blockUpdate);
        connect(m_loadColumnLayoutMenu, &LoadColumnLayoutMenuAdapter::load,
                document, &Document::setColumnLayoutFromId);

        m_activeView = view;
    }
    ActionManager::inst()->setActiveDocument(m_activeView ? m_activeView->document() : nullptr);

    if (m_add_dialog) {
        m_add_dialog->attach(m_activeView);
        if (!m_activeView)
            m_add_dialog->close();
    }

    blockUpdate(m_activeView ? m_activeView->document()->isBlockingOperationActive() : false);
    titleUpdate();
    setWindowModified(m_activeView ? m_activeView->document()->model()->isModified() : false);

    emit documentActivated(m_activeView ? m_activeView->document() : nullptr);
}

QMenu *MainWindow::createPopupMenu()
{
    auto menu = QMainWindow::createPopupMenu();
    if (menu) {
        menu->addAction(tr("Customize Toolbar..."), this, [this]() {
            showSettings("toolbar"_l1);
        });
    }
    return menu;
}

void MainWindow::blockUpdate(bool blocked)
{
    static QUndoStack blockStack;

    if (m_activeView)
        Application::inst()->undoGroup()->setActiveStack(blocked ? &blockStack : m_activeView->model()->undoStack());
}

void MainWindow::titleUpdate()
{
    QString title = QApplication::applicationName();
    QString file;

    if (m_activeView) {
        title = m_activeView->windowTitle() % u" \u2014 " % title;
        file = m_activeView->document()->filePath();
    }
    setWindowTitle(title);
    setWindowFilePath(file);
}

void MainWindow::transferProgressUpdate(int p, int t)
{
    if (!m_progress)
        return;

    if (p == t) {
        m_progress->reset();
    } else {
        if (m_progress->maximum() != t)
            m_progress->setMaximum(t);
        m_progress->setValue(p);
    }
}

void MainWindow::showSettings(const QString &page)
{
    SettingsDialog d(page, this);
    d.exec();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    //TODO move to Application and get rid of QCoro::waitFor
    QStringList files = DocumentList::inst()->allFiles();
    Config::inst()->setValue("/MainWindow/LastSessionDocuments"_l1, files);

    if (!QCoro::waitFor(closeAllViews())) {
        e->ignore();
        return;
    }
    e->accept();
    QMainWindow::closeEvent(e);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    repositionHomeWidget();
}


QCoro::Task<bool> MainWindow::closeAllViews()
{
    auto oldView = m_activeView;
    auto oldViewPane = m_activeViewPane;
    const auto docs = DocumentList::inst()->documents();

    for (const auto doc : docs) {
        if (doc->model()->isModified()) {
            // bring a View of the doc to the front, preferably in the active ViewPane

            QHash<ViewPane *, View *> allViews;
            forEachViewPane([doc, &allViews](ViewPane *vp) {
                if (auto *view = vp->viewForDocument(doc))
                    allViews.insert(vp, view);
                return false;
            });
            Q_ASSERT(!allViews.isEmpty());

            if (!allViews.contains(m_activeViewPane))
                setActiveViewPane(*allViews.keyBegin());
            m_activeViewPane->activateDocument(doc);
        }
        if (!co_await doc->requestClose())
            co_return false;
    }

    delete m_add_dialog;
    delete m_importinventory_dialog;
    delete m_importorder_dialog;
    delete m_importcart_dialog;

    co_return true;
}


#include "moc_mainwindow.cpp"
#include "moc_mainwindow_p.cpp"
