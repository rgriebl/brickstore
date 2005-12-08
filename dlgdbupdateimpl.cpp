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
#include <qlabel.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qcursor.h>

#include "bricklink.h"
#include "dlgdbupdateimpl.h"


DlgDBUpdateImpl::DlgDBUpdateImpl ( QWidget *parent, const char *name, bool modal, int fl )
	: DlgDBUpdate ( parent, name, modal, fl )
{
	connect ( BrickLink::inst ( ), SIGNAL( databaseUpdated ( bool, const QString & )), this, SLOT( finished ( bool, const QString & )));

	connect ( BrickLink::inst ( ), SIGNAL( databaseProgress ( int, int, const QString & )), this, SLOT( progress ( int, int, const QString & )));

	w_ok-> setEnabled ( false );

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	m_errors = true;

	if ( !BrickLink::inst ( )-> updateDatabase ( ))
		finished ( false, tr( "Update not possible" ));
}

void DlgDBUpdateImpl::progress ( int t, int p, const QString &msg )
{
	w_progress-> setProgress ( t, p );
	w_message-> setText ( msg );
}

void DlgDBUpdateImpl::finished ( bool errors, const QString &msg )
{
	w_progress-> setProgress ( -1, -1 );

	m_errors = errors;

	QString dispmsg;
	if ( errors )
		dispmsg = "<b>" + msg + "</b>";
	else
		dispmsg = msg;
		
	w_message-> setText ( msg );

	w_ok-> setEnabled ( true );
	
	QApplication::restoreOverrideCursor ( );
}

bool DlgDBUpdateImpl::errors ( ) const
{
	return m_errors;
}
