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
#ifndef __DLGLOADORDERIMPL_H__
#define __DLGLOADORDERIMPL_H__

#include "bricklink.h"

#include "ui_dlgloadorder.h"
//Added by qt3to4:
#include <Q3ValueList>

class DlgLoadOrderImpl : public QDialog, public Ui::DlgLoadOrder {
    Q_OBJECT

public:
    DlgLoadOrderImpl ( QWidget *parent = 0, const char * name = 0, bool modal = true );
    ~DlgLoadOrderImpl ( );

    Q3ValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > orders ( ) const;

protected:
	virtual void accept ( );

protected slots:
	void checkId ( );
	void checkSelected ( );
	void activateItem ( Q3ListViewItem * );

	void start ( );
	void download ( );

private:
	BrickLink::Order::Type orderType ( ) const;

	static int   s_last_select;
	static QDate s_last_from;
	static QDate s_last_to;
	static int   s_last_type;
};

#endif
