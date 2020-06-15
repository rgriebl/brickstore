/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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
#include <QMdiSubWindow>
#include <qmap.h>
#include <qstringlist.h>
#include <qpointer.h>
//Added by qt3to4:
#include <QMoveEvent>
#include <QDragMoveEvent>
#include <QCloseEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <Q3ValueList>
#include <QLabel>
#include <Q3PopupMenu>
#include <QEvent>
#include <QDragEnterEvent>
#include <QToolButton>

#include "cdocument.h"
#include "clistaction.h"
#include "ccheckforupdates.h"
#include "bricklink.h"

class CMultiProgressBar;
class QLabel;

class CWindow;
class CInfoBar;
class CSpinner;
class CWorkspace;
class CTaskPaneManager;
class CTaskInfoWidget;
class CTaskPriceGuideWidget;
class CTaskLinksWidget;
class CTaskAppearsInWidget;
class CDocument;
class DlgAddItemImpl;

class CFrameWork : public QMainWindow {
	Q_OBJECT

private:
	CFrameWork ( QWidget *parent = 0, const char *name = 0, Qt::WFlags fl = 0 );
	static CFrameWork *s_inst;

public:
	~CFrameWork ( );
	static CFrameWork *inst ( );

	void addToRecentFiles ( const QString &s );

    QList <CWindow *> allWindows ( ) const;

	void updateAllToggleActions ( CWindow *window );

public slots:
	void selectionUpdate ( const CDocument::ItemList &selection );
	void statisticsUpdate ( );
    void newVersionAvailable ( );
	void modificationUpdate ( );
	void showContextMenu ( bool onitem, const QPoint &pos );

	void fileImportBrickLinkInventory ( const BrickLink::Item *item );

signals:
	void windowActivated ( CWindow * );
	void documentActivated ( CDocument * );
	void selectionChanged ( CWindow *, const BrickLink::InvItemList & );
	void statisticsChanged ( CWindow * );

protected slots:
	void languageChange ( );

private slots:
	void openDocument ( const QString & );

	void fileNew ( );
	void fileOpen ( );
	void fileOpenRecent ( int );

	void fileImportBrikTrakInventory ( );
	void fileImportBrickLinkInventory ( );
	void fileImportBrickLinkOrder ( );
	void fileImportBrickLinkStore ( );
	void fileImportBrickLinkCart ( );
	void fileImportBrickLinkXML ( );
	void fileImportLDrawModel ( );
	void fileImportPeeronInventory ( );

	void viewToolBar ( bool );
	void viewStatusBar ( bool );
	void viewFullScreen ( bool );

	void windowActivate ( int );

	bool updateDatabase ( );
	
    void connectWindow (QMdiSubWindow *w );
    void setActiveSubWindow(QWidget *window);

	void gotPictureProgress ( int p, int t );
	void gotPriceGuideProgress ( int p, int t );

	void configure ( );
	void configure ( const char * );

    void setOnlineStatus ( QAction * );
    void setWindowMode ( QAction * );
	void setSimpleMode ( bool );
	void cancelAllTransfers ( );
	void toggleAddItemDialog ( bool b );
	void closedAddItemDialog ( );

	void registrationUpdate ( );

protected:
	virtual void dragMoveEvent ( QDragMoveEvent *e );
	virtual void dragEnterEvent ( QDragEnterEvent *e );
	virtual void dropEvent ( QDropEvent *e );

	virtual void closeEvent ( QCloseEvent *e );
	virtual void moveEvent ( QMoveEvent *e );
	virtual void resizeEvent ( QResizeEvent *e );

	virtual bool eventFilter ( QObject *o, QEvent *e );

private:
	void setBrickLinkUpdateIntervals ( );
	void setBrickLinkHTTPProxy ( );

	bool checkBrickLinkLogin ( );

private:
	enum { MaxRecentFiles = 9 };

private:
	class RecentListProvider : public CListAction::Provider {
	public:
		RecentListProvider ( CFrameWork *fw );
		virtual ~RecentListProvider ( );
		virtual QStringList list ( int &active, Q3ValueList <int> & );

	private:
		CFrameWork *m_fw;
	};
	friend class RecentListProvider;

	class WindowListProvider : public CListAction::Provider {
	public:
		WindowListProvider ( CFrameWork *fw );
		virtual ~WindowListProvider ( );
		virtual QStringList list ( int &active, Q3ValueList <int> & );

	private:
		CFrameWork *m_fw;
	};
	friend class WindowListProvider;


private:
	QIcon *iconSet ( const char *name );

    void connectAction ( bool do_connect, const char *name, CWindow *window, const char *slot, bool (CWindow::* is_on_func ) ( ) const = 0 );
	void connectAllActions ( bool do_connect, CWindow *window );
	void createActions ( );
    QMenu *createMenu ( const QStringList & );
    QToolBar *createToolBar ( const QString &label, const QStringList & );
	void createStatusBar ( );
	bool createWindow ( CDocument *doc );
    bool createWindows ( const Q3ValueList<CDocument *> &docs );

    QMap<QAction *, bool ( CWindow::* ) ( ) const> m_toggle_updates;

    QMdiArea *m_mdi;

	CWindow *m_current_window;

	CMultiProgressBar *m_progress;
	CSpinner *m_spinner;
	QLabel *m_statistics;
	QLabel *m_errors;
    QLabel *m_newversion;
	QLabel *m_modified;
    QToolBar *m_toolbar;
	CTaskPaneManager *m_taskpanes;
	CTaskInfoWidget *m_task_info;
	CTaskPriceGuideWidget *m_task_priceguide;
	CTaskLinksWidget *m_task_links;
	CTaskAppearsInWidget *m_task_appears;
    QMenu *m_contextmenu;
	QPointer <DlgAddItemImpl> m_add_dialog;

	QStringList m_recent_files;

	QRect m_normal_geometry;
	QRect m_normal_geometry_adddlg;
	
	bool m_running;

	int m_menuid_file;
	int m_menuid_edit;
	int m_menuid_view;
	int m_menuid_extras;
	int m_menuid_window;
	int m_menuid_help;
};

#endif
