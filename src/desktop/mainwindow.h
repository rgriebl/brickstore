// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QMainWindow>
#include <QMap>
#include <QAction>
#include <QStringList>
#include <QPointer>

#include <QCoro/QCoroTask>

#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QWidgetAction)
QT_FORWARD_DECLARE_CLASS(QSizeGrip)
QT_FORWARD_DECLARE_CLASS(QSplitter)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)
QT_FORWARD_DECLARE_CLASS(QStringListModel)

class HistoryLineEdit;
class Workspace;
class ProgressCircle;
class View;
class Document;
class UndoGroup;
class TaskInfoWidget;
class TaskPriceGuideWidget;
class TaskLinksWidget;
class TaskInventoryWidget;
class DocumentModel;
class AddItemDialog;
class ScriptManager;
class ImportInventoryDialog;
class ImportOrderDialog;
class ImportCartDialog;
class ImportWantedListDialog;
class CheckForUpdates;
class Announcements;
class DeveloperConsole;
class ViewPane;
class LoadColumnLayoutMenuAdapter;
class WelcomeWidget;


class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    MainWindow(QWidget *parent = nullptr);
    static MainWindow *s_inst;

public:
    ~MainWindow() override;
    static MainWindow *inst();

    void show();
    QCoro::Task<> shutdown();

    void closeAllDialogs();
    View *activeView() const;
    void setActiveView(View *view);

    QMenu *createPopupMenu() override;

    void showAddItemDialog(const BrickLink::Item *item = nullptr,
                            const BrickLink::Color *color = nullptr);

public slots:
    void blockUpdate(bool blocked);
    void titleUpdate();

signals:
    void documentActivated(Document *);

protected slots:
    void languageChange();

private slots:
    void connectView(View *w);
    void transferProgressUpdate(int p, int t);

    void showSettings(const QString &page = { });
    void show3DSettings();

    void goHome(bool home);
    void repositionHomeWidget();

protected:
   void dragEnterEvent(QDragEnterEvent *e) override;
   void dropEvent(QDropEvent *e) override;
   void changeEvent(QEvent *e) override;
   void closeEvent(QCloseEvent *e) override;
   void resizeEvent(QResizeEvent *e) override;

public:
    QList<QAction *> contextMenuActions() const;
    QStringList toolBarActionNames() const;
    QStringList defaultToolBarActionNames() const;

private:
    void createCentralWidget();
    void setupScripts();
    void createActions();
    QMenu *setupMenu(const QByteArray &, const QVector<QByteArray> &);
    void setupMenuBar();
    bool setupToolBar();
    void setupDockWidgets();
    QDockWidget *createDock(QWidget *widget, const char *name);
    void setActiveViewPane(ViewPane *newActive);
    ViewPane *createViewPane(Document *activeDocument, QWidget *window);

    Workspace *m_workspace;
    QList<QAction *> m_extensionContextActions;

    ProgressCircle *m_progress;
    QWidgetAction *m_progressAction;
    QToolBar *m_toolbar;
    QMenu *m_extrasMenu;
    LoadColumnLayoutMenuAdapter *m_loadColumnLayoutMenu;
    QVector<QDockWidget *> m_dock_widgets;
    QPointer<AddItemDialog> m_add_dialog;
    QPointer<ImportInventoryDialog> m_importinventory_dialog;
    QPointer<ImportOrderDialog> m_importorder_dialog;
    QPointer<ImportCartDialog> m_importcart_dialog;
    QPointer<ImportWantedListDialog> m_importwanted_dialog;
    QPointer<CheckForUpdates> m_checkForUpdates;
    QPointer<Announcements> m_announcements;

    QAction *m_goHome = nullptr;

    QPointer<View> m_activeView;
    QPointer<ViewPane> m_activeViewPane;
    QMultiHash<QWidget *, ViewPane *> m_allViewPanes;

    WelcomeWidget *m_welcomeWidget;
    QStringListModel *m_favoriteFilters;
    QByteArray m_defaultDockState;

    friend class DocumentDelegate;
};
