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
#include <qbuttongroup.h>

#include "dlgsetconditionimpl.h"


DlgSetConditionImpl::DlgSetConditionImpl ( QWidget *parent, const char *name, bool modal, WFlags fl )
	: DlgSetCondition ( parent, name, modal, fl )
{
	w_what-> setButton ( 0 );
}

BrickLink::Condition DlgSetConditionImpl::newCondition ( ) const
{
	BrickLink::Condition c;

	switch ( w_what-> selectedId ( )) {
		case  0: c = BrickLink::New; break;
		case  1: c = BrickLink::Used; break;
		case  2:
		default: c = BrickLink::ConditionCount; break;
	}

	return c;
}

DlgSetConditionImpl::~DlgSetConditionImpl ( )
{
}

