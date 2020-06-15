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
#include <qpushbutton.h>
#include <qlabel.h>
#include <q3buttongroup.h>
#include <qcheckbox.h>

#include "dlgmergeimpl.h"


DlgMergeImpl::DlgMergeImpl ( BrickLink::InvItem *existitem, BrickLink::InvItem *newitem, bool existing_attributes, QWidget *parent, const char *name, bool modal, int fl )
    : DlgMerge ( parent, name, modal, (Qt::WindowType)fl )
{
	QString newcnt = QString( "<b>%1</b>" ). arg( newitem-> quantity ( ));
	QString existcnt = QString( "<b>%1</b>" ). arg( existitem-> quantity ( ));
	QString desc = "<b>";
	
	if ( newitem-> itemType ( )-> hasColors ( )) {
		desc += newitem-> color ( )-> name ( );
		desc += " ";
	}
		
	desc += QString( "%2 [%3]" ). arg( newitem-> item ( )-> name ( ), newitem-> itemType ( )-> name ( ));

	desc += "</b>";

	w_text-> setText ( w_text-> text ( ). arg( newcnt, desc, existcnt ));
	w_existing_or_new-> setButton ( existing_attributes ? 0 : 1 );
}

bool DlgMergeImpl::yesNoToAll ( ) const
{
	return w_always-> isChecked ( );
}

bool DlgMergeImpl::attributesFromExisting ( ) const
{
	return ( w_existing_or_new-> selectedId ( ) == 0 );
}
