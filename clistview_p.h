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
#ifndef __CLISTVIEW_P_H__
#define __CLISTVIEW_P_H__

#include <qdialog.h>

class CListView;
class QListViewItem;
class QPushButton;

class CListViewColumnsDialog : public QDialog {
	Q_OBJECT

public:
	CListViewColumnsDialog ( CListView *parent );

public slots:
	virtual void accept ( );

private slots:
	void upCol ( );
	void downCol ( );
	void showCol ( );
	void hideCol ( );

	void colSelected ( QListViewItem * );

private:
	CListView *m_parent;

	CListView *w_list;
	QPushButton *w_up;
	QPushButton *w_down;
	QPushButton *w_show;
	QPushButton *w_hide;
};

#endif
