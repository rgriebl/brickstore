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
#include <qlineedit.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qstyle.h>
#include <QButtonGroup>
#include <qradiobutton.h>
#include <qvalidator.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qtoolbutton.h>
#include <QWheelEvent>

#include "cconfig.h"
//#include "cresource.h"
#include "cmoney.h"
#include "bricklink.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cpicturewidget.h"
#include "cpriceguidewidget.h"
#include "cappearsinwidget.h"
#include "cselectitem.h"
#include "cselectcolor.h"
#include "DAddItem.h"


class HackSpinBox : public QSpinBox {
public:
	inline QLineEdit *lineEdit ( )
	{ return QSpinBox::lineEdit ( ); }
};


DAddItem::DAddItem ( QWidget *parent, Qt::WindowFlags f )
	: QDialog ( parent, f | Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowContextHelpButtonHint | Qt::WindowSystemMenuHint | Qt::WindowMaximizeButtonHint	 )
{
	setupUi(this);

	m_window = 0;
	m_caption_fmt        = windowTitle ( );
	m_price_label_fmt    = w_label_currency-> text ( );
	m_currency_label_fmt = w_radio_currency-> text ( );

	m_tier_type = new QButtonGroup ( this );
	m_tier_type-> setExclusive ( true );
	m_tier_type-> addButton ( w_radio_percent, 0 );
	m_tier_type-> addButton ( w_radio_currency, 1 );

	m_condition = new QButtonGroup ( this );
	m_condition-> setExclusive ( true );
	m_condition-> addButton ( w_radio_new, BrickLink::New );
	m_condition-> addButton ( w_radio_used, BrickLink::Used );

	
	w_add = new QPushButton ( );
	w_buttons-> addButton ( w_add, QDialogButtonBox::ActionRole );

	w_select_item-> setCurrentItemType ( BrickLink::inst ( )-> itemType ( CConfig::inst ( )-> value ( "/Defaults/AddItems/ItemType", 'P' ). toInt ( )));

	w_picture-> setFrameStyle ( QFrame::StyledPanel | QFrame::Sunken );
	w_picture-> setLineWidth ( 2 );

	w_toggle_picture-> setIcon ( QIcon ( ":/images/sidebar/info" ));

	w_price_guide-> setFrameStyle ( QFrame::StyledPanel | QFrame::Sunken );
	w_price_guide-> setLineWidth ( 2 );

	w_toggle_price_guide-> setIcon ( QIcon ( ":/images/sidebar/priceguide" ));

	w_appears_in-> setFrameStyle ( QFrame::StyledPanel | QFrame::Sunken );
	w_appears_in-> setLineWidth ( 2 );
	w_appears_in-> hide ( );

	w_toggle_appears_in-> setIcon ( QIcon ( ":/images/sidebar/appearsin" ));
	
	static_cast<HackSpinBox *> ( w_qty )-> lineEdit ( )-> setValidator ( new QIntValidator ( 1, 99999, w_qty ));
	w_qty-> setValue ( 1 );

	static_cast<HackSpinBox *> ( w_bulk )-> lineEdit ( )-> setValidator ( new QIntValidator ( 1, 99999, w_bulk ));
	w_bulk-> setValue ( 1 );
	
	w_price-> setValidator ( new CMoneyValidator ( 0.000, 10000, 3, w_price ));
	
	w_tier_qty [0] = w_tier_qty_0;
	w_tier_qty [1] = w_tier_qty_1;
	w_tier_qty [2] = w_tier_qty_2;
	w_tier_price [0] = w_tier_price_0;
	w_tier_price [1] = w_tier_price_1;
	w_tier_price [2] = w_tier_price_2;

	for ( int i = 0; i < 3; i++ ) {
		static_cast<HackSpinBox *> ( w_tier_qty [i] )-> lineEdit ( )-> setValidator ( new QIntValidator ( 0, 99999, w_tier_qty [i] ));
		w_tier_qty [i]-> setValue ( 0 );
		w_tier_price [i]-> setText ( "" );

		connect ( w_tier_qty [i], SIGNAL( valueChanged ( int )), this, SLOT( checkTieredPrices ( )));
		connect ( w_tier_price [i], SIGNAL( textChanged ( const QString & )), this, SLOT( checkTieredPrices ( )));
	}

	m_percent_validator = new QIntValidator ( 1, 99, this );
	m_money_validator = new CMoneyValidator ( 0.001, 10000, 3, this );

	connect ( w_select_item, SIGNAL( hasColors ( bool )), w_select_color, SLOT( setEnabled ( bool )));
	connect ( w_select_item, SIGNAL( itemSelected ( const BrickLink::Item *, bool )), this, SLOT( updateItemAndColor ( )));
	connect ( w_select_color, SIGNAL( colorSelected ( const BrickLink::Color *, bool )), this, SLOT( updateItemAndColor ( )));
	connect ( w_price, SIGNAL( textChanged ( const QString & )), this, SLOT( showTotal ( )));
	connect ( w_qty, SIGNAL( valueChanged ( int )), this, SLOT( showTotal ( )));
	connect ( m_tier_type, SIGNAL( buttonClicked ( int )), this, SLOT( setTierType ( int )));
	connect ( w_bulk, SIGNAL( valueChanged ( int )), this, SLOT( checkAddPossible ( )));

	connect ( w_add, SIGNAL( clicked ( )), this, SLOT( addClicked ( )));

	connect ( w_price_guide, SIGNAL( priceDoubleClicked ( money_t )), this, SLOT( setPrice ( money_t )));

	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( setSimpleMode ( bool )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( updateMonetary ( )));

	updateMonetary ( );

	if ( CConfig::inst ( )-> value ( "/Defaults/AddItems/Condition", "new" ). toString ( ) != "new" ) 
		w_radio_used-> setChecked ( true );
	else
		w_radio_new-> setChecked ( true );

	w_radio_percent-> click ( );

	showTotal ( );
	checkTieredPrices ( );

	setSimpleMode ( CConfig::inst ( )-> simpleMode ( ));

	languageChange ( );
}

void DAddItem::languageChange ( )
{
	Ui::AddItem::retranslateUi ( this );
	updateMonetary ( );
	updateCaption ( );
}

DAddItem::~DAddItem ( )
{
	w_picture-> setPicture ( 0 );
	w_price_guide-> setPriceGuide ( 0 );
	w_appears_in-> setItem ( 0, 0 );
}

void DAddItem::updateCaption ( )
{
	setWindowTitle ( m_caption_fmt. arg ( m_window ? m_window-> document ( )-> title ( ) : QString ( )));
}

void DAddItem::attach ( CWindow *w )
{
	if ( m_window )
		disconnect ( m_window-> document ( ), SIGNAL( titleChanged ( const QString & )), this, SLOT( updateCaption ( )));
	m_window = w;
	if ( m_window )
		connect ( m_window-> document ( ), SIGNAL( titleChanged ( const QString & )), this, SLOT( updateCaption ( )));

	setEnabled ( m_window );

	updateCaption ( );
}

void DAddItem::wheelEvent ( QWheelEvent *e )
{
	if ( e-> modifiers ( ) == Qt::ControlModifier ) {
		double o = windowOpacity ( ) + double( e-> delta ( )) / 1200.0;
		setWindowOpacity ( double( qMin( qMax( o, 0.2 ), 1.0 ))); 

		e-> accept ( );
	}
}

void DAddItem::closeEvent ( QCloseEvent *e )
{
	QDialog::closeEvent ( e );

	if ( e-> isAccepted ( ))
		emit closed ( );
}

void DAddItem::reject ( )
{
	QDialog::reject ( );
	close ( );
}

void DAddItem::setSimpleMode ( bool b )
{
	QWidget *wl [] = {
		w_bulk,
		w_bulk_label,
		w_tier_label,
		w_radio_percent,
		w_radio_currency,
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
		w_bulk-> setValue ( 1 );
		w_tier_qty [0]-> setValue ( 0 );
		w_comments-> setText ( QString ( )); 
		checkTieredPrices ( );
	}
}

void DAddItem::updateMonetary ( )
{
	w_label_currency-> setText ( m_price_label_fmt. arg( CMoney::inst ( )-> currencySymbol ( )));
	w_radio_currency-> setText ( m_currency_label_fmt. arg( CMoney::inst ( )-> currencySymbol ( )));
	w_price-> setText ( money_t( 0 ). toLocalizedString ( true ));
}


void DAddItem::updateItemAndColor ( )
{
	showItemInColor ( w_select_item-> currentItem ( ), w_select_color-> currentColor ( ));
}

void DAddItem::showItemInColor ( const BrickLink::Item *it, const BrickLink::Color *col )
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

void DAddItem::showTotal ( )
{
	money_t tot = 0;

	if ( w_price-> hasAcceptableInput ( ) && w_qty-> hasAcceptableInput ( ))
		tot = money_t::fromLocalizedString ( w_price-> text ( )) * w_qty-> text ( ). toInt ( );

	w_total-> setText ( tot. toLocalizedString ( true ));

	checkAddPossible ( );
}

void DAddItem::setTierType ( int type )
{
	QValidator *valid = ( type == 0 ) ? m_percent_validator : m_money_validator;
	QString text = ( type == 0 ) ? QString( "0" ) : money_t ( 0 ). toLocalizedString ( true );
	
	for ( int i = 0; i < 3; i++ ) {
		w_tier_price [i]-> setValidator ( valid );
		w_tier_price [i]-> setText ( text );
	}
}

void DAddItem::checkTieredPrices ( )
{
	bool valid = true;

	for ( int i = 0; i < 3; i++ ) {
		w_tier_qty [i]-> setEnabled ( valid );

		valid &= ( w_tier_qty [i]-> text ( ). toInt ( ) > 0 );
		
		w_tier_price [i]-> setEnabled ( valid );
	}
	checkAddPossible ( );
}

money_t DAddItem::tierPriceValue ( int i )
{
	if ( i < 0 || i > 2 )
		return 0.;

	money_t val;

	if ( m_tier_type-> checkedId ( ) == 0 ) // %
		val = money_t::fromLocalizedString ( w_price-> text ( )) * ( 100 - w_tier_price [i]-> text ( ). toInt ( )) / 100;
	else // $
		val = money_t::fromLocalizedString ( w_tier_price [i]-> text ( ));

	return val;
}

bool DAddItem::checkAddPossible ( )
{
	bool acceptable = w_select_item-> currentItem ( ) &&
	                  w_select_color-> currentColor ( ) &&
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

void DAddItem::addClicked ( )
{
	if ( !checkAddPossible ( ) || !m_window )
		return;

	BrickLink::InvItem *ii = new BrickLink::InvItem ( w_select_color-> currentColor ( ), w_select_item-> currentItem ( ));

	ii-> setQuantity ( w_qty-> text ( ). toInt ( ));
	ii-> setPrice ( money_t::fromLocalizedString ( w_price-> text ( )));
	ii-> setBulkQuantity ( w_bulk-> text ( ). toInt ( ));
	ii-> setCondition ( static_cast <BrickLink::Condition> ( m_condition-> checkedId ( )));
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

void DAddItem::setPrice ( money_t d )
{
	w_price-> setText ( d. toLocalizedString ( true ));
	checkAddPossible ( );
}
