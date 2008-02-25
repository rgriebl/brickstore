/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __CFRAMEWORK_H__
#define __CFRAMEWORK_H__

#include <QMainWindow>
#include <QMap>
#include <QAction>
#include <QStringList>
#include <QPointer>

#include "cdocument.h"
//#include "clistaction.h"
#include "bricklink.h"


class QLabel;
class QMdiArea;
class QMdiSubWindow;

class CMultiProgressBar;
class CWindow;
class CFilterEdit;
class CInfoBar;
class CSpinner;
class CUndoGroup;
class CTaskPaneManager;
class CTaskInfoWidget;
class CTaskPriceGuideWidget;
class CTaskLinksWidget;
class CTaskAppearsInWidget;
class CDocument;
class DAddItem;


class CFrameWork : public QMainWindow {
    Q_OBJECT

private:
    CFrameWork(QWidget *parent = 0, Qt::WindowFlags f = 0);
    static CFrameWork *s_inst;

public:
    ~CFrameWork();
    static CFrameWork *inst();

    void addToRecentFiles(const QString &s);
    QStringList recentFiles() const;

    bool closeAllWindows();

    QList<CWindow *> allWindows() const;

    void updateAllToggleActions(CWindow *window);

    bool eventFilter(QObject *o, QEvent *e);

public slots:
    void selectionUpdate(const CDocument::ItemList &selection);
    void statisticsUpdate();
    void modificationUpdate();
    void titleUpdate();
    void showContextMenu(bool onitem, const QPoint &pos);

    void fileImportBrickLinkInventory(const BrickLink::Item *item);

signals:
    void windowActivated(CWindow *);
    void documentActivated(CDocument *);
    void selectionChanged(CWindow *, const BrickLink::InvItemList &);
    void statisticsChanged(CWindow *);

protected slots:
    void languageChange();

private slots:
    void openDocument(const QString &);

    void fileNew();
    void fileOpen();
    void fileOpenRecent(int);

    void fileImportBrikTrakInventory();
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

    void connectWindow(QMdiSubWindow *w);

    void gotPictureProgress(int p, int t);
    void gotPriceGuideProgress(int p, int t);

    void configure();
    void configure(const char *);

    void setOnlineStatus(QAction *);
    void setSimpleMode(bool);
    void cancelAllTransfers();
    void toggleAddItemDialog(bool b);
    void closedAddItemDialog();

    void registrationUpdate();

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

    QAction *findAction(const QString &);
    QActionGroup *findActionGroup(const QString &);
    void connectAction(bool do_connect, const char *name, CWindow *window, const char *slot, bool (CWindow::* is_on_func)() const = 0);
    void connectAllActions(bool do_connect, CWindow *window);
    void createActions();
    void translateActions();
    QMenu *createMenu(const QString &, const QStringList &);
    bool setupToolBar(QToolBar *, const QStringList &);
    void createStatusBar();
    bool createWindow(CDocument *doc);
    bool createWindows(const QList<CDocument *> &docs);

    QMap<QAction *, bool (CWindow::*)() const> m_toggle_updates;

    QMdiArea * m_mdi;

    CWindow *m_current_window;

    //CMultiProgressBar *m_progress;
    CSpinner *m_spinner;
    CFilterEdit *m_filter;
    QLabel *m_statistics;
    QLabel *m_errors;
    QLabel *m_modified;
    QToolBar *m_toolbar;
    CTaskPaneManager *m_taskpanes;
    CTaskInfoWidget *m_task_info;
    CTaskPriceGuideWidget *m_task_priceguide;
    CTaskLinksWidget *m_task_links;
    CTaskAppearsInWidget *m_task_appears;
    QMenu *m_contextmenu;
    QPointer <DAddItem> m_add_dialog;

    QStringList m_recent_files;

    bool m_running;

    int m_menuid_file;
    int m_menuid_edit;
    int m_menuid_view;
    int m_menuid_extras;
    int m_menuid_window;
    int m_menuid_help;


    CUndoGroup *m_undogroup;
};

#endif
