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
#include <qlineedit.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qstyle.h>
#include <qradiobutton.h>
#include <qvalidator.h>
#include <q3textedit.h>
#include <qpushbutton.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qtoolbutton.h>
//Added by qt3to4:
#include <QCloseEvent>
#include <Q3BoxLayout>
#include <QWheelEvent>
#include <Q3Frame>

#include "cconfig.h"
#include "cresource.h"
#include "cmoney.h"
#include "bricklink.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cpicturewidget.h"
#include "cpriceguidewidget.h"
#include "cappearsinwidget.h"
#include "cselectitem.h"
#include "cselectcolor.h"
#include "dlgadditemimpl.h"



DlgAddItemImpl::DlgAddItemImpl ( QWidget *parent, const char *name, bool modal, int fl )
    : DlgAddItem ( parent, name, modal, (Qt::WindowType)(fl | Qt::WStyle_Customize | Qt::WStyle_Title | Qt::WStyle_ContextHelp | Qt::WStyle_NormalBorder | Qt::WStyle_SysMenu | Qt::WStyle_Maximize) )
{
	m_window = 0;
	m_caption_fmt        = caption ( );
	m_price_label_fmt    = w_label_currency-> text ( );
	m_currency_label_fmt = w_radio_currency-> text ( );

	// Qt3's designer is too stupid...
//	static_cast <QBoxLayout *> ( layout ( ))-> setStretchFactor ( l_top, 100 );
//	l_top-> setStretchFactor ( w_select_item, 100 );
//	l_grid-> setColStretch ( 1, 50 );
//	l_grid-> setColStretch ( 3, 50 );

	w_select_item-> setItemType ( BrickLink::inst ( )-> itemType ( CConfig::inst ( )-> readNumEntry ( "/Defaults/AddItems/ItemType", 'P' )));

	w_picture-> setFrameStyle ( Q3Frame::StyledPanel | Q3Frame::Sunken );
	w_picture-> setLineWidth ( 2 );

	w_toggle_picture-> setPixmap ( CResource::inst ( )-> pixmap ( "sidebar/info" ));

	w_price_guide-> setFrameStyle ( Q3Frame::StyledPanel | Q3Frame::Sunken );
	w_price_guide-> setLineWidth ( 2 );

	w_toggle_price_guide-> setPixmap ( CResource::inst ( )-> pixmap ( "sidebar/priceguide" ));

	w_appears_in-> setFrameStyle ( Q3Frame::StyledPanel | Q3Frame::Sunken );
	w_appears_in-> setLineWidth ( 2 );
	w_appears_in-> hide ( );

	w_toggle_appears_in-> setPixmap ( CResource::inst ( )-> pixmap ( "sidebar/appearsin" ));
	
	w_qty-> setValidator ( new QIntValidator ( 1, 99999, w_qty ));
	w_qty-> setText ( "1" );

	w_bulk-> setValidator ( new QIntValidator ( 1, 99999, w_bulk ));
	w_bulk-> setText ( "1" );
	
	w_price-> setValidator ( new CMoneyValidator ( 0.000, 10000, 3, w_price ));
	
	w_tier_qty [0] = w_tier_qty_0;
	w_tier_qty [1] = w_tier_qty_1;
	w_tier_qty [2] = w_tier_qty_2;
	w_tier_price [0] = w_tier_price_0;
	w_tier_price [1] = w_tier_price_1;
	w_tier_price [2] = w_tier_price_2;

	for ( int i = 0; i < 3; i++ ) {
		w_tier_qty [i]-> setValidator ( new QIntValidator ( 0, 99999, w_tier_qty [i] ));
		w_tier_qty [i]-> setText ( "0" );
		w_tier_price [i]-> setText ( "" );

		connect ( w_tier_qty [i], SIGNAL( textChanged ( const QString & )), this, SLOT( checkTieredPrices ( )));
		connect ( w_tier_price [i], SIGNAL( textChanged ( const QString & )), this, SLOT( checkTieredPrices ( )));
	}

	m_percent_validator = new QIntValidator ( 1, 99, this );
	m_money_validator = new CMoneyValidator ( 0.001, 10000, 3, this );

	connect ( w_select_item, SIGNAL( hasColors ( bool )), w_select_color, SLOT( setEnabled ( bool )));
	connect ( w_select_item, SIGNAL( itemSelected ( const BrickLink::Item *, bool )), this, SLOT( updateItemAndColor ( )));
	connect ( w_select_color, SIGNAL( colorSelected ( const BrickLink::Color *, bool )), this, SLOT( updateItemAndColor ( )));
	connect ( w_price, SIGNAL( textChanged ( const QString & )), this, SLOT( showTotal ( )));
	connect ( w_qty, SIGNAL( textChanged ( const QString & )), this, SLOT( showTotal ( )));
	connect ( w_tier_type, SIGNAL( clicked ( int )), this, SLOT( setTierType ( int )));
	connect ( w_bulk, SIGNAL( textChanged ( const QString & )), this, SLOT( checkAddPossible ( )));

	connect ( w_add, SIGNAL( clicked ( )), this, SLOT( addClicked ( )));

	connect ( w_price_guide, SIGNAL( priceDoubleClicked ( money_t )), this, SLOT( setPrice ( money_t )));

	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( setSimpleMode ( bool )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( updateMonetary ( )));

	updateMonetary ( );

	w_condition-> setButton ( CConfig::inst ( )-> readEntry ( "/Defaults/AddItems/Condition", "new" ) == "new" ? 0 : 1 );
	w_tier_type-> setButton ( 0 );
	setTierType ( 0 );

	showTotal ( );
	checkTieredPrices ( );

	setSimpleMode ( CConfig::inst ( )-> simpleMode ( ));

	languageChange ( );
}

void DlgAddItemImpl::languageChange ( )
{
	DlgAddItem::languageChange ( );
	updateMonetary ( );
	updateCaption ( );
}

DlgAddItemImpl::~DlgAddItemImpl ( )
{
	w_picture-> setPicture ( 0 );
	w_price_guide-> setPriceGuide ( 0 );
	w_appears_in-> setItem ( 0, 0 );
}

void DlgAddItemImpl::updateCaption ( )
{
	setCaption ( m_caption_fmt. arg ( m_window ? m_window-> document ( )-> title ( ) : QString ( )));
}

void DlgAddItemImpl::attach ( CWindow *w )
{
	if ( m_window )
		disconnect ( m_window-> document ( ), SIGNAL( titleChanged ( const QString & )), this, SLOT( updateCaption ( )));
	m_window = w;
	if ( m_window )
		connect ( m_window-> document ( ), SIGNAL( titleChanged ( const QString & )), this, SLOT( updateCaption ( )));

	setEnabled ( m_window );

	updateCaption ( );
}

void DlgAddItemImpl::wheelEvent ( QWheelEvent *e )
{
	if ( e-> state ( ) == Qt::ControlModifier ) {
		double o = windowOpacity ( ) + double( e-> delta ( )) / 1200.0;
		setWindowOpacity ( double( QMIN( QMAX( o, 0.2 ), 1.0 ))); 

		e-> accept ( );
	}
}

void DlgAddItemImpl::closeEvent ( QCloseEvent *e )
{
	DlgAddItem::closeEvent ( e );

	if ( e-> isAccepted ( ))
		emit closed ( );
}

void DlgAddItemImpl::reject ( )
{
	DlgAddItem::reject ( );
	close ( );
}

void DlgAddItemImpl::setSimpleMode ( bool b )
{
	QWidget *wl [] = {
		w_bulk,
		w_bulk_label,
		w_tier_label,
		w_radio_percent,
		w_radio_currency,
		w_tier_type,
		w_tier_price_0,
		w_tier_price_1,
		w_tier_price_2,
		w_tier_qty_0,
		w_tier_qty_1,
		w_tier_qty_2,
		w_comments,
		w_comments_label,

		0
	};

	for ( QWidget **wpp = wl; *wpp; wpp++ ) {
		if ( b )
			( *wpp )-> hide ( );
		else
			( *wpp )-> show ( );
	}

	if ( b ) {
		w_bulk-> setText ( "1" );
		w_tier_qty [0]-> setText ( "0" );
		w_comments-> setText ( QString ( )); 
		checkTieredPrices ( );
	}
}

void DlgAddItemImpl::updateMonetary ( )
{
	w_label_currency-> setText ( m_price_label_fmt. arg( CMoney::inst ( )-> currencySymbol ( )));
	w_radio_currency-> setText ( m_currency_label_fmt. arg( CMoney::inst ( )-> currencySymbol ( )));
	w_price-> setText ( money_t( 0 ). toLocalizedString ( false ));
}


void DlgAddItemImpl::updateItemAndColor ( )
{
	showItemInColor ( w_select_item-> item ( ), w_select_color-> color ( ));
}

void DlgAddItemImpl::showItemInColor ( const BrickLink::Item *it, const BrickLink::Color *col )
{
	if ( it && col ) {
		w_picture-> setPicture ( BrickLink::inst ( )-> picture ( it, col, true ));
		w_price_guide-> setPriceGuide ( BrickLink::inst ( )-> priceGuide ( it, col, true ));
		w_appears_in-> setItem ( it, col );
	}
	else {
		w_picture-> setPicture ( 0 );
		w_price_guide-> setPriceGuide ( 0 );
		w_appears_in-> setItem ( 0, 0 );
	}
	checkAddPossible ( );
}

void DlgAddItemImpl::showTotal ( )
{
	money_t tot = 0;

	if ( w_price-> hasAcceptableInput ( ) && w_qty-> hasAcceptableInput ( ))
		tot = money_t::fromLocalizedString ( w_price-> text ( )) * w_qty-> text ( ). toInt ( );

	w_total-> setText ( tot. toLocalizedString ( false ));

	checkAddPossible ( );
}

void DlgAddItemImpl::setTierType ( int id )
{
	QValidator *valid = ( id == 0 ) ? m_percent_validator : m_money_validator;
	QString text = ( id == 0 ) ? QString( "0" ) : money_t ( 0 ). toLocalizedString ( false );
	
	for ( int i = 0; i < 3; i++ ) {
		w_tier_price [i]-> setValidator ( valid );
		w_tier_price [i]-> setText ( text );
	}
}

void DlgAddItemImpl::checkTieredPrices ( )
{
	bool valid = true;

	for ( int i = 0; i < 3; i++ ) {
		w_tier_qty [i]-> setEnabled ( valid );

		valid &= ( w_tier_qty [i]-> text ( ). toInt ( ) > 0 );
		
		w_tier_price [i]-> setEnabled ( valid );
	}
	checkAddPossible ( );
}

money_t DlgAddItemImpl::tierPriceValue ( int i )
{
	if ( i < 0 || i > 2 )
		return 0.;

	money_t val;

	if ( w_tier_type-> selectedId ( ) == 0 ) // %
		val = money_t::fromLocalizedString ( w_price-> text ( )) * ( 100 - w_tier_price [i]-> text ( ). toInt ( )) / 100;
	else // $
		val = money_t::fromLocalizedString ( w_tier_price [i]-> text ( ));

	return val;
}

bool DlgAddItemImpl::checkAddPossible ( )
{
	bool acceptable = w_select_item-> item ( ) &&
	                  w_select_color-> color ( ) &&
	                  w_price-> hasAcceptableInput ( ) &&
	                  w_qty-> hasAcceptableInput ( ) &&
	                  w_bulk-> hasAcceptableInput ( );

	for ( int i = 0; i < 3; i++ ) {
		if ( !w_tier_price [i]-> isEnabled ( ))
			break;

		acceptable &= w_tier_qty [i]-> hasAcceptableInput ( ) && 
			          w_tier_price [i]-> hasAcceptableInput ( );



		if ( i > 0 )
			acceptable &= ( w_tier_qty [i-1]-> text ( ). toInt ( ) < w_tier_qty [i]-> text ( ). toInt ( )) &&
			              ( tierPriceValue ( i - 1 ) > tierPriceValue ( i ));
		else
			acceptable &= ( money_t::fromLocalizedString ( w_price-> text ( )) > tierPriceValue ( i ));
	}


	w_add-> setEnabled ( acceptable );
	return acceptable;
}

void DlgAddItemImpl::addClicked ( )
{
	if ( !checkAddPossible ( ) || !m_window )
		return;

	BrickLink::InvItem *ii = new BrickLink::InvItem ( w_select_color-> color ( ), w_select_item-> item ( ));

	ii-> setQuantity ( w_qty-> text ( ). toInt ( ));
	ii-> setPrice ( money_t::fromLocalizedString ( w_price-> text ( )));
	ii-> setBulkQuantity ( w_bulk-> text ( ). toInt ( ));
	ii-> setCondition ( w_condition-> selectedId ( ) == 0 ? BrickLink::New : BrickLink::Used );
	ii-> setRemarks ( w_remarks-> text ( ));
	ii-> setComments ( w_comments-> text ( ));

	for ( int i = 0; i < 3; i++ ) {
		if ( !w_tier_price [i]-> isEnabled ( ))
			break;

		ii-> setTierQuantity ( i, w_tier_qty [i]-> text ( ). toInt ( ));
		ii-> setTierPrice ( i, tierPriceValue ( i ));
	}

	m_window-> addItem ( ii, w_merge-> isChecked ( ) ? CWindow::MergeAction_Force | CWindow::MergeKeep_Old : CWindow::MergeAction_None );
}

void DlgAddItemImpl::setPrice ( money_t d )
{
	w_price-> setText ( d. toLocalizedString ( false ));
	checkAddPossible ( );
}
