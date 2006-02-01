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
#ifndef __CLISTVIEW_H__
#define __CLISTVIEW_H__

#include <qlistview.h>
#include <qcolor.h>
#include <qmap.h>

class QSettings;
class QPopupMenu;


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

class CListView : public QListView {
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

	void centerItem ( const QListViewItem *item );

	bool gridMode ( ) const;
	void setGridMode ( bool b );
	int currentColumn ( ) const;
	void setCurrentColumn ( int col );

protected:
	virtual bool event ( QEvent *e );
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual void viewportPaintEvent ( QPaintEvent *e );
	virtual void keyPressEvent( QKeyEvent *e );

protected slots:
	void hideColumn ( int );
	void hideColumn ( int, bool completly );
	void showColumn ( int );
	void toggleColumn ( int );

private slots:
	void checkCurrentColumn ( int button, QListViewItem * item, const QPoint & pos, int c );
	void configureColumns ( );

private:
	void recalc_alternate_background ( );
	void recalc_highlight_palette ( );

	QListViewItem *m_paint_above;
	QListViewItem *m_paint_current;
	QListViewItem *m_paint_below;
	bool           m_painting;
	QColor         m_alternate_background;

	bool           m_always_show_selection;
	bool           m_use_header_popup;

	bool           m_grid_mode;
	int            m_current_column;

	QPopupMenu *   m_header_popup;
	QMap<int,int>  m_col_width;
	QMap<int,bool> m_completly_hidden;

	friend class CListViewItem;
	friend class CListViewColumnsDialog;
};

class CListViewItem : public QListViewItem {
public:
	CListViewItem ( QListView *parent );
	CListViewItem ( QListViewItem *parent );
	CListViewItem ( QListView *parent, QListViewItem *after );
	CListViewItem ( QListViewItem *parent, QListViewItem *after );

	CListViewItem ( QListView *parent,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( QListViewItem *parent,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( QListView *parent, QListViewItem *after,
		QString, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null,
		QString = QString::null, QString = QString::null );

	CListViewItem ( QListViewItem *parent, QListViewItem *after,
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
