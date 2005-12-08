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
#include <qlayout.h>
#include <qframe.h>

#include "dlgreportuiimpl.h"


DlgReportUIImpl::DlgReportUIImpl ( QWidget* parent,  const char* name, bool modal, WFlags fl )
	: DlgReportUI ( parent, name, modal, fl )
{
	m_content = 0;
}

DlgReportUIImpl::~DlgReportUIImpl ( )
{
}

void DlgReportUIImpl::setContent ( QWidget *w )
{
	m_content = w;
	w-> reparent ( w_content, 0, QPoint ( 0, 0 ));
	QBoxLayout *lay = new QVBoxLayout ( w_content, 0, 6 );
	lay-> addWidget ( w );
	w-> show ( );

	resize ( sizeHint ( ));
}

QWidget *DlgReportUIImpl::content ( )
{
	return m_content;
}
