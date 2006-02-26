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
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qlineedit.h>
#include <qspinbox.h>

#include "cconfig.h"
#include "cselectitem.h"
#include "bricklink.h"

#include "dlgloadinventoryimpl.h"


DlgLoadInventoryImpl::DlgLoadInventoryImpl ( QWidget *parent,  const char *name, bool modal )
	: DlgLoadInventory ( parent, name, modal )
{
	w_select-> setOnlyWithInventory ( true );
	w_select-> setItemType ( BrickLink::inst ( )-> itemType ( CConfig::inst ( )-> readNumEntry ( "/Defaults/ImportInventory/ItemType", 'S' )));
	connect ( w_select, SIGNAL( itemSelected ( const BrickLink::Item *, bool )), this, SLOT( checkItem ( const BrickLink::Item *, bool )));
	w_quantity-> setValue ( 1 );
	w_ok-> setEnabled ( false );

	resize ( sizeHint ( ));
}

DlgLoadInventoryImpl::~DlgLoadInventoryImpl ( )
{ }

bool DlgLoadInventoryImpl::setItem ( const BrickLink::Item *item )
{
	return w_select-> setItem ( item );
}

const BrickLink::Item *DlgLoadInventoryImpl::item ( )
{
	return w_select-> item ( );
}

int DlgLoadInventoryImpl::quantity ( )
{
	return QMAX( 1, w_quantity-> value ( ));
}

void DlgLoadInventoryImpl::checkItem ( const BrickLink::Item *it, bool ok )
{
	bool b = w_select-> isOnlyWithInventory ( ) ? it-> hasInventory ( ) : true;

	w_ok-> setEnabled (( it ) && b );

	if ( it && b && ok )
		w_ok-> animateClick ( );
}
