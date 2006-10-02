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
#ifndef __CPICTUREWIDGET_H__
#define __CPICTUREWIDGET_H__

#include <qframe.h>
#include <qlabel.h>

#include "bricklink.h"


class CPictureWidgetPrivate;
class QAction;


class CPictureWidget : public QFrame {
	Q_OBJECT
public:
	CPictureWidget ( QWidget *parent = 0, const char *name = 0, WFlags fl = 0 );
	virtual ~CPictureWidget ( );
	
	void setPicture ( BrickLink::Picture *pic );
	BrickLink::Picture *picture ( ) const;

protected slots:
	void doUpdate ( );
	void gotUpdate ( BrickLink::Picture * );
	void redraw ( );
	void viewLargeImage ( );
	void showBLCatalogInfo ( );
	void showBLPriceGuideInfo ( );
	void showBLLotsForSale ( );
	void languageChange ( );
	
protected:
	virtual void contextMenuEvent ( QContextMenuEvent *e );
	virtual void mouseDoubleClickEvent ( QMouseEvent *e );
	
private:
	CPictureWidgetPrivate *d;
};


class CLargePictureWidgetPrivate;

class CLargePictureWidget : public QLabel {
	Q_OBJECT
	
public:
	CLargePictureWidget ( BrickLink::Picture *lpic, QWidget *parent );
	virtual ~CLargePictureWidget ( );

protected slots:
	void doUpdate ( );
	void gotUpdate ( BrickLink::Picture * );
	void redraw ( );
	void languageChange ( );

protected:
	virtual void contextMenuEvent ( QContextMenuEvent *e );
	virtual void mouseDoubleClickEvent ( QMouseEvent *e );
	virtual void keyPressEvent ( QKeyEvent *e );

private:
	CLargePictureWidgetPrivate *d;
};

#endif
