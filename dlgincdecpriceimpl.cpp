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
#include <qvalidator.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <q3buttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>

#include "cmoney.h"
#include "dlgincdecpriceimpl.h"


DlgIncDecPriceImpl::DlgIncDecPriceImpl ( QWidget* parent, const char* name, bool modal, Qt::WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    setupUi ( this );

	w_inc_dec-> setButton ( 1 );
	w_value-> setText ( "0" );
	w_percent_fixed-> setButton ( 0 );
	w_apply_to_tiers-> setChecked ( false );
	w_currency-> setText ( CMoney::inst ( )-> currencySymbol ( ));

    m_pos_percent_validator = new QDoubleValidator ( 0., 1000., 2, this );
    m_neg_percent_validator = new QDoubleValidator ( 0., 99.99, 2, this );
    m_fixed_validator       = new CMoneyValidator  ( 0,  10000, 3, this );
	
	connect ( w_inc_dec, SIGNAL( clicked ( int )), this, SLOT( slotIncDec ( int )));
	connect ( w_percent_fixed, SIGNAL( clicked ( int )), this, SLOT( slotPercentFixed ( int )));
	connect ( w_value, SIGNAL( textChanged ( const QString & )), this, SLOT( checkValue ( )));

	slotIncDec ( 0 );

    w_value-> selectAll ( );
    w_value-> setFocus ( );
}

DlgIncDecPriceImpl::~DlgIncDecPriceImpl()
{
}

void DlgIncDecPriceImpl::checkValue ( )
{
	w_ok-> setEnabled ( w_value-> hasAcceptableInput ( ));
}

void DlgIncDecPriceImpl::slotIncDec ( int )
{
	slotPercentFixed ( w_percent_fixed-> selectedId ( ));
}

void DlgIncDecPriceImpl::slotPercentFixed ( int i )
{
	if ( i == 0 ) {
		w_value-> setValidator (( w_inc_dec-> selectedId ( ) == 0 ) ? m_pos_percent_validator : m_neg_percent_validator );
		w_value-> setText ( "0" );
	}
	else {
		w_value-> setValidator ( m_fixed_validator );
		w_value-> setText ( money_t ( 0 ). toLocalizedString ( false ));
	}
	checkValue ( );
}

money_t DlgIncDecPriceImpl::fixed ( )
{
	if ( w_value-> hasAcceptableInput ( ) && ( w_percent_fixed-> selectedId ( ) == 1 )) {
		money_t v = money_t::fromLocalizedString ( w_value-> text ( ));
		return ( w_inc_dec-> selectedId ( ) == 0 ) ? v : -v;
	}
	else
		return 0;
}

double DlgIncDecPriceImpl::percent ( )
{
	if ( w_value-> hasAcceptableInput ( ) && ( w_percent_fixed-> selectedId ( ) == 0 )) {
        double v = w_value-> text ( ). replace ( ',', '.' ). toDouble ( );
		return ( w_inc_dec-> selectedId ( ) == 0 ) ? v : -v;
	}
	else
		return 0.;
}

bool DlgIncDecPriceImpl::applyToTiers ( )
{
	return w_apply_to_tiers-> isChecked ( );
}
