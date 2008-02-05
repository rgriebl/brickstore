/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#ifndef __DLGSUBTRACTITEMIMPL_H__
#define __DLGSUBTRACTITEMIMPL_H__

#include "bricklink.h"
#include "dlgsubtractitem.h"

class QListViewItem;
class CWindow;


class DlgSubtractItemImpl : public DlgSubtractItem {
	Q_OBJECT

public:
	DlgSubtractItemImpl ( const QString &headertext, CWindow *parent, const char *name = 0, bool modal = true, int fl = 0 );
	
	BrickLink::InvItemList items ( ) const;

private slots:
	void docSelected ( QListViewItem *item );

private:
	CWindow *m_window;
};

#endif
