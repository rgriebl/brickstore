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
#include <qvalidator.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>

#include "dlgloadorderimpl.h"


DlgLoadOrderImpl::DlgLoadOrderImpl ( QWidget *parent, const char *name, bool modal )
	: DlgLoadOrder ( parent, name, modal )
{
	w_number-> setValidator ( new QIntValidator ( 1, 999999, w_number ));
	connect ( w_number, SIGNAL( textChanged ( const QString & )), this, SLOT( checkId ( )));

	w_ok-> setEnabled ( false );

	resize ( sizeHint ( ));
}

DlgLoadOrderImpl::~DlgLoadOrderImpl ( )
{ }

QString DlgLoadOrderImpl::orderId ( ) const
{
	return w_number-> text ( );
}

BrickLink::Order::Type DlgLoadOrderImpl::orderType ( ) const
{
	return ( w_type-> selectedId ( ) == 0 ) ? BrickLink::Order::Received : BrickLink::Order::Placed;
}

void DlgLoadOrderImpl::checkId ( )
{
	w_ok-> setEnabled ( w_number-> hasAcceptableInput ( ) && ( w_number-> text ( ). length ( ) == 6 ));
}

