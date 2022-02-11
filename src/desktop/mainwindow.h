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
#pragma once

#include <QMainWindow>
#include <QMap>
#include <QAction>
#include <QStringList>
#include <QPointer>

#include "bricklink/global.h"
#include "common/documentmodel.h"
#include "qcoro/task.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QWidgetAction)
QT_FORWARD_DECLARE_CLASS(QSizeGrip)
QT_FORWARD_DECLARE_CLASS(QSplitter)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)

class HistoryLineEdit;
class Workspace;
class ProgressCircle;
class View;
class Document;
class UndoGroup;
class TaskInfoWidget;
class TaskPriceGuideWidget;
class TaskLinksWidget;
class TaskAppearsInWidget;
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


class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    MainWindow(QWidget *parent = nullptr);
    static MainWindow *s_inst;

public:
    ~MainWindow() override;
    static MainWindow *inst();

    QCoro::Task<bool> closeAllViews();
    View *activeView() const;
    void setActiveView(View *view);

    QMenu *createPopupMenu() override;

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
    void forEachViewPane(std::function<bool(ViewPane *)> callback);
    void setActiveViewPane(ViewPane *newActive);
    ViewPane *createViewPane(Document *activeDocument);
    void deleteViewPane(ViewPane *viewPane);

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

    QWidget *m_welcomeWidget;

    friend class DocumentDelegate;
};
