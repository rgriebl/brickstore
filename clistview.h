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
#ifndef __CLISTVIEW_H__
#define __CLISTVIEW_H__

#include <q3listview.h>
#include <qcolor.h>
#include <qmap.h>
//Added by qt3to4:
#include <QEvent>
#include <QKeyEvent>
#include <Q3PopupMenu>
#include <QPaintEvent>

class QSettings;
class Q3PopupMenu;


class CDisableUpdates {
public:
	CDisableUpdates ( QWidget *w );
	~CDisableUpdates ( );
	void reenable ( );

private:
	QWidget * m_w;
	bool      m_upd_enabled : 1;
	bool      m_reenabled   : 1;
};

 class CListViewColumnsDialog;

class CListView : public Q3ListView {
	Q_OBJECT
public:
	CListView ( QWidget *parent, const char *name = 0 );

	virtual void setSorting ( int column, bool ascending = true );
	virtual void setColumnWidth ( int column, int w );

	void setAlwaysShowSelection ( bool b );
	bool hasAlwaysShowSelection ( ) const;

	void setColumnsHideable ( bool b );
	bool columnsHideable ( ) const;

	bool isColumnVisible ( int ) const;

	void loadSettings ( const QMap <QString, QString> &map );
	QMap <QString, QString> saveSettings ( ) const;

	void centerItem ( const Q3ListViewItem *item );

	bool gridMode ( ) const;
	void setGridMode ( bool b );
	int currentColumn ( ) const;
	void setCurrentColumn ( int col );

	void setColumnAvailable ( int, bool avail );

protected:
	virtual bool event ( QEvent *e );
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual void viewportPaintEvent ( QPaintEvent *e );
	virtual void keyPressEvent( QKeyEvent *e );

protected slots:
	void hideColumn ( int );
	void showColumn ( int );
	void toggleColumn ( int );
	void setColumnVisible ( int, bool visible );

private slots:
	void checkCurrentColumn ( int button, Q3ListViewItem * item, const QPoint & pos, int c );
	void configureColumns ( );

private:
	void recalc_alternate_background ( );
	void recalc_highlight_palette ( );
	void update_column ( int, bool toggle_visible, bool toggle_available );
	
	Q3ListViewItem *m_paint_above;
	Q3ListViewItem *m_paint_current;
	Q3ListViewItem *m_paint_below;
	bool           m_painting;
	QColor         m_alternate_background;

	bool           m_always_show_selection;
	bool           m_use_header_popup;

	bool           m_grid_mode;
	int            m_current_column;

	Q3PopupMenu *   m_header_popup;
	
	struct colinfo {
		bool m_visible   : 1;
		bool m_available : 1;
		uint m_width     : 30;

		inline colinfo ( ) : m_visible ( true ), m_available ( true ), m_width ( 0 ) { }
	};
	
	QMap<int, colinfo>  m_columns;

	friend class CListViewItem;
	friend class CListViewColumnsDialog;
};

class CListViewItem : public Q3ListViewItem {
public:
	CListViewItem ( Q3ListView *parent );
	CListViewItem ( Q3ListViewItem *parent );
	CListViewItem ( Q3ListView *parent, Q3ListViewItem *after );
	CListViewItem ( Q3ListViewItem *parent, Q3ListViewItem *after );

	CListViewItem ( Q3ListView *parent,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( Q3ListViewItem *parent,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( Q3ListView *parent, Q3ListViewItem *after,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( Q3ListViewItem *parent, Q3ListViewItem *after,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );


	bool isAlternate ( );
	virtual const QColor &backgroundColor ( );

	virtual void paintCell ( QPainter *p, const QColorGroup &cg, int column, int width, int alignment );

private:
	void init ( );

private:
	bool m_odd   : 1;
	bool m_known : 1;

	friend class CListView;
};

#endif
