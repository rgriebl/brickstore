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
#pragma once

#include <QMainWindow>
#include <QMap>
#include <QAction>
#include <QStringList>
#include <QPointer>

#include "document.h"
#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)

class HistoryLineEdit;
class Workspace;
class ProgressCircle;
class Window;
class UndoGroup;
class TaskInfoWidget;
class TaskPriceGuideWidget;
class TaskLinksWidget;
class TaskAppearsInWidget;
class Document;
class AddItemDialog;
class ScriptManager;
class ImportInventoryDialog;
class ImportOrderDialog;
class ImportCartDialog;
class CheckForUpdates;


class FrameWork : public QMainWindow
{
    Q_OBJECT
private:
    FrameWork(QWidget *parent = nullptr);
    static FrameWork *s_inst;

public:
    ~FrameWork() override;
    static FrameWork *inst();

    bool closeAllWindows();
    QVector<Window *> allWindows() const;
    bool restoreWindowsFromAutosave();
    void setupWindow(Window *win);
    Window *createWindow(Document *doc);
    Window *activeWindow() const;
    void setActiveWindow(Window *window);

    static constexpr int maxQuantity = 9999999;
    static constexpr double maxPrice = 99999;

public slots:
    void selectionUpdate(const LotList &selection);
    void blockUpdate(bool blocked);
    void modificationUpdate();
    void titleUpdate();
    void setFilter(const QString &filterText);
    void reFilter();
    void updateReFilterAction(bool isFiltered);

    void fileImportBrickLinkInventory(const BrickLink::Item *item, int quantity = 1,
                                      BrickLink::Condition condition = BrickLink::Condition::New);

signals:
    void windowActivated(Window *);
    void windowListChanged();
    void filterChanged(const QString &filter);

protected slots:
    void languageChange();

private slots:
    void openDocument(const QString &);

    bool updateDatabase(bool forceSync = false);
    void checkForUpdates(bool silent = false);

    void connectWindow(QWidget *w);
    void transferJobProgressUpdate(int p, int t);

    void showSettings(const char *page = nullptr);

    void cancelAllTransfers(bool force = false);
    void showAddItemDialog();

    void onlineStateChanged(bool isOnline);
    void manageLayouts();

protected:
   void dragEnterEvent(QDragEnterEvent *e) override;
   void dropEvent(QDropEvent *e) override;
   void changeEvent(QEvent *e) override;
   void closeEvent(QCloseEvent *e) override;

private:
    void setBrickLinkUpdateIntervals();
    bool checkBrickLinkLogin();

public:
    QList<QAction *> contextMenuActions() const;
    QList<QAction *> allActions() const;
    QAction *findAction(const char *name) const;

private:
    void setupScripts();
    void connectAllActions(bool do_connect, Window *window);
    void createActions();
    void translateActions();
    void updateActions();
    QMenu *createMenu(const QByteArray &, const QVector<QByteArray> &);
    bool setupToolBar(QToolBar *, const QVector<QByteArray> &);
    QDockWidget *createDock(QWidget *widget);

    Workspace *m_workspace;
    QPointer<Window> m_activeWin;
    QList<QAction *> m_extensionExtraActions;
    QList<QAction *> m_extensionContextActions;

    ProgressCircle *m_progress;
    HistoryLineEdit *m_filter;
    QToolBar *m_toolbar;
    QVector<QDockWidget *> m_dock_widgets;
    TaskInfoWidget *m_task_info;
    TaskPriceGuideWidget *m_task_priceguide;
    TaskAppearsInWidget *m_task_appears;
    QPointer<AddItemDialog> m_add_dialog;
    QPointer<ImportInventoryDialog> m_importinventory_dialog;
    QPointer<ImportOrderDialog> m_importorder_dialog;
    QPointer<ImportCartDialog> m_importcart_dialog;
    QPointer<CheckForUpdates> m_checkForUpdates;
    QTimer *m_filter_delay = nullptr;
    bool m_running;
    UndoGroup *m_undogroup;

    friend class WelcomeWidget;
    friend class DocumentDelegate;
};
