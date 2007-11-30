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
#ifndef __DADDITEM_H__
#define __DADDITEM_H__

#include <QDialog>

#include "ui_additem.h"

#include "cmoney.h"
#include "bricklink.h"

class QValidator;
class QPushButton;
class CWindow;


class DAddItem : public QDialog, private Ui::AddItem {
	Q_OBJECT
public:
	DAddItem ( QWidget *parent = 0, Qt::WindowFlags f = 0 );
	virtual ~DAddItem ( );

	void attach ( CWindow *window );

signals:
	void closed ( );

protected slots:
	virtual void languageChange ( );
	virtual void reject ( );

protected:
	virtual void wheelEvent ( QWheelEvent *e );
	virtual void closeEvent ( QCloseEvent *e );

private slots:
	void updateCaption ( );
	void updateItemAndColor ( );
	void showTotal ( );
	bool checkAddPossible ( );
	void addClicked ( );
	void setPrice ( money_t d );
	void checkTieredPrices ( );
	void setTierType ( int type );
	void updateMonetary ( );
	void setSimpleMode ( bool );
	
private:
	void showItemInColor ( const BrickLink::Item *it, const BrickLink::Color *col );
	money_t tierPriceValue ( int i );

private:
	CWindow *m_window;

	QPushButton *w_add;
	QSpinBox *w_tier_qty [3];
	QLineEdit *w_tier_price [3];

	QButtonGroup *m_tier_type;
	QButtonGroup *m_condition;

	QValidator *m_money_validator;
	QValidator *m_percent_validator;

	QString m_caption_fmt;
	QString m_price_label_fmt;
	QString m_currency_label_fmt;
};

#endif
