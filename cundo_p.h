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
#ifndef __CUNDO_P_H__
#define __CUNDO_P_H__

#include <qaction.h>
//Added by qt3to4:
#include <Q3PopupMenu>
#include <QEvent>
#include <QLabel>

class Q3ListBox;
class Q3ListBoxItem;
class Q3PopupMenu;
class QLabel;
class CUndoManager;
class CUndoStack;


class CUndoAction : public QAction {
	Q_OBJECT

signals:
	void activated ( int );

public slots:
	virtual void setDescription ( const QString &str );

protected:
	virtual void addedTo ( QWidget *w, QWidget *cont );
	virtual bool eventFilter ( QObject *o, QEvent *e );
	virtual bool event ( QEvent * );

protected slots:
	void updateDescriptions ( );
	void fixMenu ( );
	void itemSelected ( Q3ListBoxItem * );
	void selectRange ( Q3ListBoxItem * );
	void setCurrentItemSlot ( Q3ListBoxItem *item );
	void languageChange ( );

private:
	friend class CUndoManager;
	friend class CUndoStack;
	
	enum Type {
		Undo,
		Redo
	};

	CUndoAction ( Type t, QObject *parent, const char *name = 0 );
	
private:
	Type        m_type;
	Q3PopupMenu *m_menu;
	Q3ListBox *  m_list;
	QLabel *    m_label;
	QString     m_desc;

	static const char *s_strings [];
};

#endif
