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
#ifndef __CTASKPANEMANAGER_H__
#define __CTASKPANEMANAGER_H__

#include <qobject.h>
#include <qiconset.h>
#include <qvaluelist.h>
#include <qframe.h>

class CTaskPane;
class QMainWindow;
class QDockWindow;
class QWidget;
class QAction;
class QPopupMenu;

class CTaskPaneManager : public QObject {
	Q_OBJECT

public:
	CTaskPaneManager ( QMainWindow *parent, const char *name = 0 );
	~CTaskPaneManager ( );

	enum Mode {
		Classic,
		Modern
	};

	Mode *mode ( ) const;
	void setMode ( Mode m );

	int addItem ( QWidget *w, const QIconSet &is, const QString &txt );
	void removeItem ( int i, bool delete_widget = true );

	QAction *createItemAction ( QObject *parent = 0, const char *name = 0 ) const;
	QPopupMenu *createItemMenu ( ) const;

	QValueList <int> allItems ( ) const;

	QString label ( int id ) const;
	QIconSet iconSet ( int id ) const;

	QWidget *widget ( int id ) const;
	int id ( QWidget *widget ) const;

	bool isItemVisible ( int id ) const;
	void setItemVisible ( int id, bool  visible );
	

private slots:
	void itemMenuAboutToShow ( );
	void itemMenuActivated ( int );

private:
	void kill ( );
	void create ( );

private:
	Mode         m_mode;
	int          m_idcounter;
	QMainWindow *m_mainwindow;
	QDockWindow *m_panedock;
	CTaskPane   *m_taskpane;

	struct Item {
		QDockWindow *m_itemdock;
		int          m_id;
		uint         m_taskpaneid;
		QWidget *    m_widget;
		QIconSet     m_iconset;
		QString      m_label;
		int          m_orig_framestyle;
		bool         m_expandible       : 1;
		bool         m_expanded         : 1;
		bool         m_special          : 1;
		bool         m_visible          : 1;

		Item ( )
			: m_itemdock ( 0 ), m_id ( -1 ), m_taskpaneid ( 0 ),
			m_widget ( 0 ), m_orig_framestyle ( QFrame::NoFrame ), m_expandible ( false ),
			m_expanded ( false ), m_special ( false ), m_visible ( false )
		{ }

		Item ( const Item &copy )
			: m_itemdock ( copy. m_itemdock ), m_id ( copy. m_id ), m_taskpaneid ( copy. m_taskpaneid ),
			m_widget ( copy. m_widget ), m_iconset ( copy. m_iconset ), m_label ( copy. m_label ),
			m_orig_framestyle ( copy. m_orig_framestyle ), m_expandible ( copy. m_expandible ),
			m_expanded ( copy. m_expanded ), m_special ( copy. m_special ), m_visible ( copy. m_visible )
		{ }
	};

	QValueList<Item> m_items;
};

#endif

