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
#ifndef __CWORKSPACE_H__
#define __CWORKSPACE_H__

#include <qworkspace.h>
#include <qwidgetlist.h>
#include <qptrdict.h>
#include <qtabbar.h>

class QToolButton;
class QMainWindow;


class CWorkspace : public QWorkspace {
	Q_OBJECT

public:
	CWorkspace ( QMainWindow *parent, const char *name = 0 );

	bool showTabs ( ) const;
	void setShowTabs ( bool b );

	bool spreadSheetTabs ( ) const;
	void setSpreadSheetTabs ( bool b );

protected:
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual void childEvent ( QChildEvent *e );

private slots:
	void tabClicked ( int );
	void setActiveTab ( QWidget * );

private:
	void refillContainer ( QWidget *container );

private:
	bool               m_showtabs  : 1;
	bool               m_exceltabs : 1;
	QMainWindow *      m_mainwindow;
	QTabBar *          m_tabbar;
	QPtrDict <QTab>    m_widget2tab;
};

#endif

