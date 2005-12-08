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
#ifndef __CTASKBAR_H__
#define __CTASKBAR_H__

#include <qwidget.h>

class CTaskBarPrivate;
class CTaskGroup;
class QIconSet;

class CTaskBar : public QWidget {
public:
	CTaskBar ( QWidget *parent, const char *name = 0 );
	~CTaskBar ( );

	uint addItem ( QWidget *w, const QIconSet &is, const QString &txt, bool expandible = true, bool special = false );
	void removeItem ( uint i, bool delete_widget = true );

//	void setCondensedMode ( bool );
//	bool condensedMode ( ) const;

	virtual QSize sizeHint ( ) const;

protected:
	friend class CTaskGroup;

	void drawHeaderBackground ( QPainter *p, CTaskGroup *g, bool /*hot*/ );
	void drawHeaderContents ( QPainter *p, CTaskGroup *g, bool hot );
	void recalcLayout ( );
	QSize sizeForGroup ( const CTaskGroup *g ) const;

	void resizeEvent ( QResizeEvent *re );
	void paintEvent ( QPaintEvent *pe );
	void paletteChange ( const QPalette &oldpal );
	void fontChange ( const QFont &oldfont );

private:
	CTaskBarPrivate *d;
};


#endif
