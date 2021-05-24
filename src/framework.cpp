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
#if defined(Q_OS_WINDOWS)
#  include <QWinTaskbarButton>
#  include <QWinTaskbarProgress>
#  include <QtPlatformHeaders/QWindowsWindowFunctions>
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
#include "systeminfodialog.h"
#include "managecolumnlayoutsdialog.h"
#include "framework.h"
#include "stopwatch.h"
#include "importinventorydialog.h"
#include "importorderdialog.h"
#include "importcartdialog.h"
#include "historylineedit.h"
#include "checkforupdates.h"
#include "announcements.h"

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
    NeedLots         = 0x0040,
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
                    s.prepend(QString("&%1   "_l1).arg(cnt));
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

            for (auto cl : Window::columnLayoutCommands()) {
                addAction(Window::columnLayoutCommandName(cl))
                        ->setData(Window::columnLayoutCommandId(cl));
            }

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

        fontChange();
        paletteChange();
    }

protected:
    void changeEvent(QEvent *e) override
    {
        if (e->type() == QEvent::PaletteChange)
            paletteChange();
        else if (e->type() == QEvent::FontChange)
            fontChange();
        QLabel::changeEvent(e);
    }

    void fontChange()
    {
        QFont f = font();
        f.setBold(true);
        setFont(f);
    }

    void paletteChange()
    {
        QPalette p = palette();
        QPalette ivp = QApplication::palette("QAbstractItemView");
        QColor high = ivp.color(p.currentColorGroup(), QPalette::Highlight);
        QColor text = ivp.color(p.currentColorGroup(), QPalette::HighlightedText);
        QColor win = QApplication::palette(this).color(QPalette::Window);

        m_gradient.setStops({ { 0, high },
                              { .6, Utility::gradientColor(high, win, 0.5) },
                              { 1, win } });
        p.setBrush(QPalette::Window, m_gradient);
        p.setColor(QPalette::WindowText, text);
        setPalette(p);
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

    // keep QTBUG-39781 in mind: we cannot use QOpenGLWidget directly
    setUnifiedTitleAndToolBarOnMac(true);

    setDocumentMode(true);
    setAcceptDrops(true);

    m_undogroup = new UndoGroup(this);

    connect(Application::inst(), &Application::openDocument,
            this, &FrameWork::openDocument);

    m_activeWin = nullptr;

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
    m_task_info->setObjectName("TaskInfo"_l1);
    addDockWidget(Qt::LeftDockWidgetArea, createDock(m_task_info));

    m_task_appears = new TaskAppearsInWidget(nullptr);
    m_task_appears->setObjectName("TaskAppears"_l1);
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_appears), Qt::Vertical);

    m_task_priceguide = new TaskPriceGuideWidget(nullptr);
    m_task_priceguide->setObjectName("TaskPriceGuide"_l1);
    splitDockWidget(m_dock_widgets.first(), createDock(m_task_priceguide), Qt::Vertical);

    auto logDock = createDock(Application::inst()->logWidget());
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    logDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, logDock, Qt::Horizontal);

    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName("toolbar"_l1);
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
                                  }));

    menuBar()->addMenu(createMenu("menu_view", {
                                      "view_toolbar",
                                      "view_docks",
                                      "-",
                                      "view_fullscreen",
                                      "-",
                                      "view_show_input_errors",
                                      "-",
                                      "view_show_diff_indicators",
                                      "view_reset_diff_mode",
                                      "-",
                                      "view_column_layout_save",
                                      "view_column_layout_manage",
                                      "view_column_layout_load"
                                  }));

    m_extrasMenu = createMenu("menu_extras", {
                                  "update_database",
                                  "-",
                                  "configure",
                                  "-scripts-start",
                                  "-scripts-end",
                                  "reload_scripts",
                              });
    menuBar()->addMenu(m_extrasMenu);

    QMenu *m = m_workspace->windowMenu(true, this);
    m->menuAction()->setObjectName("menu_window"_l1);
    menuBar()->addMenu(m);

    menuBar()->addMenu(createMenu("menu_help", {
                                      "check_for_updates",
                                      "-",
                                      "help_reportbug",
                                      "help_announcements",
                                      "help_releasenotes",
                                      "-",
                                      "help_systeminfo",
                                      "-",
                                      "help_about"
                                  }));

    setupToolBar();
    addToolBar(m_toolbar);

    m_workspace->setWelcomeWidget(new WelcomeWidget());

    languageChange();


    connect(Application::inst(), &Application::onlineStateChanged,
            this, &FrameWork::onlineStateChanged);
    onlineStateChanged(Application::inst()->isOnline());

    findAction("view_show_input_errors")->setChecked(Config::inst()->showInputErrors());
    findAction("view_show_diff_indicators")->setChecked(Config::inst()->showDifferenceIndicators());

    connect(BrickLink::core(), &BrickLink::Core::transferProgress,
            this, &FrameWork::transferProgressUpdate);

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
    QWindowsWindowFunctions::setHasBorderInFullScreen(windowHandle(), true);
#endif

    QByteArray ba;

    ba = Config::inst()->value("/MainWindow/Layout/Geometry"_l1).toByteArray();
    restoreGeometry(ba);

    ba = Config::inst()->value("/MainWindow/Layout/State"_l1).toByteArray();
    if (ba.isEmpty() || !restoreState(ba, DockStateVersion))
        m_toolbar->show();

    if (m_filter) {
        ba = Config::inst()->value("/MainWindow/Filter"_l1).toByteArray();
        if (!ba.isEmpty()) {
            m_filter->restoreState(ba);
            m_filter->clear();
        }
        m_filter->setEnabled(false);
    }


    findAction("view_fullscreen")->setChecked(windowState() & Qt::WindowFullScreen);

    Currency::inst()->updateRates();
    auto *currencyUpdateTimer = new QTimer(this);
    currencyUpdateTimer->start(4h);
    currencyUpdateTimer->callOnTimeout(Currency::inst(),
                                       []() { Currency::inst()->updateRates(true /*silent*/); });

    m_checkForUpdates = new CheckForUpdates(Application::inst()->gitHubUrl(), this);
    m_announcements = new Announcements(Application::inst()->gitHubUrl(), this);
    connect(m_announcements, &Announcements::newAnnouncements,
            m_announcements, &Announcements::showNewAnnouncements);

    m_checkForUpdates->check(true /*silent*/);
    m_announcements->check();
    auto checkTimer = new QTimer(this);
    checkTimer->callOnTimeout(this, [=]() {
        m_checkForUpdates->check(true /*silent*/);
        m_announcements->check();
    });
    checkTimer->start(24h);

    setupScripts();

    if (!restoreWindowsFromAutosave()) {
        if (Config::inst()->restoreLastSession()) {
            const auto files = Config::inst()->value("/MainWindow/LastSessionDocuments"_l1).toStringList();
            for (const auto &file : files)
                openDocument(file);
        }
    }

    connect(Config::inst(), &Config::shortcutsChanged,
            this, &FrameWork::translateActions);
    connect(Config::inst(), &Config::toolBarActionsChanged,
            this, &FrameWork::setupToolBar);
}

void FrameWork::setupScripts()
{
    if (!ScriptManager::inst()->initialize(BrickLink::core())) {
        MessageBox::warning(nullptr, { }, tr("Could not initialize the JavaScript scripting environment."));
        return;
    }

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
                        MessageBox::warning(nullptr, { }, e.error());
                    }
                });
            }
            const auto printingActions = script->printingActions();
            for (auto printingAction : printingActions) {
                auto action = new QAction(printingAction->text(), printingAction);
                extrasActions.append(action);

                connect(action, &QAction::triggered, printingAction, [this, printingAction]() {
                    if (activeWindow())
                        activeWindow()->printScriptAction(printingAction);
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
            qWarning() << a->objectName() << deleteRest;
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

void FrameWork::languageChange()
{
    m_toolbar->setWindowTitle(tr("Toolbar"));

    foreach (QDockWidget *dock, m_dock_widgets) {
        QString name = dock->objectName();

        if (name == "Dock-TaskInfo"_l1)
            dock->setWindowTitle(tr("Info"));
        if (name == "Dock-TaskPriceGuide"_l1)
            dock->setWindowTitle(tr("Price Guide"));
        if (name == "Dock-TaskAppears"_l1)
            dock->setWindowTitle(tr("Appears In Sets"));
        if (name == "Dock-LogWidget"_l1)
            dock->setWindowTitle(tr("Error Log"));
    }
    if (m_filter) {
        m_filter->setPlaceholderText(tr("Filter"));
        // DocumentProxyModel hasn't received the LanguageChange event yet,
        // so we need to skip this event loop round
        QMetaObject::invokeMethod(this, [this]() {
            QString tt = findAction("edit_filter_focus")->toolTip();
            if (m_activeWin)
                tt.append(m_activeWin->document()->filterToolTip());
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
        { "document_save_as",               tr("Save As..."),                         QKeySequence::SaveAs },
        { "document_print",                 tr("Print..."),                           QKeySequence::Print },
        { "document_print_pdf",             tr("Print to PDF..."),                    },
        { "document_import",                tr("Import"),                             },
        { "document_import_bl_inv",         tr("BrickLink Set Inventory..."),         tr("Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "document_import_bl_xml",         tr("BrickLink XML..."),                   tr("Ctrl+I,Ctrl+X", "File|Import BrickLink XML") },
        { "document_import_bl_order",       tr("BrickLink Order..."),                 tr("Ctrl+I,Ctrl+O", "File|Import BrickLink Order") },
        { "document_import_bl_store_inv",   tr("BrickLink Store Inventory..."),       tr("Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory") },
        { "document_import_bl_cart",        tr("BrickLink Shopping Cart..."),         tr("Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart") },
        { "document_import_ldraw_model",    tr("LDraw or Studio Model..."),           tr("Ctrl+I,Ctrl+L", "File|Import LDraw Model") },
        { "document_export",                tr("Export"),                             },
        { "document_export_bl_xml",         tr("BrickLink XML..."),                         tr("Ctrl+E,Ctrl+X", "File|Import BrickLink XML") },
        { "document_export_bl_xml_clip",    tr("BrickLink Mass-Upload XML to Clipboard"),   tr("Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload") },
        { "document_export_bl_update_clip", tr("BrickLink Mass-Update XML to Clipboard"),   tr("Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update") },
        { "document_export_bl_invreq_clip", tr("BrickLink Set Inventory XML to Clipboard"), tr("Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory") },
        { "document_export_bl_wantedlist_clip", tr("BrickLink Wanted List XML to Clipboard"),   tr("Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List") },
        { "document_close",                 tr("Close"),                              QKeySequence::Close },
        { "application_exit",               tr("Exit"),                               QKeySequence::Quit },
        { "menu_edit",                      tr("&Edit"),                              },
        { "edit_undo",                      { },                                      QKeySequence::Undo },
        { "edit_redo",                      { },                                      QKeySequence::Redo },
        { "edit_cut",                       tr("Cut"),                                QKeySequence::Cut },
        { "edit_copy",                      tr("Copy"),                               QKeySequence::Copy },
        { "edit_paste",                     tr("Paste"),                              QKeySequence::Paste },
        { "edit_paste_silent",              tr("Paste silent"),                       tr("Ctrl+Shift+V", "Edit|Paste silent") },
        { "edit_duplicate",                 tr("Duplicate"),                          tr("Ctrl+D", "Edit|Duplicate") },
        { "edit_delete",                    tr("Delete"),                             tr("Del", "Edit|Delete") },
        // ^^ QKeySequence::Delete has Ctrl+D as secondary on macOS and Linux and this clashes with Duplicate
        { "edit_additems",                  tr("Add Items..."),                       tr("Insert", "Edit|AddItems") },
        { "edit_subtractitems",             tr("Subtract Items..."),                  },
        { "edit_mergeitems",                tr("Consolidate Items..."),               tr("Ctrl+L", "Edit|Consolidate Items") },
        { "edit_partoutitems",              tr("Part out Item..."),                   },
        { "edit_copy_fields",               tr("Copy values from document..."),       },
        { "edit_select_all",                tr("Select All"),                         QKeySequence::SelectAll },
        { "edit_select_none",               tr("Select None"),                        tr("Ctrl+Shift+A", "Edit|Select None") },
        // ^^ QKeySequence::Deselect is only mapped on Linux
        { "edit_filter_from_selection",     tr("Create a Filter from the Selection"), },
        { "edit_filter_focus",              tr("Filter the Item List"),               QKeySequence::Find },
        { "menu_view",                      tr("&View"),                              },
        { "view_toolbar",                   tr("View Toolbar"),                       },
        { "view_docks",                     tr("View Info Docks"),                    },
        { "view_fullscreen",                tr("Full Screen"),                        QKeySequence::FullScreen },
        { "view_show_input_errors",         tr("Show Input Errors"),                  },
        { "view_show_diff_indicators",      tr("Show difference mode indicators"),    },
        { "view_reset_diff_mode",           tr("Reset difference mode base values"),  },
        { "view_column_layout_save",        tr("Save Column Layout..."),              },
        { "view_column_layout_manage",      tr("Manage Column Layouts..."),           },
        { "view_column_layout_load",        tr("Load Column Layout"),                 },
        { "menu_extras",                    tr("E&xtras"),                            },
        { "update_database",                tr("Update Database"),                    },
        { "configure",                      tr("Settings..."),                        tr("Ctrl+,", "Extras|Settings") },
        { "reload_scripts",                 tr("Reload user scripts"),                },
        { "menu_window",                    tr("&Windows"),                           },
        { "menu_help",                      tr("&Help"),                              },
        { "help_about",                     tr("About..."),                           },
        { "help_systeminfo",                tr("System Information..."),              },
        { "help_announcements",             tr("Important Announcements..."),         },
        { "help_releasenotes",              tr("Release notes..."),                   },
        { "help_reportbug",                 tr("Report a bug..."),                    },
        { "check_for_updates",              tr("Check for Program Updates..."),       },
        { "edit_status",                    tr("Status"),                             },
        { "edit_status_include",            tr("Set status to Include"),              },
        { "edit_status_exclude",            tr("Set status to Exclude"),              },
        { "edit_status_extra",              tr("Set status to Extra"),                },
        { "edit_status_toggle",             tr("Toggle status between Include and Exclude"), },
        { "edit_cond",                      tr("Condition"),                          },
        { "edit_cond_new",                  tr("Set condition to New", "Cond|New"),   },
        { "edit_cond_used",                 tr("Set condition to Used"),              },
        { "edit_subcond_none",              tr("Set sub-condition to None"),          },
        { "edit_subcond_sealed",            tr("Set sub-condition to Sealed"),        },
        { "edit_subcond_complete",          tr("Set sub-condition to Complete"),      },
        { "edit_subcond_incomplete",        tr("Set sub-condition to Incomplete"),    },
        { "edit_item",                      tr("Set item..."),                        },
        { "edit_color",                     tr("Set color..."),                       },
        { "edit_qty",                       tr("Quantity"),                           },
        { "edit_qty_set",                   tr("Set quantity..."),                    },
        { "edit_qty_multiply",              tr("Multiply quantity..."),               tr("Ctrl+*", "Edit|Quantity|Multiply") },
        { "edit_qty_divide",                tr("Divide quantity..."),                 tr("Ctrl+/", "Edit|Quantity|Divide") },
        { "edit_price",                     tr("Price"),                              },
        { "edit_price_round",               tr("Round price to 2 decimals"),          },
        { "edit_price_set",                 tr("Set price..."),                       },
        { "edit_price_to_priceguide",       tr("Set price to guide..."),              tr("Ctrl+G", "Edit|Price|Set to PriceGuide") },
        { "edit_price_inc_dec",             tr("Adjust price..."),                    tr("Ctrl++", "Edit|Price|Inc/Dec") },
        { "edit_cost",                      tr("Cost"),                               },
        { "edit_cost_round",                tr("Round cost to 2 decimals") ,          },
        { "edit_cost_set",                  tr("Set cost..."),                        },
        { "edit_cost_inc_dec",              tr("Adjust cost..."),                     },
        { "edit_cost_spread",               tr("Spread Cost Amount..."),              },
        { "edit_bulk",                      tr("Set bulk quantity..."),               },
        { "edit_sale",                      tr("Set sale..."),                        tr("Ctrl+%", "Edit|Sale") },
        { "edit_comment",                   tr("Comment"),                            },
        { "edit_comment_set",               tr("Set comment..."),                     },
        { "edit_comment_add",               tr("Add to comment..."),                  },
        { "edit_comment_rem",               tr("Remove from comment..."),             },
        { "edit_comment_clear",             tr("Clear comment"),                      },
        { "edit_remark",                    tr("Remark"),                             },
        { "edit_remark_set",                tr("Set remark..."),                      },
        { "edit_remark_add",                tr("Add to remark..."),                   },
        { "edit_remark_rem",                tr("Remove from remark..."),              },
        { "edit_remark_clear",              tr("Clear remark"),                       },
        { "edit_retain",                    tr("Retain in Inventory"),                },
        { "edit_retain_yes",                tr("Set retain flag"),                    },
        { "edit_retain_no",                 tr("Unset retain flag"),                  },
        { "edit_retain_toggle",             tr("Toggle retain flag"),                 },
        { "edit_stockroom",                 tr("Stockroom Item"),                     },
        { "edit_stockroom_no",              tr("Set stockroom to None"),              },
        { "edit_stockroom_a",               tr("Set stockroom to A"),                 },
        { "edit_stockroom_b",               tr("Set stockroom to B"),                 },
        { "edit_stockroom_c",               tr("Set stockroom to C"),                 },
        { "edit_reserved",                  tr("Set reserved for..."),                },
        { "edit_marker",                    tr("Marker"),                             },
        { "edit_marker_text",               tr("Set marker text"),                    },
        { "edit_marker_color",              tr("Set marker color..."),                },
        { "edit_marker_clear",              tr("Clear marker"),                       },
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
        { "edit_color", "color_management" },
        { "edit_filter_focus", "view-filter" },
        { "document_import_bl_inv", "brick-1x1" },
        { "document_import_bl_xml", "dialog-xml-editor" },
        { "document_import_bl_order", "view-financial-list" },
        { "document_import_bl_cart", "bricklink-cart" },
        { "document_import_bl_store_inv", "bricklink-store" },
        { "document_import_ldraw_model", "bricklink-studio" },

    };

    QVariantMap customShortcuts = Config::inst()->shortcuts();

    for (auto &at : actiontable) {
        if (QAction *a = findAction(at.name)) {
            if (!at.text.isNull())
                a->setText(at.text);

            QList<QKeySequence> defaultShortcuts;
            if (at.standardKey != QKeySequence::UnknownKey)
                defaultShortcuts = QKeySequence::keyBindings(at.standardKey);
            else if (!at.shortcut.isNull())
                defaultShortcuts = QKeySequence::listFromString(at.shortcut);
            a->setProperty("bsShortcut", QVariant::fromValue(defaultShortcuts));

            QList<QKeySequence> shortcuts = defaultShortcuts;
            auto custom = customShortcuts.value(QLatin1String(at.name)).value<QKeySequence>();
            if (!custom.isEmpty())
                shortcuts = { custom };

            a->setShortcuts(shortcuts);
            if (!shortcuts.isEmpty())
                a->setToolTip(Utility::toolTipLabel(a->text(), shortcuts));
            else
                a->setToolTip(a->text());

            QString iconName = QString::fromLatin1(iconalias.value(at.name, at.name));
            iconName.replace("_"_l1, "-"_l1);

            QIcon icon = QIcon::fromTheme(iconName);
            if (!icon.isNull())
                a->setIcon(icon);
        }
    }
}

FrameWork::~FrameWork()
{
    Config::inst()->setValue("/MainWindow/Layout/State"_l1, saveState(DockStateVersion));
    Config::inst()->setValue("/MainWindow/Layout/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/Filter"_l1, m_filter->saveState());

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

QAction *FrameWork::findAction(const char *name) const
{
    return findAction(QString::fromLatin1(name));
}

QAction *FrameWork::findAction(const QString &name) const
{
    static QHash<QString, QAction *> cache;

    if (name.isEmpty())
        return nullptr;
    auto &action = cache[name];
    if (!action)
        action = findChild<QAction *>(name);
    return action;
}

QDockWidget *FrameWork::createDock(QWidget *widget)
{
    QDockWidget *dock = new QDockWidget(QString(), this);
    dock->setObjectName("Dock-"_l1 + widget->objectName());
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
        if (an.startsWith("-")) {
            auto *a = m->addSeparator();
            if (an.length() > 1)
                a->setObjectName(QString::fromLatin1(an.mid(1)));
        } else if (QAction *a = findAction(an.constData())) {
            m->addAction(a);
        } else  {
            qWarning("Couldn't find action '%s' when creating menu '%s'",
                     an.constData(), name.constData());
        }
    }
    return m;
}


bool FrameWork::setupToolBar()
{
    if (!m_toolbar)
        return false;
    m_toolbar->clear();

    QStringList actionNames = toolBarActionNames();
    if (actionNames.isEmpty())
        actionNames = defaultToolBarActionNames();

    actionNames.append({ "|"_l1, "widget_progress"_l1, "|"_l1 });

    for (const QString &an : qAsConst(actionNames)) {
        if (an == "-"_l1) {
            m_toolbar->addSeparator()->setObjectName(an);
        } else if (an == "|"_l1) {
            QWidget *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setFixedSize(sp, sp);
            m_toolbar->addWidget(spacer);
        } else if (an == "<>"_l1) {
            QWidget *spacer = new QWidget();
            spacer->setObjectName(an);
            int sp = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent);
            spacer->setMinimumSize(sp, sp);
            spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            m_toolbar->addWidget(spacer);
        } else if (an == "edit_filter_focus"_l1) {
            m_toolbar->addAction(m_filterAction);
        } else if (an == "widget_progress"_l1) {
            m_toolbar->addAction(m_progressAction);
        } else if (QAction *a = findAction(an)) {
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

template <typename M = QMenu>
inline static QMenu *newQMenu(QWidget *parent, const char *name, quint32 flags = 0)
{
    M *m = new M(parent);
    m->menuAction()->setObjectName(QLatin1String(name));
    if (flags)
        m->menuAction()->setProperty("bsFlags", flags);
    m->menuAction()->setProperty("bsAction", true);
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
    a->setProperty("bsAction", true);
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
        setupWindow(DocumentIO::open());
    });

    auto rm = new RecentMenu(this);
    rm->menuAction()->setObjectName("document_open_recent"_l1);
    connect(rm, &RecentMenu::openRecent,
            this, &FrameWork::openDocument);
    connect(rm, &RecentMenu::clearRecent,
            this, []() { Config::inst()->setRecentFiles({ }); });

    (void) newQAction(this, "document_save");
    (void) newQAction(this, "document_save_as", NeedDocument);
    (void) newQAction(this, "document_print", NeedDocument | NeedLots);
    (void) newQAction(this, "document_print_pdf", NeedDocument | NeedLots);

    m = newQMenu(this, "document_import");
    m->addAction(newQAction(this, "document_import_bl_inv", 0, false, this, [this]() {
        if (!m_importinventory_dialog)
            m_importinventory_dialog = new ImportInventoryDialog(this);
        m_importinventory_dialog->show();
    }));
    m->addAction(newQAction(this, "document_import_bl_xml", 0, false, this, [this]() {
        createWindow(DocumentIO::importBrickLinkXML());
    }));
    m->addAction(newQAction(this, "document_import_bl_order", NeedNetwork, false, this, [this]() {
        if (checkBrickLinkLogin()) {
            if (!m_importorder_dialog)
                m_importorder_dialog = new ImportOrderDialog(this);
            m_importorder_dialog->show();
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
            m_importcart_dialog->show();
        }
    }));
    m->addAction(newQAction(this, "document_import_ldraw_model", 0, false, this, [this]() {
        createWindow(DocumentIO::importLDrawModel());
    }));


    m = newQMenu(this, "document_export");
    m->addAction(newQAction(this, "document_export_bl_xml", NeedDocument | NeedLots));
    m->addAction(newQAction(this, "document_export_bl_xml_clip", NeedDocument | NeedLots));
    m->addAction(newQAction(this, "document_export_bl_update_clip", NeedDocument | NeedLots));
    m->addAction(newQAction(this, "document_export_bl_invreq_clip", NeedDocument | NeedLots));
    m->addAction(newQAction(this, "document_export_bl_wantedlist_clip", NeedDocument | NeedLots));

    (void) newQAction(this, "document_close", NeedDocument);

    a = newQAction(this, "application_exit", 0, false, this, &FrameWork::close);
    a->setMenuRole(QAction::QuitRole);

    a = m_undogroup->createUndoAction(this);
    a->setObjectName("edit_undo"_l1);
    a->setProperty("bsAction", true);
    a = m_undogroup->createRedoAction(this);
    a->setObjectName("edit_redo"_l1);
    a->setProperty("bsAction", true);

    (void) newQAction(this, "edit_cut", NeedSelection(1));
    (void) newQAction(this, "edit_copy", NeedSelection(1));
    (void) newQAction(this, "edit_paste", NeedDocument);
    (void) newQAction(this, "edit_paste_silent", NeedDocument);
    (void) newQAction(this, "edit_duplicate", NeedSelection(1));
    (void) newQAction(this, "edit_delete", NeedSelection(1));

    a = newQAction(this, "edit_additems", NeedDocument, false, this, &FrameWork::showAddItemDialog);
    a->setShortcutContext(Qt::ApplicationShortcut);

    (void) newQAction(this, "edit_subtractitems", NeedDocument);
    (void) newQAction(this, "edit_mergeitems", NeedSelection(2));
    (void) newQAction(this, "edit_partoutitems", NeedInventory | NeedSelection(1) | NeedQuantity);
    (void) newQAction(this, "edit_copy_fields", NeedDocument | NeedLots);
    (void) newQAction(this, "edit_select_all", NeedDocument | NeedLots);
    (void) newQAction(this, "edit_select_none", NeedDocument | NeedLots);
    (void) newQAction(this, "edit_filter_from_selection", NeedSelection(1, 1));
    (void) newQAction(this, "edit_filter_focus", NeedDocument, false, this, [this]() {
        if (m_filter)
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
    g = newQActionGroup(this, nullptr, true);
    m->addAction(newQAction(g, "edit_subcond_none", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_sealed", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_complete", NeedSelection(1) | NeedSubCondition, true));
    m->addAction(newQAction(g, "edit_subcond_incomplete", NeedSelection(1) | NeedSubCondition, true));

    (void) newQAction(this, "edit_item", NeedSelection(1));
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

    m = newQMenu(this, "edit_marker", NeedSelection(1));
    m->addAction(newQAction(this, "edit_marker_text", NeedSelection(1)));
    m->addAction(newQAction(this, "edit_marker_color", NeedSelection(1)));
    m->addSeparator();
    m->addAction(newQAction(this, "edit_marker_clear", NeedSelection(1)));

    (void) newQAction(this, "bricklink_catalog", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_priceguide", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_lotsforsale", NeedSelection(1, 1) | NeedNetwork);
    (void) newQAction(this, "bricklink_myinventory", NeedSelection(1, 1) | NeedNetwork);

    (void) newQAction(this, "view_fullscreen", 0, true, this, [this](bool fullScreen) {
        setWindowState(windowState().setFlag(Qt::WindowFullScreen, fullScreen));
    });

    m_toolbar->toggleViewAction()->setObjectName("view_toolbar"_l1);
    m = newQMenu(this, "view_docks");
    foreach (QDockWidget *dock, m_dock_widgets)
        m->addAction(dock->toggleViewAction());

    (void) newQAction(this, "view_show_input_errors", 0, true,
                      Config::inst(), &Config::setShowInputErrors);
    (void) newQAction(this, "view_show_diff_indicators", 0, true,
                      Config::inst(), &Config::setShowDifferenceIndicators);
    (void) newQAction(this, "view_reset_diff_mode", NeedSelection(1));

    (void) newQAction(this, "view_column_layout_save", NeedDocument, false);
    (void) newQAction(this, "view_column_layout_manage", 0, false, this, &FrameWork::manageLayouts);
    auto lclm = newQMenu<LoadColumnLayoutMenu>(this, "view_column_layout_load", NeedDocument);
    lclm->menuAction()->setIcon(QIcon::fromTheme("object-columns"_l1));
    lclm->setObjectName("view_column_layout_list"_l1);

    (void) newQAction(this, "update_database", NeedNetwork, false, this, &FrameWork::updateDatabase);

    a = newQAction(this, "configure", 0, false, this, [this]() { showSettings(); });
    a->setMenuRole(QAction::PreferencesRole);

    (void) newQAction(this, "reload_scripts", 0, false, ScriptManager::inst(), &ScriptManager::reload);

    a = newQAction(this, "help_about", 0, false, Application::inst(), [this]() {
        AboutDialog(this).exec();
    });
    a->setMenuRole(QAction::AboutRole);

    (void) newQAction(this, "help_systeminfo", 0, false, Application::inst(), [this]() {
        SystemInfoDialog(this).exec();
    });
    (void) newQAction(this, "help_reportbug", 0, false, Application::inst(), []() {
        QString url = "https://"_l1 % Application::inst()->gitHubUrl() % "/issues/new"_l1;
        QDesktopServices::openUrl(url);
    });
    (void) newQAction(this, "help_announcements", 0, false, Application::inst(), [this]() {
        m_announcements->showAllAnnouncements();
    });
    (void) newQAction(this, "help_releasenotes", 0, false, Application::inst(), []() {
        QString url = "https://"_l1 % Application::inst()->gitHubUrl() % "/releases"_l1;
        QDesktopServices::openUrl(url);
    });

    a = newQAction(this, "check_for_updates", NeedNetwork, false,
                   this, [this]() { m_checkForUpdates->check(false /*not silent*/); });
    a->setMenuRole(QAction::ApplicationSpecificRole);

    m_filter = new HistoryLineEdit(Config::MaxFilterHistory, this);
    m_filter->setClearButtonEnabled(true);
    m_filter_delay = new QTimer(this);
    m_filter_delay->setInterval(800ms);
    m_filter_delay->setSingleShot(true);

    // a bit of a hack, but the action needs to be "used" for the shortcut to be
    // active, but we don't want an icon in the filter edit.
    m_filter->QWidget::addAction(findAction("edit_filter_focus"));

    connect(m_filter->reFilterAction(), &QAction::triggered,
            this, &FrameWork::reFilter);

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

    m_filterAction = new QWidgetAction(this);
    m_filterAction->setDefaultWidget(m_filter);

    m_progress = new ProgressCircle();
    m_progress->setIcon(QIcon(":/images/brickstore_icon.png"_l1));
    m_progress->setColor("#4ba2d8");

    connect(m_progress, &ProgressCircle::cancelAll,
            BrickLink::core(), &BrickLink::Core::cancelTransfers);

    m_progressAction = new QWidgetAction(this);
    m_progressAction->setDefaultWidget(m_progress);
}

void FrameWork::openDocument(const QString &file)
{
    setupWindow(DocumentIO::open(file));
}

void FrameWork::fileImportBrickLinkInventory(const BrickLink::Item *item, int quantity,
                                             BrickLink::Condition condition)
{
    if (!item)
        return;

    if (auto doc = DocumentIO::importBrickLinkInventory(item, quantity, condition))
        FrameWork::inst()->createWindow(doc);
}

bool FrameWork::checkBrickLinkLogin()
{
    forever {
        QPair<QString, QString> auth = Config::inst()->brickLinkCredentials();

        if (!auth.first.isEmpty() && !auth.second.isEmpty())
            return true;

        if (MessageBox::question(this, { },
                                 tr("No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?")
                                 ) == QMessageBox::Yes)
            showSettings("bricklink");
        else
            return false;
    }
}

QList<QAction *> FrameWork::contextMenuActions() const
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
        for (const auto &aname : contextActions) {
            if (aname == QByteArray("-"))
                actions << nullptr;
            else
                actions << findAction(aname);
        }
    }

    QList<QAction *> result = actions;
    if (!m_extensionContextActions.isEmpty()) {
        result.append(nullptr);
        result.append(m_extensionContextActions);
    }

    return result;
}

static QString rootMenu(const QAction *action)
{
    QList<QWidget *> widgets = action->associatedWidgets();
    for (QWidget *w : qAsConst(widgets)) {
        if (auto m = qobject_cast<QMenu *>(w))
            return rootMenu(m->menuAction());
        else if (qobject_cast<QMenuBar *>(w))
            return action->text();
    }
    return { };
}

QMultiMap<QString, QString> FrameWork::allActions() const
{
    QMultiMap<QString, QString> result;

    const auto list = findChildren<QAction *>();
    for (const auto *action : list) {
        if (!action->property("bsAction").toBool())
            continue;
        QString mtitle = rootMenu(action);
        if (mtitle.isEmpty())
            mtitle = tr("Other");
        result.insert(mtitle, action->objectName());
    }
    return result;
}

QStringList FrameWork::toolBarActionNames() const
{
    const QStringList actionNames = Config::inst()->toolBarActions();
    return actionNames.isEmpty() ? defaultToolBarActionNames() : actionNames;
}

QStringList FrameWork::defaultToolBarActionNames() const
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
        "|"_l1,
        "edit_filter_focus"_l1,
    };
    return actionNames;
}

bool FrameWork::restoreWindowsFromAutosave()
{
    int restorable = Window::restorableAutosaves();
    if (restorable > 0) {
        bool b = (MessageBox::question(this, tr("Restore Documents"), tr("It seems like BrickStore crashed while %n document(s) had unsaved modifications.", nullptr, restorable)
                                       % u"<br><br>" % tr("Should these documents be restored from their last available auto-save state?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                  == QMessageBox::Yes);

        auto restoredWindows = Window::processAutosaves(b ? Window::AutosaveAction::Restore
                                                          : Window::AutosaveAction::Delete);
        for (auto window : restoredWindows) {
            m_undogroup->addStack(window->document()->undoStack());
            m_workspace->addWindow(window);
            setActiveWindow(window);
        }
    }
    return (restorable > 0);
}

void FrameWork::setupWindow(Window *win)
{
    if (!win)
        return;

    m_undogroup->addStack(win->document()->undoStack());
    m_workspace->addWindow(win);
    setActiveWindow(win);

    if (win->document()->legacyCurrencyCode()
            && (Config::inst()->defaultCurrencyCode() != "USD"_l1)) {
        QMetaObject::invokeMethod(this, []() {
            MessageBox::information(nullptr, { }, tr("You have loaded an old style document that does not have any currency information attached. You can convert this document to include this information by using the currency code selector in the top right corner."));
        }, Qt::QueuedConnection);
    }
}

Window *FrameWork::createWindow(Document *doc)
{
    if (!doc)
        return nullptr;

    Window *win = new Window(doc);
    setupWindow(win);
    return win;
}

Window *FrameWork::activeWindow() const
{
    return m_activeWin;
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

    QStringList files;
    foreach (QWidget *w, m_workspace->windowList()) {
        QString fileName = static_cast<Window *>(w)->document()->fileName();
        if (!fileName.isEmpty())
            files << fileName;
    }

    if (noWindows || closeAllWindows()) {
        delete m_add_dialog;
        delete m_importinventory_dialog;
        delete m_importorder_dialog;
        delete m_importcart_dialog;

        auto doUpdate = [this, files]() -> bool {
            if (!m_workspace->windowList().isEmpty())
                return false;

            Transfer trans;
            ProgressDialog d(tr("Update Database"), &trans, this);
            UpdateDatabase update(&d);
            bool result = d.exec();

            for (const auto &file : files)
                openDocument(file);

            return result;
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
    if (w && w == m_activeWin)
        return;

    QString filterToolTip;

    if (m_activeWin) {
        Document *doc = m_activeWin->document();

        connectAllActions(false, m_activeWin);

        disconnect(m_activeWin.data(), &Window::windowTitleChanged,
                   this, &FrameWork::titleUpdate);
        disconnect(doc, &Document::modificationChanged,
                   this, &FrameWork::modificationUpdate);
        disconnect(m_activeWin.data(), &Window::selectedLotsChanged,
                   this, &FrameWork::selectionUpdate);
        disconnect(m_activeWin.data(), &Window::blockingOperationActiveChanged,
                   this, &FrameWork::blockUpdate);
        if (m_filter) {
            disconnect(this, &FrameWork::filterChanged,
                       doc, &Document::setFilter);
            disconnect(doc, &Document::filterChanged,
                       this, &FrameWork::setFilter);
            disconnect(doc, &Document::isFilteredChanged,
                       this, &FrameWork::updateReFilterAction);

            m_filter->setText(QString());
            m_filter->setEnabled(false);
            m_filter->reFilterAction()->setVisible(false);
        }
        m_undogroup->setActiveStack(nullptr);

        m_activeWin = nullptr;
    }

    if (Window *window = qobject_cast<Window *>(w)) {
        Document *doc = window->document();

        connectAllActions(true, window);

        connect(window, &Window::windowTitleChanged,
                this, &FrameWork::titleUpdate);
        connect(doc, &Document::modificationChanged,
                this, &FrameWork::modificationUpdate);
        connect(window, &Window::selectedLotsChanged,
                this, &FrameWork::selectionUpdate);
        connect(window, &Window::blockingOperationActiveChanged,
                this, &FrameWork::blockUpdate);

        if (m_filter) {
            m_filter->setText(doc->filter());
            filterToolTip = doc->filterToolTip();
            connect(this, &FrameWork::filterChanged,
                    doc, &Document::setFilter);
            connect(doc, &Document::filterChanged,
                    this, &FrameWork::setFilter);
            connect(doc, &Document::isFilteredChanged,
                    this, &FrameWork::updateReFilterAction);

            if (auto a = findAction("edit_filter_focus"))
                m_filter->setToolTip(Utility::toolTipLabel(a->text(), a->shortcut(),
                                                           filterToolTip
                                                           + m_filter->instructionToolTip()));
            m_filter->setEnabled(true);
        }

        m_undogroup->setActiveStack(doc->undoStack());
        m_activeWin = window;
    }

    if (m_add_dialog) {
        m_add_dialog->attach(m_activeWin);
        if (!m_activeWin)
            m_add_dialog->close();
    }

    selectionUpdate(m_activeWin ? m_activeWin->selectedLots() : LotList());
    blockUpdate(m_activeWin ? m_activeWin->isBlockingOperationActive() : false);
    titleUpdate();
    modificationUpdate();
    updateReFilterAction(m_activeWin ? m_activeWin->document()->isFiltered() : true);

    emit windowActivated(m_activeWin);
}

void FrameWork::updateActions()
{
    LotList selection;
    if (m_activeWin)
        selection = m_activeWin->selectedLots();
    bool isOnline = Application::inst()->isOnline();

    const auto *doc = m_activeWin ? m_activeWin->document() : nullptr;
    int docItemCount = doc ? int(doc->rowCount()) : 0;

    foreach (QAction *a, findChildren<QAction *>()) {
        quint32 flags = a->property("bsFlags").toUInt();

        if (!flags)
            continue;

        bool b = true;

        if (m_activeWin && m_activeWin->isBlockingOperationActive()
                && (flags & (NeedDocument | NeedLots | NeedItemMask))) {
            b = false;
        }

        if (flags & NeedNetwork)
            b = b && isOnline;

        if (flags & NeedDocument) {
            b = b && m_activeWin;

            if (b) {
                if (flags & NeedLots)
                    b = b && (docItemCount > 0);

                quint8 minSelection = (flags >> 24) & 0xff;
                quint8 maxSelection = (flags >> 16) & 0xff;

                if (minSelection)
                    b = b && (selection.count() >= minSelection);
                if (maxSelection)
                    b = b && (selection.count() <= maxSelection);
            }
        }

        if (flags & NeedItemMask) {
            foreach (Lot *item, selection) {
                if (flags & NeedLotId)
                    b = b && (item->lotId() != 0);
                if (flags & NeedInventory)
                    b = b && (item->item() && item->item()->hasInventory());
                if (flags & NeedQuantity)
                    b = b && (item->quantity() != 0);
                if (flags & NeedSubCondition)
                    b = b && (item->itemType() && item->itemType()->hasSubConditions());

                if (!b)
                    break;
            }
        }
        a->setEnabled(b);
    }
}

QMenu *FrameWork::createPopupMenu()
{
    auto menu = QMainWindow::createPopupMenu();
    if (menu) {
        menu->addAction(tr("Customize Toolbar..."), this, [=]() {
            showSettings("toolbar");
        });
    }
    return menu;
}

void FrameWork::selectionUpdate(const LotList &selection)
{
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

        foreach (Lot *item, selection) {
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

    updateActions();
}

void FrameWork::blockUpdate(bool blocked)
{
    static QUndoStack blockStack;

    if (activeWindow())
        m_undogroup->setActiveStack(blocked ? &blockStack : activeWindow()->document()->undoStack());
    if (m_filter)
        m_filter->setDisabled(blocked);
    updateActions();
    modificationUpdate();
}

void FrameWork::titleUpdate()
{
    QString title = QApplication::applicationName();
    QString file;

    if (m_activeWin) {
        title = m_activeWin->windowTitle() % u" \u2014 " % title;
        file = m_activeWin->document()->fileName();
    }
    setWindowTitle(title);
    setWindowFilePath(file);
}

void FrameWork::modificationUpdate()
{
    bool modified = m_activeWin ? m_activeWin->isWindowModified() : false;
    bool blocked = m_activeWin ? m_activeWin->isBlockingOperationActive() : false;
    bool hasNoFileName = (m_activeWin && m_activeWin->document()->fileName().isEmpty());

    setWindowModified(modified);
    findAction("document_save")->setEnabled((modified || hasNoFileName) && !blocked);
}

void FrameWork::transferProgressUpdate(int p, int t)
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

void FrameWork::showSettings(const char *page)
{
    SettingsDialog d(QLatin1String(page), this);
    d.exec();
}

void FrameWork::onlineStateChanged(bool isOnline)
{
    BrickLink::core()->setOnlineStatus(isOnline);
    if (m_progress)
        m_progress->setOnlineState(isOnline);
    if (!isOnline)
        cancelAllTransfers(true);

    updateActions();
}

void FrameWork::setFilter(const QString &filter)
{
    if (m_filter && (filter != m_filter->text()))
        m_filter->setText(filter);
}

void FrameWork::reFilter()
{
    if (m_filter && m_activeWin)
        m_activeWin->document()->reFilter();
}

void FrameWork::updateReFilterAction(bool isFiltered)
{
    if (m_filter)
        m_filter->reFilterAction()->setVisible(!m_filter->text().isEmpty() && !isFiltered);
}

void FrameWork::closeEvent(QCloseEvent *e)
{
    QStringList files;
    foreach (QWidget *w, m_workspace->windowList()) {
        QString fileName = static_cast<Window *>(w)->document()->fileName();
        if (!fileName.isEmpty())
            files << fileName;
    }
    Config::inst()->setValue("/MainWindow/LastSessionDocuments"_l1, files);

    if (!closeAllWindows()) {
        e->ignore();
        return;
    }

    QMainWindow::closeEvent(e);
}


bool FrameWork::closeAllWindows()
{
    foreach (QWidget *w, m_workspace->windowList()) {
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
                                       ) == QMessageBox::Yes)) {
        BrickLink::core()->cancelTransfers();
    }
}

void FrameWork::showAddItemDialog()
{
    if (!m_add_dialog) {
        m_add_dialog = new AddItemDialog();
        m_add_dialog->setObjectName("additems"_l1);
        m_add_dialog->attach(m_activeWin);

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
