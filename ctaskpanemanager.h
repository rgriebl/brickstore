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
#ifndef __CTASKPANEMANAGER_H__
#define __CTASKPANEMANAGER_H__

#include <QMainWindow>
#include <qobject.h>
#include <qicon.h>
#include <q3valuelist.h>
#include <qframe.h>
//Added by qt3to4:
#include <Q3PopupMenu>
#include <Q3Action>

class CTaskPane;
class CTaskPaneManagerPrivate;
class QWidget;
class QAction;
class Q3PopupMenu;

class CTaskPaneManager : public QObject {
    Q_OBJECT

public:
    CTaskPaneManager ( QMainWindow *parent, const char *name = 0 );
	~CTaskPaneManager ( );

	enum Mode {
		Classic,
		Modern
	};

	Mode mode ( ) const;
	void setMode ( Mode m );

	void addItem ( QWidget *w, const QIcon &is, const QString &txt = QString ( ));
	void removeItem ( QWidget *w, bool delete_widget = true );

	QString itemText ( QWidget *w ) const;
	void setItemText ( QWidget *w, const QString &txt );

	bool isItemVisible ( QWidget *w ) const;
	void setItemVisible ( QWidget *w, bool  visible );


    QMenu *createItemVisibilityAction (QWidget *parent = 0, const char *name = 0 ) const;
    QMenu *createItemVisibilityMenu ( ) const;

private slots:
	void itemMenuAboutToShow ( );
	void itemMenuActivated ( int );
	void dockVisibilityChanged ( bool b );
	void itemVisibilityChanged ( QWidget *w, bool b );

private:
	void kill ( );
	void create ( );

private:
	CTaskPaneManagerPrivate *d;
};

class CTaskGroup;

class CTaskPane : public QWidget {
    Q_OBJECT
public:
    CTaskPane ( QWidget *parent, const char *name = 0 );
    virtual ~CTaskPane ( );

    void addItem ( QWidget *w, const QIcon &is, const QString &txt, bool expandible = true, bool special = false );
    void removeItem ( QWidget *w, bool delete_widget = true );

    bool isExpanded ( QWidget *w ) const;
    void setExpanded ( QWidget *w, bool exp );

    virtual QSize sizeHint ( ) const;

signals:
    void itemVisibilityChanged ( QWidget *, bool );

protected:
    friend class CTaskGroup;

    void drawHeaderBackground ( QPainter *p, CTaskGroup *g, bool /*hot*/ );
    void drawHeaderContents ( QPainter *p, CTaskGroup *g, bool hot );
    void recalcLayout ( );
    QSize sizeForGroup ( const CTaskGroup *g ) const;

    virtual void resizeEvent ( QResizeEvent *re );
    virtual void paintEvent ( QPaintEvent *pe );
    virtual void contextMenuEvent( QContextMenuEvent *cme );
    virtual void paletteChange ( const QPalette &oldpal );
    virtual void fontChange ( const QFont &oldfont );

private:
    QPixmap            m_lgrad;
    QPixmap            m_rgrad;
    QColor             m_mcol;

    QRect m_margins;
    QFont m_font;
    int m_xh, m_xw;
    int m_vspace, m_hspace;
    int m_arrow;
    bool m_condensed : 1;
    QList <CTaskGroup *> *m_groups;
};

#endif
