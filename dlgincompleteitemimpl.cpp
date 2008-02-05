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

#include <qwidgetstack.h>
#include <qpushbutton.h>
#include <qlabel.h>

#include "curllabel.h"
#include "cselectitem.h"
#include "cselectcolor.h"
#include "dlgincompleteitemimpl.h"


DlgIncompleteItemImpl::DlgIncompleteItemImpl ( BrickLink::InvItem *ii, QWidget *parent, const char *name, bool modal )
	: DlgIncompleteItem ( parent, name, modal )
{
	m_ii = ii;

	w_incomplete-> setText ( createDisplayString ( ii ));

	bool itemok = ( ii-> item ( ));
	bool colorok = ( ii-> color ( ));
	
	w_item_stack-> setShown ( !itemok );

	if ( !itemok ) {
		w_item_stack-> raiseWidget ( 1 );
		w_item_add_info-> setText ( QString( "<a href=\"%1\">%2</a>" ). arg ( BrickLink::inst ( )-> url ( BrickLink::URL_ItemChangeLog, ii-> isIncomplete ( )-> m_item_id. latin1 ( ))). arg ( tr( "BrickLink Item Change Log" )));
	}
	
	w_color_stack-> setShown ( !colorok );

	if ( !colorok ) {
		w_color_stack-> raiseWidget ( 1 );
		w_color_add_info-> setText ( QString( "<a href=\"%1\">%2</a>" ). arg ( BrickLink::inst ( )-> url ( BrickLink::URL_ColorChangeLog )). arg ( tr( "BrickLink Color Change Log" )));
	}
	
	w_ok-> setEnabled ( false );

	connect ( w_fix_color, SIGNAL( clicked ( )), this, SLOT( fixColor ( )));
	connect ( w_fix_item, SIGNAL( clicked ( )), this, SLOT( fixItem ( )));
}

DlgIncompleteItemImpl::~DlgIncompleteItemImpl ( )
{ }

void DlgIncompleteItemImpl::fixItem ( )
{
	const BrickLink::InvItem::Incomplete *inc = m_ii-> isIncomplete ( );
	const BrickLink::ItemType *itt = inc-> m_itemtype_id. isEmpty ( ) ? 0 : BrickLink::inst ( )-> itemType ( inc-> m_itemtype_id [0] );
	const BrickLink::Category *cat = inc-> m_category_id. isEmpty ( ) ? 0 : BrickLink::inst ( )-> category ( inc-> m_category_id. toInt ( ));

	CSelectItemDialog d ( false, this );

	d. setCaption ( tr( "Fix Item" ));
	d. setItemTypeCategoryAndFilter ( itt, cat, inc-> m_item_name );

	if ( d. exec ( ) == QDialog::Accepted ) {
		const BrickLink::Item *it = d. item ( );
	
		m_ii-> setItem ( it );
		
		w_item_fixed-> setText ( tr( "New Item is: %1, %2 %3" ). arg ( it-> itemType ( )-> name ( )). arg ( it-> id ( )). arg ( it-> name ( )));		
		w_item_stack-> raiseWidget ( 0 );

		checkOk ( );
	}
}

void DlgIncompleteItemImpl::fixColor ( )
{
	CSelectColorDialog d ( this );

	d. setCaption ( tr( "Fix Color" ));
	
	if ( d. exec ( ) == QDialog::Accepted ) {
		m_ii-> setColor ( d. color ( ));
		
		w_color_fixed-> setText ( tr( "New color is: %1" ). arg ( d. color ( )-> name ( )));
		
		w_color_stack-> raiseWidget ( 0 );

		checkOk ( );
	}
}

void DlgIncompleteItemImpl::checkOk ( )
{
	bool ok = ( m_ii-> item ( ) && m_ii-> color ( ));

	if ( ok )
		m_ii-> setIncomplete ( 0 );
	
	w_ok-> setEnabled ( ok );
}

QString DlgIncompleteItemImpl::createDisplayString ( BrickLink::InvItem *ii )
{
	const BrickLink::InvItem::Incomplete *inc = ii-> isIncomplete ( );
	
	QString str_qty, str_cat, str_typ, str_itm, str_col;

	// Quantity
	if ( ii-> quantity ( ))
		str_qty = QString::number ( ii-> quantity ( ));

	// Item
	if ( ii-> item ( )) {
		str_itm = QString ( "%1 %2" ). arg( ii-> item ( )-> id ( )). arg( ii-> item ( )-> name ( ));
	}	
	else {
		str_itm = createDisplaySubString ( tr( "Item" ), QString::null, inc-> m_item_id, inc-> m_item_name );
	}

	// Itemtype
	if ( ii-> itemType ( )) {
		str_typ = ii-> itemType ( )-> name ( );
	}	
	else {
		const BrickLink::ItemType *itt = inc-> m_itemtype_id. isEmpty ( ) ? 0 : BrickLink::inst ( )-> itemType ( inc-> m_itemtype_id [0] );
		str_typ = createDisplaySubString ( tr( "Type" ), itt ? QString( itt-> name ( )) : QString::null, inc-> m_itemtype_id, inc-> m_itemtype_name );
	}

	// Category
	if ( ii-> category ( )) {
		str_cat = ii-> category ( )-> name ( );
	}	
	else {
		const BrickLink::Category *cat = inc-> m_category_id. isEmpty ( ) ? 0 : BrickLink::inst ( )-> category ( inc-> m_category_id. toInt ( ));
		str_cat = createDisplaySubString ( tr( "Category" ), cat ? QString( cat-> name ( )) : QString::null, inc-> m_category_id, inc-> m_category_name );
	}

	// Color
	if ( ii-> color ( )) {
		str_col = ii-> color ( )-> name ( );
	}	
	else {
		const BrickLink::Color *col = inc-> m_color_id. isEmpty ( ) ? 0 : BrickLink::inst ( )-> color ( inc-> m_color_id. toInt ( ));
		str_col = createDisplaySubString ( tr( "Color" ), col ? QString( col-> name ( )) : QString::null, inc-> m_color_id, inc-> m_color_name );
	}

	return QString( "<b>%1</b> <b>%2</b>, <b>%3</b> (<b>%4</b>, <b>%5</b>)" ). arg( str_qty ). arg( str_col ). arg( str_itm ). arg( str_typ ). arg( str_cat );
}


QString DlgIncompleteItemImpl::createDisplaySubString ( const QString &what, const QString &realname, const QString &id, const QString &name )
{
	QString str;

	if ( !realname. isEmpty ( )) {
		str = realname;
	}
	else {
		if ( !name. isEmpty ( ))
			str = name;

		if ( !id. isEmpty ( )) {
			if ( !str. isEmpty ( ))
				str += " ";
			str += tr( "[%1]" ). arg( id );
		}

		if ( str. isEmpty ( ))
			str = tr( "Unknown" );

		str. prepend ( what + ": " );
	}
	return str;
}
