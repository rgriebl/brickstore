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
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qtooltip.h>

#include "cmessagebox.h"
#include "cutility.h"
#include "cconfig.h"
#include "version.h"

#include "dlgregistrationimpl.h"


DlgRegistrationImpl::DlgRegistrationImpl ( bool initial, QWidget *parent, const char *name, bool modal )
	: DlgRegistration ( parent, name, modal )
{
	switch ( CConfig::inst ( )-> registration ( )) {
		case CConfig::None:
		case CConfig::Personal:
			w_personal-> setChecked ( true );
			break;

		case CConfig::Demo:
			w_demo-> setChecked ( true );
			break;

		case CConfig::Full: {
			w_full-> setChecked ( true );
			w_full_name-> setText ( CConfig::inst ( )-> registrationName ( ));
			QString s = CConfig::inst ( )-> registrationKey ( );
			if ( s. isEmpty ( ))
				s = "1234-5678-9ABC";
			w_full_key-> setText ( s );

			w_demo-> setEnabled ( false );
			w_personal-> setEnabled ( false );
			break;
		}
		case CConfig::OpenSource:
			// we shouldn't be here at all
			break;
	}

	if ( !initial ) {
		w_initial_hint-> hide ( );
	}
	else {
		w_initial_hint-> setPalette ( QToolTip::palette ( ));
		w_cancel-> setEnabled ( false );
	}

	resize ( sizeHint ( ));
}

DlgRegistrationImpl::~DlgRegistrationImpl ( )
{ }

void DlgRegistrationImpl::accept ( )
{
	QString name;
	QString key = 0;
	bool ok = true;

	if ( w_personal-> isChecked ( )) {
		name = "PERSONAL";
	}
	else if ( w_demo-> isChecked ( )) {
		name = "DEMO";
	}
	else if ( w_full-> isChecked ( )) {
		name = w_full_name-> text ( );
		key = w_full_key-> text ( );
		ok = CConfig::inst ( )-> checkRegistrationKey ( name, key );
	}

	hide ( );

	if ( !ok ) {
		CMessageBox::information ( this, tr( "Sorry - the registration key you entered is not valid!" ));
		reject ( );
	}
	else {
		CConfig::inst ( )-> setRegistration ( name, key );
		DlgRegistration::accept ( );
	}
}

void DlgRegistrationImpl::reject ( )
{
	if ( w_cancel-> isEnabled ( ))
		DlgRegistration::reject ( );
}
