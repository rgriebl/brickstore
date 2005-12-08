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
#ifndef __CINFOBAR_H__
#define __CINFOBAR_H__

#include <qdockwindow.h>

#include "cmoney.h"
#include "bricklink.h"

class CInfoBarPrivate;


class CInfoBar : public QDockWindow {
	Q_OBJECT
public:
	CInfoBar ( const QString &title, QWidget *parent, const char *name = 0 );
	virtual ~CInfoBar ( );

	void setPriceGuide ( BrickLink::PriceGuide *pg );
	void setPicture ( BrickLink::Picture *pic );
	void setInfoText ( const QString &text );

	virtual void setOrientation ( Orientation o );
	
	enum Look { Classic, ModernLeft, ModernRight };

public slots:
	void setLook ( int look );

signals:
	void priceDoubleClicked ( money_t p );

protected:
	void paletteChange ( const QPalette &oldpal );

private:
	CInfoBarPrivate *d;
};

#endif
