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

QT_FORWARD_DECLARE_CLASS(QLabel);
QT_FORWARD_DECLARE_CLASS(QToolButton);
QT_FORWARD_DECLARE_CLASS(QLineEdit);

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
class ItemDetailPopup;


class FrameWork : public QMainWindow
{
    Q_OBJECT
private:
    FrameWork(QWidget *parent = nullptr);
    static FrameWork *s_inst;

public:
    ~FrameWork();
    static FrameWork *inst();

    bool closeAllWindows();

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
    void filterTextChanged(const QString &filter);

protected slots:
    void languageChange();

private slots:
    void openDocument(const QString &);

    bool updateDatabase();

    void connectWindow(QWidget *w);
    void transferJobProgressUpdate(int p, int t);

    void configure(const char *page = nullptr);

    void cancelAllTransfers(bool force = false);
    void showAddItemDialog();

    void setItemDetailHelper(Document::Item *docitem);

    void onlineStateChanged(bool isOnline);

    void changeDocumentCurrency(QAction *a);
    void updateCurrencyRates();

protected:
   void dragEnterEvent(QDragEnterEvent *e) override;
   void dropEvent(QDropEvent *e) override;
   void changeEvent(QEvent *e) override;
   void closeEvent(QCloseEvent *e) override;

private:
    void setBrickLinkUpdateIntervals();

    bool checkBrickLinkLogin();
    void createAddItemDialog();

private:
    enum { MaxRecentFiles = 9 };

private:
    QIcon *icon(const char *name);

    QAction *findAction(const char *name);
    QList<QAction *> findActions(const char *startsWithName);
    void connectAllActions(bool do_connect, Window *window);
    void createActions();
    void translateActions();
    void updateActions(const Document::ItemList &selection = Document::ItemList());
    QMenu *createMenu(const QByteArray &, const QList<QByteArray> &);
    bool setupToolBar(QToolBar *, const QList<QByteArray> &);
    QDockWidget *createDock(QWidget *widget);
    void createStatusBar();
    bool createWindow(Document *doc);
    QMap<QAction *, bool (Window::*)() const> m_toggle_updates;

    Workspace *m_workspace;

    QPointer<Window> m_current_window;

    ProgressCircle *m_progress;
    QLineEdit *m_filter;
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
    TaskAppearsInWidget *m_task_appears;
    QMenu *m_contextmenu;
    QPointer<AddItemDialog> m_add_dialog;
    QPointer<ItemDetailPopup> m_details;
    QTimer *m_filter_delay = nullptr;
    bool m_running;
    UndoGroup *m_undogroup;

    friend class WelcomeWidget;
    friend class DocumentDelegate;
};
