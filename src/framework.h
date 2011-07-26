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
#ifndef __FRAMEWORK_H__
#define __FRAMEWORK_H__

#include <QMainWindow>
#include <QMap>
#include <QAction>
#include <QStringList>
#include <QPointer>

#include "document.h"
#include "bricklinkfwd.h"

class QLabel;
class QMdiSubWindow;
class QToolButton;

class Workspace;
class ProgressCircle;
class Window;
class FilterEdit;
class UndoGroup;
class TaskInfoWidget;
class TaskPriceGuideWidget;
class TaskLinksWidget;
class TaskAppearsInWidget;
class Document;
class AddItemDialog;
class ItemDetailPopup;


class FrameWork : public QMainWindow {
    Q_OBJECT

private:
    FrameWork(QWidget *parent = 0, Qt::WindowFlags f = 0);
    static FrameWork *s_inst;

public:
    ~FrameWork();
    static FrameWork *inst();

    void addToRecentFiles(const QString &s);
    QStringList recentFiles() const;

    bool closeAllWindows();

    QList<Window *> allWindows() const;

    void updateAllToggleActions(Window *window);

public slots:
    void selectionUpdate(const Document::ItemList &selection);
    void statisticsUpdate();
    void modificationUpdate();
    void titleUpdate();
    void showContextMenu(bool onitem, const QPoint &pos);

    void fileImportBrickLinkInventory(const BrickLink::Item *item);

    void toggleItemDetailPopup();

signals:
    void windowActivated(Window *);

protected slots:
    void languageChange();

private slots:
    void openDocument(const QString &);

    void fileNew();
    void fileOpen();
    void fileOpenRecent(int);

    void fileImportBrickLinkInventory();
    void fileImportBrickLinkOrder();
    void fileImportBrickLinkStore();
    void fileImportBrickLinkCart();
    void fileImportBrickLinkXML();
    void fileImportLDrawModel();
    void fileImportPeeronInventory();

    void viewToolBar(bool);
    void viewStatusBar(bool);
    void viewFullScreen(bool);

    bool updateDatabase();

    void connectWindowMdiArea(QMdiSubWindow *w);
    void connectWindow(QWidget *w);

    void transferJobProgressUpdate(int p, int t);

    void configure();
    void configure(const char *);

    void cancelAllTransfers(bool force = false);
    void showAddItemDialog();

    void setItemDetailHelper(Document::Item *docitem);

    void onlineStateChanged(bool isOnline);

    void changeDocumentCurrency(QAction *a);
    void updateCurrencyRates();

protected:
   virtual void dragEnterEvent(QDragEnterEvent *e);
   virtual void dropEvent(QDropEvent *e);

   virtual void closeEvent(QCloseEvent *e);

private:
    void setBrickLinkUpdateIntervals();
    void setBrickLinkHTTPProxy();

    bool checkBrickLinkLogin();
    void createAddItemDialog();

private:
    enum { MaxRecentFiles = 9 };

private:
    QIcon *icon(const char *name);

    QAction *findAction(const char *name);
    void connectAction(bool do_connect, const char *name, Window *window, const char *slot, bool (Window::* is_on_func)() const = 0);
    void connectAllActions(bool do_connect, Window *window);
    void createActions();
    void translateActions();
    QMenu *createMenu(const QByteArray &, const QList<QByteArray> &);
    bool setupToolBar(QToolBar *, const QList<QByteArray> &);
    QDockWidget *createDock(QWidget *widget);
    void createStatusBar();
    bool createWindow(Document *doc);
    bool createWindows(const QList<Document *> &docs);

    QMap<QAction *, bool (Window::*)() const> m_toggle_updates;

    Workspace *m_workspace;

    QPointer<Window> m_current_window;

    ProgressCircle *m_progress;
    FilterEdit *m_filter;
    QLabel *m_st_weight;
    QLabel *m_st_lots;
    QLabel *m_st_items;
    QLabel *m_st_value;
    QLabel *m_st_errors;
    QToolButton *m_st_currency;
    QToolBar *m_toolbar;
    QList<QDockWidget *> m_dock_widgets;
    TaskInfoWidget *m_task_info;
    TaskPriceGuideWidget *m_task_priceguide;
    TaskLinksWidget *m_task_links;
    TaskAppearsInWidget *m_task_appears;
    QMenu *m_contextmenu;
    QPointer <AddItemDialog> m_add_dialog;
    QPointer<ItemDetailPopup> m_details;

    QStringList m_recent_files;
    bool m_running;

    UndoGroup *m_undogroup;
};

#endif
