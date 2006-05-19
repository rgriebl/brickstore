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
#ifndef __CAPPEARSINWIDGET_H__
#define __CAPPEARSINWIDGET_H__

#include "clistview.h"
#include "bricklink.h"


class CAppearsInWidgetPrivate;
class QAction;

class CAppearsInWidget : public CListView {
	Q_OBJECT

public:
	CAppearsInWidget ( QWidget *parent = 0, const char *name = 0, WFlags fl = 0 );
	virtual ~CAppearsInWidget ( );
	
	void setItem ( const BrickLink::Item *item, const BrickLink::Color *color = 0 );

	virtual QSize minimumSizeHint ( ) const;
	virtual QSize sizeHint ( ) const;

protected slots:
	void viewLargeImage ( );
	void showBLCatalogInfo ( );
	void showBLPriceGuideInfo ( );
	void showBLLotsForSale ( );
	void languageChange ( );

private slots:
	void showContextMenu ( QListViewItem *, const QPoint & );
	void partOut ( );

private:
	CAppearsInWidgetPrivate *d;
};

#endif
