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
#include <qtimer.h>

#include "curllabel.h"
#include "dlgmessageimpl.h"

DlgMessageImpl::DlgMessageImpl ( const QString &title, const QString &text, bool delayok, QWidget *parent, const char *name, bool modal, int fl )
	: DlgMessage ( parent, name, modal, fl )
{
	setCaption ( title );
	w_label-> setText ( text );

	if ( delayok ) {
		w_ok-> setEnabled ( false );
		QTimer::singleShot ( 3 * 1000, this, SLOT( enableOk ( )));
	}

	setFixedSize ( sizeHint ( ));
}

void DlgMessageImpl::enableOk ( )
{
	w_ok-> setEnabled ( true );
}

void DlgMessageImpl::reject ( )
{
	if ( w_ok-> isEnabled ( ))
		DlgMessage::reject ( );
}
