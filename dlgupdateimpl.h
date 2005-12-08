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
#ifndef __DLGUPDATEIMPL_H__
#define __DLGUPDATEIMPL_H__

#include "ctransfer.h"
#include "dlgupdate.h"

class DlgUpdateImplPrivate;


class DlgUpdateImpl : public DlgUpdate {
	Q_OBJECT

public:
	DlgUpdateImpl ( QWidget *parent = 0, const char *name = 0, bool modal = true, int fl = 0 );	
	virtual ~DlgUpdateImpl ( );

private slots:
	void doProgress ( );
	void transferJobFinished ( CTransfer::Job *job );

private:
	class DlgUpdateImplPrivate *d;
};

#endif
