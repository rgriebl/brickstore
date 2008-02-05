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
#include <qcombobox.h>
#include <qcheckbox.h>

#include "cconfig.h"

#include "dlgsettopgimpl.h"


DlgSetToPGImpl::DlgSetToPGImpl( QWidget *parent, const char *name, bool modal, WFlags fl )
	: DlgSetToPG ( parent, name, modal, fl )
{
	QStringList timel, pricel;

	timel << tr( "All Time Sales" ) << tr( "Last 6 Months Sales" ) << tr( "Current Inventory" );
	pricel << tr( "Minimum" ) << tr( "Average" ) << tr( "Quantity Average" ) << tr( "Maximum" );

	w_type_time-> insertStringList ( timel );
	w_type_price-> insertStringList ( pricel );

	int timedef = CConfig::inst ( )-> readNumEntry ( "/Defaults/SetToPG/Time", 1 );
	int pricedef = CConfig::inst ( )-> readNumEntry ( "/Defaults/SetToPG/Price", 1 );

	if (( timedef >= 0 ) && ( timedef < int( timel. count ( ))))
		w_type_time-> setCurrentItem ( timedef );
	if (( pricedef >= 0 ) && ( pricedef < int( pricel. count ( ))))
		w_type_price-> setCurrentItem ( pricedef );
}

BrickLink::PriceGuide::Time DlgSetToPGImpl::time ( ) const
{
	return (BrickLink::PriceGuide::Time) w_type_time-> currentItem ( );
}

BrickLink::PriceGuide::Price DlgSetToPGImpl::price ( ) const
{
	return (BrickLink::PriceGuide::Price) w_type_price-> currentItem ( );
}

bool DlgSetToPGImpl::forceUpdate ( ) const
{
	return w_force_update-> isChecked ( );
}


DlgSetToPGImpl::~DlgSetToPGImpl ( )
{
}

