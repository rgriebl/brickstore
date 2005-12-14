/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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

#include <qmainwindow.h>
#include <qptrlist.h>
#include <qmap.h>
#include <qaction.h>
#include <qstringlist.h>

#include "clistaction.h"
#include "bricklink.h"


class QWorkspace;
class QWidgetStack;
class CMultiProgressBar;
class QLabel;

class CWindow;
class CInfoBar;
class CSpinner;


class CFrameWork : public QMainWindow {
	Q_OBJECT

private:
	CFrameWork ( QWidget *parent = 0, const char *name = 0, WFlags fl = 0 );
	static CFrameWork *s_inst;

public:
	~CFrameWork ( );
	static CFrameWork *inst ( );

	void addToRecentFiles ( const QString &s );

	bool closeAllWindows ( );

	QPtrList <CWindow> allWindows ( ) const;

	void updateAllToggleActions ( CWindow *window );

public slots:
	void showInfoForSelection ( const QPtrList<BrickLink::InvItem> &selection );
	void showStatistics ( const QPtrList<BrickLink::InvItem> & );
	void showModification ( bool b );
	void showContextMenu ( bool onitem, const QPoint &pos );

	void fileImportBrickLinkInventory ( const BrickLink::Item *item );

private slots:
	void openDocument ( const QString & );

	void fileNew ( );
	void fileOpen ( );
	void fileOpenRecent ( int );

	void fileImportBrikTrakInventory ( );
	void fileImportBrickLinkInventory ( );
	void fileImportBrickLinkOrder ( );
	void fileImportBrickLinkStore ( );
	void fileImportBrickLinkXML ( );
	void fileImportLDrawModel ( );

	void viewToolBar ( bool );
	void viewInfoBar ( bool );
	void viewStatusBar ( bool );

	void windowActivate ( int );

	void updateDatabase ( );

	void connectWindow ( QWidget *w );

	void gotInventoryProgress ( int p, int t );
	void gotPictureProgress ( int p, int t );
	void gotPriceGuideProgress ( int p, int t );

	void configure ( );
	void configure ( const char * );

	void setOnlineStatus ( QAction * );

	void initBrickLinkDelayed ( );

	void cancelAllTransfers ( );

protected:
	virtual void dragMoveEvent ( QDragMoveEvent *e );
	virtual void dragEnterEvent ( QDragEnterEvent *e );
	virtual void dropEvent ( QDropEvent *e );

	virtual void closeEvent ( QCloseEvent *e );
	virtual void moveEvent ( QMoveEvent *e );
	virtual void resizeEvent ( QResizeEvent *e );

private:
	void setBrickLinkUpdateIntervals ( );
	void setBrickLinkHTTPProxy ( );

	bool checkBrickLinkLogin ( );

private:
	enum { MaxRecentFiles = 9 };

	enum ActionCategory {
		AC_File,
		AC_Edit,
		AC_View,
		AC_Extras,
		AC_Window,
		AC_Help,

		AC_Context,

		AC_Count
	};

private:
	class RecentListProvider : public CListAction::Provider {
	public:
		RecentListProvider ( CFrameWork *fw );
		virtual ~RecentListProvider ( );
		virtual QStringList list ( int &active );

	private:
		CFrameWork *m_fw;
	};
	friend class RecentListProvider;

	class WindowListProvider : public CListAction::Provider {
	public:
		WindowListProvider ( CFrameWork *fw );
		virtual ~WindowListProvider ( );
		virtual QStringList list ( int &active );

	private:
		CFrameWork *m_fw;
	};
	friend class WindowListProvider;


private:
	QIconSet *iconSet ( const char *name );

	QAction *findAction ( const char *name, ActionCategory cat = AC_Count );
	void connectAction ( bool do_connect, const char *name, CWindow *window, const char *slot, bool (CWindow::* is_on_func ) ( ) const = 0 );
	void connectAllActions ( bool do_connect, CWindow *window );
	void createActions ( );
	QPopupMenu *createMenu ( ActionCategory cat, const QStringList & );
	QToolBar *createToolBar ( const QString &label, const QStringList & );
	void createStatusBar ( );
	CWindow *createWindow ( );
	bool showOrDeleteWindow ( CWindow *w, bool b );


	QPtrList<QAction> m_actions [AC_Count];
	QMap<QAction *, bool ( CWindow::* ) ( ) const> m_toggle_updates;

	QWorkspace * m_mdi;

	CWindow *m_current_window;

	CMultiProgressBar *m_progress;
	CSpinner *m_spinner;
	QLabel *m_statistics;
	QLabel *m_errors;
	QLabel *m_modified;
	QToolBar *m_toolbar;
	CInfoBar *m_infobar;
	QPopupMenu *m_contextmenu;

	QStringList m_recent_files;

	QRect m_normal_geometry;
	
	bool m_running;
};


#endif
