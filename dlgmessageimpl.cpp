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
#include <curllabel.h>

#include "dlgmessageimpl.h"

DlgMessageImpl::DlgMessageImpl ( const QString &title, const QString &text, QWidget *parent, const char *name, bool modal, int fl )
	: DlgMessage ( parent, name, modal, fl )
{
	setCaption ( title );
	w_label-> setText ( text );

	setFixedSize ( sizeHint ( ));
}
