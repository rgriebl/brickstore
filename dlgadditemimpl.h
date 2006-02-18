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
#ifndef __DLGADDITEMIMPL_H__
#define __DLGADDITEMIMPL_H__

#include "dlgadditem.h"
#include "cmoney.h"
#include "bricklink.h"

class QValidator;
class CDocument;

class DlgAddItemImpl : public DlgAddItem {
	Q_OBJECT
public:
	DlgAddItemImpl ( QWidget *parent, CDocument *doc, const char *name = 0, bool modal = false, int fl = 0 );
	virtual ~DlgAddItemImpl ( );

signals:
	void addItem ( BrickLink::InvItem *, uint );

protected slots:
	virtual void languageChange ( );

private slots:
	void updateCaption ( );
	void updateItemAndColor ( );
	void showTotal ( );
	bool checkAddPossible ( );
	void addClicked ( );
	void setPrice ( money_t d );
	void checkTieredPrices ( );
	void setTierType ( int id );
	void updateMonetary ( );
	void setSimpleMode ( bool );
	
private:
	void showItemInColor ( const BrickLink::Item *it, const BrickLink::Color *col );
	money_t tierPriceValue ( int i );

private:
	CDocument *m_document;

	QLineEdit *w_tier_qty [3];
	QLineEdit *w_tier_price [3];

	QValidator *m_money_validator;
	QValidator *m_percent_validator;

	QString m_caption_fmt;
	QString m_price_label_fmt;
	QString m_currency_label_fmt;
};

#endif
