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
#ifndef __CPRICEGUIDEWIDGET_H__
#define __CPRICEGUIDEWIDGET_H__

#include <QFrame>

#include "cmoney.h"
#include "bricklink.h"


class QAction;
class CPriceGuideWidgetPrivate;


class CPriceGuideWidget : public QFrame {
	Q_OBJECT
public:
	CPriceGuideWidget ( QWidget *parent = 0, Qt::WindowFlags f = 0 );
	virtual ~CPriceGuideWidget ( );

	virtual BrickLink::PriceGuide *priceGuide ( ) const;

	enum Layout {
		Normal,
		Horizontal,
		Vertical
	};
	
public slots:
	void setLayout ( Layout l );
	virtual void setPriceGuide ( BrickLink::PriceGuide *pg );

signals:
	void priceDoubleClicked ( money_t p );
	
protected:
	void recalcLayout ( );
	void recalcLayoutNormal ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb );
	void recalcLayoutHorizontal ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb );
	void recalcLayoutVertical ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb );

	virtual void resizeEvent ( QResizeEvent * );
	virtual void mouseDoubleClickEvent ( QMouseEvent * );
	virtual void mouseMoveEvent ( QMouseEvent * );
	virtual void leaveEvent ( QEvent * );
	virtual void paintEvent ( QPaintEvent * );
	virtual bool event ( QEvent * );

protected slots:
	void doUpdate ( );
	void gotUpdate ( BrickLink::PriceGuide *pg );
	void showBLCatalogInfo ( );
	void showBLPriceGuideInfo ( );
	void showBLLotsForSale ( );
	void languageChange ( );
	
private:
	void paintHeader ( QPainter *p, const QRect &r, Qt::Alignment align, const QString &str, bool bold = false );
	void paintCell ( QPainter *p, const QRect &r, Qt::Alignment align, const QString &str, bool alternate = false );

private:
	CPriceGuideWidgetPrivate *d;
};

#endif

