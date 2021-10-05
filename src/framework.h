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
#include "bricklink/bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QWidgetAction)

class HistoryLineEdit;
class Workspace;
class ProgressCircle;
class View;
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
class Announcements;


class FrameWork : public QMainWindow
{
    Q_OBJECT
private:
    FrameWork(QWidget *parent = nullptr);
    static FrameWork *s_inst;

public:
    ~FrameWork() override;
    static FrameWork *inst();

    bool closeAllViews();
    QVector<View *> allViews() const;
    bool restoreViewsFromAutosave();
    void setupView(View *win);
    View *createView(Document *doc);
    View *activeView() const;
    void setActiveView(View *view);

public slots:
    void openDocument(const QString &file);

    void selectionUpdate(const LotList &selection);
    void blockUpdate(bool blocked);
    void modificationUpdate();
    void titleUpdate();

    void fileImportBrickLinkInventory(const BrickLink::Item *item,
                                      const BrickLink::Color *color = nullptr, int quantity = 1,
                                      BrickLink::Condition condition = BrickLink::Condition::New);

signals:
    void viewActivated(View *);
    void viewListChanged();

protected slots:
    void languageChange();

private slots:
    bool updateDatabase(bool forceSync = false);

    void connectView(QWidget *w);
    void transferProgressUpdate(int p, int t);

    void showSettings(const char *page = nullptr);
    void showAddItemDialog();

    void onlineStateChanged(bool isOnline);
    void manageLayouts();

protected:
   void dragEnterEvent(QDragEnterEvent *e) override;
   void dropEvent(QDropEvent *e) override;
   void changeEvent(QEvent *e) override;
   void closeEvent(QCloseEvent *e) override;

private:
    bool checkBrickLinkLogin();

public:
    QList<QAction *> contextMenuActions() const;
    QMultiMap<QString, QString> allActions() const;
    QStringList toolBarActionNames() const;
    QStringList defaultToolBarActionNames() const;
    QAction *findAction(const QString &name) const;
    QAction *findAction(const char *name) const;

private:
    void setupScripts();
    void connectAllActions(bool do_connect, View *view);
    void createActions();
    void translateActions();
    void updateActions();
    QMenu *createPopupMenu() override;
    QMenu *createMenu(const QByteArray &, const QVector<QByteArray> &);
    bool setupToolBar();
    QDockWidget *createDock(QWidget *widget);

    Workspace *m_workspace;
    QPointer<View> m_activeView;
    QList<QAction *> m_extensionContextActions;

    ProgressCircle *m_progress;
    QWidgetAction *m_progressAction;
    QToolBar *m_toolbar;
    QMenu *m_extrasMenu;
    QVector<QDockWidget *> m_dock_widgets;
    TaskInfoWidget *m_task_info;
    TaskPriceGuideWidget *m_task_priceguide;
    TaskAppearsInWidget *m_task_appears;
    QPointer<AddItemDialog> m_add_dialog;
    QPointer<ImportInventoryDialog> m_importinventory_dialog;
    QPointer<ImportOrderDialog> m_importorder_dialog;
    QPointer<ImportCartDialog> m_importcart_dialog;
    QPointer<CheckForUpdates> m_checkForUpdates;
    QPointer<Announcements> m_announcements;
    UndoGroup *m_undogroup;

    friend class DocumentDelegate;
};
