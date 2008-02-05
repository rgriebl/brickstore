/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#ifndef __DLGSELECTREPORTIMPL_H__
#define __DLGSELECTREPORTIMPL_H__

#include <qptrlist.h>

#include "dlgselectreport.h"

class CReport;
class QListViewItem;

class DlgSelectReportImpl : public DlgSelectReport {
	Q_OBJECT

public:
	DlgSelectReportImpl ( QWidget *parent = 0, const char * name = 0, bool modal = true );
	~DlgSelectReportImpl ( );

	void setReports ( const QPtrList <CReport> & );
	const CReport *report ( ) const;

private slots:
	void updateList ( );
	void checkItem ( QListViewItem *it );
	void activateItem ( QListViewItem *it );
};

#endif
