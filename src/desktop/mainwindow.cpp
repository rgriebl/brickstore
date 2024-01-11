// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <array>

#include <QUndoStack>
#include <QAction>
#include <QCloseEvent>
#include <QMetaObject>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>
#include <QLabel>
#include <QToolButton>
#include <QDockWidget>
#include <QFont>
#include <QStyle>
#include <QWidgetAction>
#include <QSplitter>
#include <QListWidget>
#if defined(Q_OS_WINDOWS)
#  include <qtwinextras/qwintaskbarbutton.h>
#  include <qtwinextras/qwintaskbarprogress.h>
#endif

#include <QCoro/QCoroSignal>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/announcements.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentio.h"
#include "common/documentmodel.h"
#include "common/eventfilter.h"
#include "common/onlinestate.h"
#include "common/script.h"
#include "common/scriptmanager.h"
#include "common/systeminfo.h"
#include "common/uihelpers.h"
#include "common/undo.h"
#include "utility/exception.h"

#include "checkforupdates.h"
#include "desktopapplication.h"
#include "desktopuihelpers.h"
#include "developerconsole.h"
#include "mainwindow.h"
#include "mainwindow_p.h"
#include "progresscircle.h"
#include "rendersettingsdialog.h"
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
#include "importwantedlistdialog.h"
#include "managecolumnlayoutsdialog.h"
#include "settingsdialog.h"
#include "systeminfodialog.h"
#include "consolidatedialog.h"


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

void MainWindow::show()
{
    QMetaObject::invokeMethod(this, [this]() {
        bool doNotRestoreGeometry = false;

        if (SystemInfo::inst()->value(u"windows.tabletmode"_qs).toBool()) {
            // in tablet mode, the window is auto-maximized, but the geometry restore prevents that
            // (as in: we end up in a weird state)
            doNotRestoreGeometry = true;

            // if we don't do this, we get force maximized by Windows after showing and this messes
            // up the dock widget layout
            setWindowState(Qt::WindowMaximized);
        }

        auto geo = Config::inst()->value(u"MainWindow/Layout/Geometry"_qs).toByteArray();
        if (!doNotRestoreGeometry)
            restoreGeometry(geo);

        auto state = Config::inst()->value(u"MainWindow/Layout/State"_qs).toByteArray();
        if (state.isEmpty() || !restoreState(state, DockStateVersion))
            m_toolbar->show();

        QMainWindow::show();

#if defined(Q_OS_MACOS)
        MainWindow::inst()->raise();
#endif
    }, Qt::QueuedConnection);
}

QCoro::Task<> MainWindow::shutdown()
{
    QStringList files = DocumentList::inst()->allFiles();

    if (co_await Application::inst()->closeAllDocuments()) {
        Config::inst()->setValue(u"MainWindow/LastSessionDocuments"_qs, files);
        Config::inst()->setValue(u"MainWindow/Filter"_qs, m_favoriteFilters->stringList());

#if defined(Q_OS_MACOS)
        // from QtCreator:
        // On OS X applications usually do not restore their full screen state.
        // To be able to restore the correct non-full screen geometry, we have to put
        // the window out of full screen before saving the geometry.
        // Works around QTBUG-45241

        if (isFullScreen())
            setWindowState(windowState() & ~Qt::WindowFullScreen);
#endif

        Config::inst()->setValue(u"MainWindow/Layout/Geometry"_qs, saveGeometry());
        Config::inst()->setValue(u"MainWindow/Layout/State"_qs, saveState(DockStateVersion));

        qApp->exit();
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_favoriteFilters(new QStringListModel(this))
{
    s_inst = this;

    DesktopUIHelpers::setDefaultParent(this);

    m_progress = nullptr;

    // keep QTBUG-39781 in mind: we cannot use QOpenGLWidget directly
    setUnifiedTitleAndToolBarOnMac(true);

    setDocumentMode(true);
    setAcceptDrops(true);

    connect(Application::inst(), &Application::showSettings,
            this, &MainWindow::showSettings);
    connect(Application::inst(), &Application::show3DSettings,
            this, &MainWindow::show3DSettings);
    connect(Application::inst(), &Application::showDeveloperConsole,
            this, [this]() {
        if (auto *dc = DesktopApplication::inst()->developerConsole()) {
            if (dc->parentWidget() != this)
                dc->setParent(this, dc->windowFlags());
            if (dc->isVisible()) {
                dc->raise();
                dc->activateWindow();
            } else {
                dc->show();
            }
        }
    });

    // vvv increase DockStateVersion if you change the dock/toolbar setup

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName(u"toolbar"_qs);
    m_toolbar->setMovable(false);

    connect(m_toolbar, &QToolBar::visibilityChanged,
            this, [this]() {
        // we may get this callback after ~MainWindow
        if (!qobject_cast<MainWindow *>(this)) // clazy:exclude=unneeded-cast
            return;
        if (m_welcomeWidget->isVisibleTo(this))
            repositionHomeWidget();
    });

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

    m_favoriteFilters->setStringList(Config::inst()->value(u"MainWindow/Filter"_qs).toStringList());

    connectView(nullptr);

    m_defaultDockState = saveState();

#if defined(Q_OS_WINDOWS)
    new EventFilter(this, { QEvent::Show }, [this](QObject *, QEvent *) -> EventFilter::Result {
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
        return EventFilter::ContinueEventProcessing | EventFilter::DeleteEventFilter;
    });
#endif

    menuBar()->show();

    ActionManager::inst()->qAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

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

    DocumentModel::setConsolidateFunction([](DocumentModel *model, QVector<DocumentModel::Consolidate> &list, bool addItems) -> QCoro::Task<bool> {
        auto *doc = DocumentList::inst()->documentForModel(model);

        if (!doc || !MainWindow::inst())
            co_return false;

        emit doc->requestActivation();

        auto *view = MainWindow::inst()->activeView();
        if (!view || (view->document() != doc))
            co_return false;

        ConsolidateDialog dlg(view, list, addItems);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();

        co_return (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted);
    });

    goHome(true);
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
                        UIHelpers::warning(e.errorString());
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
        for (QAction *a : std::as_const(actions)) {
            if (a->objectName() == u"scripts-start")
                deleteRest = true;
            else if (a->objectName() == u"scripts-end")
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

    for (QDockWidget *dock : std::as_const(m_dock_widgets)) {
        QString name = dock->objectName();

        if (name == u"dock_info")
            dock->setWindowTitle(tr("Info"));
        if (name == u"dock_priceguide")
            dock->setWindowTitle(tr("Price Guide"));
        if (name == u"dock_appearsin")
            dock->setWindowTitle(tr("Item Inventory"));
        if (name == u"dock_opendocuments")
            dock->setWindowTitle(tr("Open Documents"));
        if (name == u"dock_recentdocuments")
            dock->setWindowTitle(tr("Recent Documents"));
    }
    if (m_progress) {
        m_progress->setToolTipTemplates(tr("Offline"),
                                        tr("No outstanding jobs"),
                                        tr("Downloading...<br><b>%p%</b> finished<br>(%v of %m)"));
    }
}

MainWindow::~MainWindow()
{
    closeAllDialogs();

    s_inst = reinterpret_cast<MainWindow *>(-1);

    // we have to break a connection loop here to prevent a crash: if we didn't do this, then
    // the ViewPane's d'tors would be called AFTER "this" is not a MainWindow anymore, but the
    // deletedViewPane() callback wants to update members of "this".
    for (const auto *vp : std::as_const(m_allViewPanes))
        vp->disconnect();
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
    const auto urls = e->mimeData()->urls();
    for (const QUrl &u : urls)
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
    auto *rootSplitter = new QSplitter();

    rootSplitter->setObjectName(u"RootSplitter"_qs);
    auto *vp = createViewPane(nullptr, this);
    rootSplitter->addWidget(vp);
    setCentralWidget(rootSplitter);
    setActiveViewPane(vp);

    connect(qApp, &QApplication::focusChanged,
            this, [this](QWidget *, QWidget *now) {
        for ( ; now; now = now->parentWidget()) {
            if (auto vpl = qobject_cast<ViewPane *>(now)) {
                if (vpl->activeView()) {
                    if (m_goHome->isChecked())
                        goHome(false);
                    setActiveViewPane(vpl);
                    break;
                }
            }
        }
    });

    m_welcomeWidget = new WelcomeWidget(this);
    m_welcomeWidget->setAutoFillBackground(true);
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

ViewPane *MainWindow::createViewPane(Document *activeDocument, QWidget *window)
{
    Q_ASSERT(window);

    auto vp = new ViewPane([this](Document *doc, QWidget *win) { return createViewPane(doc, win); },
                           activeDocument);

    vp->setFilterFavoritesModel(m_favoriteFilters);

    connect(vp, &ViewPane::viewActivated,
            this, [this, vp](View *view) {
        setActiveViewPane(vp);
        connectView(view);
    });

    m_allViewPanes.insert(window, vp);

    // QObject destroyed is too late, because ViewPane is just an QObject at this point then
    connect(vp, &ViewPane::beingDestroyed,
            this, [this, window, vp]() {

        // vp is just a QObject at this point!
        m_allViewPanes.remove(window, vp);

        if (m_activeViewPane == vp) {
            // try the current window first
            ViewPane *newActive = m_allViewPanes.value(window);

            if (!newActive) {
                // we need to find another pane to transfer the active state
                for (const auto &check : std::as_const(m_allViewPanes)) {
                    if (check != vp) {
                        newActive = check;
                        break;
                    }
                }
            }
            setActiveViewPane(newActive);
        }
    });

    setActiveViewPane(vp);
    return vp;
}

QDockWidget *MainWindow::createDock(QWidget *widget, const char *name)
{
    auto *dock = new QDockWidget(QString(), this);
    dock->setObjectName(QString::fromLatin1(name));
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
        m_welcomeWidget->fadeIn();
        m_welcomeWidget->setFocus();
        connectView(nullptr);
    } else {
        m_welcomeWidget->fadeOut();
        if (View *v = m_activeViewPane ? m_activeViewPane->activeView() : nullptr) {
            v->setFocus();
            connectView(v);
        }
    }
    m_goHome->setIcon(QIcon::fromTheme(home ? u"go-previous"_qs : u"go-home"_qs));
    m_goHome->setChecked(home);
    m_goHome->setEnabled(DocumentList::inst()->count() > 0);
}

void MainWindow::repositionHomeWidget()
{
    QRect r = rect();
    if (menuBar() && menuBar()->isVisibleTo(this))
        r.setTop(r.top() + menuBar()->height());
    if (m_toolbar->isVisibleTo(this))
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
        for (QDockWidget *dock : std::as_const(m_dock_widgets))
            viewDocksMenu->addAction(dock->toggleViewAction());
    });

    setupMenu("document_import", {
                  "document_import_bl_inv",
                  "document_import_bl_store_inv",
                  "document_import_bl_order",
                  "document_import_bl_cart",
                  "document_import_bl_wanted",
                  "document_import_ldraw_model",
                  "document_import_bl_xml",
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
                  "-",
                  "edit_tierprice_relative",
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
                  "view_row_height_inc",
                  "view_row_height_dec",
                  "view_row_height_reset",
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
                                 "3d_settings",
                                 "-",
                                 "developer_console",
                                 "-",
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

    actionNames = QStringList { u"go_home"_qs, u"-"_qs } + actionNames
            + QStringList { u"<>"_qs, u"widget_progress"_qs, u"|"_qs };

    for (const QString &an : std::as_const(actionNames)) {
        if (an == u"-") {
            m_toolbar->addSeparator()->setObjectName(an);
        } else if (an == u"|") {
            auto *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = 2 * style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setFixedSize(sp, sp);
            m_toolbar->addWidget(spacer);
        } else if (an == u"<>") {
            auto *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = 2 * style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setMinimumSize(sp, sp);
            spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            m_toolbar->addWidget(spacer);
        } else if (an == u"edit_filter_focus") {
            // the old filter control - ignore
        } else if (an == u"widget_progress") {
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

    auto dockInfo = createDock(new TaskInfoWidget(this), "dock_info");
    auto dockOpen = createDock(new TaskOpenDocumentsWidget(this), "dock_opendocuments");
    auto dockRecent = createDock(new TaskRecentDocumentsWidget(this), "dock_recentdocuments");
    auto dockInventory = createDock(new TaskInventoryWidget(this), "dock_appearsin");
    auto dockPriceGuide = createDock(new TaskPriceGuideWidget(this), "dock_priceguide");

    addDockWidget(Qt::LeftDockWidgetArea, dockInfo);
    addDockWidget(Qt::LeftDockWidgetArea, dockOpen);
    addDockWidget(Qt::LeftDockWidgetArea, dockRecent);
    addDockWidget(Qt::LeftDockWidgetArea, dockInventory);
    addDockWidget(Qt::LeftDockWidgetArea, dockPriceGuide);
    tabifyDockWidget(dockRecent, dockOpen);
    tabifyDockWidget(dockOpen, dockInfo);
    tabifyDockWidget(dockInventory, dockPriceGuide);
}

QMenu *MainWindow::setupMenu(const QByteArray &name, const QVector<QByteArray> &a_names)
{
    if (a_names.isEmpty())
        return nullptr;

    QAction *menuAction = ActionManager::inst()->qAction(name);
    QMenu *menu = menuAction ? menuAction->menu() : nullptr;
    if (!menuAction || !menu)
        return nullptr;

    for (const QByteArray &an : a_names) {
        if (an.startsWith("-")) {
            auto *a = menu->addSeparator();
            if (an.length() > 1)
                a->setObjectName(QString::fromLatin1(an.mid(1)));
        } else if (QAction *a = ActionManager::inst()->qAction(an.constData())) {
            menu->addAction(a);
        } else  {
            qWarning("Couldn't find action '%s' when creating menu '%s'",
                     an.constData(), name.constData());
        }
    }
    return menu;
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
            return UndoAction::create(UndoAction::Undo, Application::inst()->undoGroup(), this);
        } else if (aa->isRedo()) {
            return UndoAction::create(UndoAction::Redo, Application::inst()->undoGroup(), this);
        } else if (aa->name() == QByteArray("view_toolbar")) {
            return m_toolbar->toggleViewAction();
        } else {
            return new QAction(this);
        }
    });

    ActionManager::ActionTable actionTable = {
        { "go_home", [this](bool b) { goHome(b); } },
        { "document_save_all", [this](bool) {
              auto oldView = m_activeView;
              auto oldViewPane = m_activeViewPane;
              const auto docs = DocumentList::inst()->documents();

              for (const auto doc : docs) {
                  if (doc->model()->isModified()) {
                      if (doc->filePath().isEmpty()) {
                          QVector<ViewPane *> possibleVPs;
                          for (const auto &vp : std::as_const(m_allViewPanes)) {
                              if (vp->viewForDocument(doc))
                                  possibleVPs << vp;
                          }
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
        { "document_import_bl_inv", [this](bool) {
              if (!m_importinventory_dialog)
                  m_importinventory_dialog = new ImportInventoryDialog(this);
              m_importinventory_dialog->show();
          } },
        { "document_import_bl_order", [this](bool) -> QCoro::Task<> {
              if (!co_await Application::inst()->checkBrickLinkLogin())
                  co_return;

              if (!m_importorder_dialog)
                  m_importorder_dialog = new ImportOrderDialog(this);
              m_importorder_dialog->show();
          } },
        { "document_import_bl_cart", [this](bool) -> QCoro::Task<> {
              if (!co_await Application::inst()->checkBrickLinkLogin())
                  co_return;

              if (!m_importcart_dialog)
                  m_importcart_dialog = new ImportCartDialog(this);
              m_importcart_dialog->show();
          } },
        { "document_import_bl_wanted", [this](bool) -> QCoro::Task<> {
              if (!co_await Application::inst()->checkBrickLinkLogin())
                  co_return;

              if (!m_importwanted_dialog)
                  m_importwanted_dialog = new ImportWantedListDialog(this);
              m_importwanted_dialog->show();
          } },
        { "application_exit", [this](bool) { close(); } },
        { "edit_additems", [this](bool) {
              showAddItemDialog();
          } },
        { "view_fullscreen", [this](bool fullScreen) {
              setWindowState(windowState().setFlag(Qt::WindowFullScreen, fullScreen));
          } },
        { "view_row_height_inc", [](bool) {
              // round up to 10% and add 10%
              Config::inst()->setRowHeightPercent((((Config::inst()->rowHeightPercent() + 5) / 10) + 1) * 10);
          } },
        { "view_row_height_dec", [](bool) {
              // round down to 10% and subtract 10%
              Config::inst()->setRowHeightPercent((((Config::inst()->rowHeightPercent()) / 10) - 1) * 10);
          } },
        { "view_row_height_reset", [](bool) {
              Config::inst()->setRowHeightPercent(100);
          } },
        { "update_database", [](bool) { Application::inst()->updateDatabase(); } },
        { "help_about", [this](bool) {
              auto *dlg = new AboutDialog(this);
              dlg->setWindowModality(Qt::ApplicationModal);
              dlg->setAttribute(Qt::WA_DeleteOnClose);
              dlg->show();
          } },
        { "help_systeminfo", [this](bool) {
              auto *dlg = new SystemInfoDialog(this);
              dlg->setWindowModality(Qt::ApplicationModal);
              dlg->setAttribute(Qt::WA_DeleteOnClose);
              dlg->show();
          } },
        { "check_for_updates", [this](bool) { m_checkForUpdates->check(false /*not silent*/); } },
        { "view_filter", [this](bool) {
              if (m_activeViewPane)
                  m_activeViewPane->focusFilter();
          } },
        { "view_column_layout_manage", [this](bool) {
              auto *dlg = new ManageColumnLayoutsDialog(this);
              dlg->setWindowModality(Qt::ApplicationModal);
              dlg->setAttribute(Qt::WA_DeleteOnClose);
              dlg->show();
          } },
        { "reload_scripts", [](bool) { ScriptManager::inst()->reload(); } },
    };

    am->connectActionTable(actionTable);

    m_goHome = am->qAction("go_home");
    m_goHome->setEnabled(false);
    am->qAction("edit_additems")->setShortcutContext(Qt::ApplicationShortcut);

    m_progress = new ProgressCircle();
    m_progress->setIcon(QIcon(u":/assets/generated-app-icons/brickstore.png"_qs));
    m_progress->setColor({ 0x4b, 0xa2, 0xd8 });

    connect(m_progress, &ProgressCircle::cancelAll,
            BrickLink::core(), &BrickLink::Core::cancelTransfers);

    m_progressAction = new QWidgetAction(this);
    m_progressAction->setDefaultWidget(m_progress);
}

QList<QAction *> MainWindow::contextMenuActions() const
{
    static const std::array contextActions =  {
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
        actions.reserve(contextActions.size());
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
        u"document_new"_qs,
        u"document_open"_qs,
        u"document_save"_qs,
        u"document_print"_qs,
        u"-"_qs,
        u"document_import"_qs,
        u"document_export"_qs,
        u"-"_qs,
        u"edit_undo"_qs,
        u"edit_redo"_qs,
        u"-"_qs,
        u"edit_cut"_qs,
        u"edit_copy"_qs,
        u"edit_paste"_qs,
        u"edit_delete"_qs,
        u"-"_qs,
        u"edit_additems"_qs,
        u"edit_subtractitems"_qs,
        u"edit_mergeitems"_qs,
        u"edit_partoutitems"_qs,
        u"-"_qs,
        u"edit_price_to_priceguide"_qs,
        u"edit_price_inc_dec"_qs,
        u"-"_qs,
        u"view_column_layout_load"_qs,
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
        disconnect(document->model(), &DocumentModel::modificationChanged,
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
        connect(document->model(), &DocumentModel::modificationChanged,
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
            showSettings(u"toolbar"_qs);
        });
        menu->addSeparator();
        menu->addAction(tr("Reset Info Docks layout"), this, [this]() {
            restoreState(m_defaultDockState);
            // somehow the width ends up being too wide, so we try to squeeze the dock
            resizeDocks({ m_dock_widgets.at(0) }, { 20 }, Qt::Horizontal);
        });
    }
    return menu;
}

void MainWindow::showAddItemDialog(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if (!m_add_dialog) {
        m_add_dialog = new AddItemDialog();
        m_add_dialog->setObjectName(u"additems"_qs);
        m_add_dialog->attach(m_activeView);

        connect(m_add_dialog, &AddItemDialog::closed,
                this, [this]() {
            raise();
            activateWindow();
        });
    }

    if (!m_add_dialog->isVisible())
        m_add_dialog->show();
    if (m_add_dialog->isMinimized())
        m_add_dialog->setWindowState(m_add_dialog->windowState() & ~Qt::WindowMinimized);
    m_add_dialog->raise();
    m_add_dialog->activateWindow();

    if (item) {
        QMetaObject::invokeMethod(m_add_dialog.get(), [this, item, color]() {
            if (m_add_dialog)
                m_add_dialog->goToItem(item, color);
        }, Qt::QueuedConnection);
    }
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
        title = m_activeView->windowTitle() + u" \u2014 " + title;
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
    auto *dlg = new SettingsDialog(page, this);
    dlg->setWindowModality(Qt::ApplicationModal);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::show3DSettings()
{
    RenderSettingsDialog::inst()->show();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    e->ignore();
    QMetaObject::invokeMethod(this, &MainWindow::shutdown, Qt::QueuedConnection);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    repositionHomeWidget();
    QMainWindow::resizeEvent(e);
}

void MainWindow::closeAllDialogs()
{
    delete m_add_dialog;
    delete m_importinventory_dialog;
    delete m_importorder_dialog;
    delete m_importcart_dialog;
    delete m_importwanted_dialog;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QWidget *UndoAction::createWidget(QWidget *parent)
{
    if (auto *tb = qobject_cast<QToolBar *>(parent)) {
        // straight from QToolBar
        auto *button = new QToolButton(tb);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(tb->iconSize());
        button->setToolButtonStyle(tb->toolButtonStyle());
        connect(tb, &QToolBar::iconSizeChanged,
                button, &QToolButton::setIconSize);
        connect(tb, &QToolBar::toolButtonStyleChanged,
                button, &QToolButton::setToolButtonStyle);
        button->setDefaultAction(this);

        auto menu = new QMenu(button);
        button->setMenu(menu);
        button->setPopupMode(QToolButton::MenuButtonPopup);

        // we need a margin - otherwise the list will paint over the menu's border
        auto *w = new QWidget();
        auto *l = new QVBoxLayout(w);
        int fw = menu->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        l->setContentsMargins(fw, fw, fw, fw);

        auto list = new QListWidget();
        list->setFrameShape(QFrame::NoFrame);
        list->setSelectionMode(QAbstractItemView::MultiSelection);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setMouseTracking(true);
        list->setResizeMode(QListView::Adjust);
        auto pal = QApplication::palette(menu);
        list->setPalette(pal);

        auto label = new QLabel();
        label->setAlignment(Qt::AlignCenter);
        label->setPalette(pal);

        l->addWidget(list);
        l->addWidget(label);
        auto *widgetaction = new QWidgetAction(this);
        widgetaction->setDefaultWidget(w);
        menu->addAction(widgetaction);

        menu->setFocusProxy(list);

        connect(list, &QListWidget::itemEntered, this, [list](QListWidgetItem *item) {
            if (list->currentItem() != item)
                list->setCurrentItem(item);
        });

        connect(list, &QListWidget::currentItemChanged, this, [this, list, label](QListWidgetItem *item) {
            if (item) {
                int currentIndex = list->row(item);
                for (int i = 0; i < list->count(); i++)
                    list->item(i)->setSelected(i <= currentIndex);
                label->setText(listLabel(currentIndex + 1));
            }
        });

        connect(list, &QListWidget::itemClicked, list, &QListWidget::itemActivated);
        connect(list, &QListWidget::itemActivated, this, [this, list, menu](QListWidgetItem *item) {
            if (item) {
                menu->close();
                emit multipleTriggered(list->row(item) + 1);
            }
        });

        connect(menu, &QMenu::aboutToShow, this, [this, list, label]() {
            list->clear();

            if (m_undoStack) {
                int maxw = 0;
                QFontMetrics fm = list->fontMetrics();
                const auto style = list->style();

                const QStringList dl = descriptionList(m_undoStack);
                for (const auto &desc : dl) {
                    (void) new QListWidgetItem(desc, list);
                    maxw = std::max(maxw, fm.horizontalAdvance(desc));
                }

                list->setMinimumWidth(maxw
                                        + style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, list)
                                        + 2 * style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, list)
                                        + 2 * (style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, list) + 1));

                if (list->count()) {
                    list->setCurrentItem(list->item(0));
                    list->setFocus();
                }
            }

            // we cannot resize the menu dynamically, so we need to set a minimum width now
            const auto fm = label->fontMetrics();
            label->setMinimumWidth(std::max(fm.horizontalAdvance(listLabel(1)),
                                              fm.horizontalAdvance(listLabel(100000)))
                                     + 2 * label->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
        });

        new EventFilter(button, { QEvent::LanguageChange }, [this](QObject *, QEvent *) {
            if (m_undoStack)
                setDescription(m_type == Undo ? m_undoStack->undoText() : m_undoStack->redoText());
            return EventFilter::ContinueEventProcessing;
        });
        return button;
    }
    return nullptr;
}



#include "moc_mainwindow.cpp"
#include "moc_mainwindow_p.cpp"
