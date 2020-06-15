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
#include <q3listview.h>
#include <qtoolbutton.h>

#include "creport.h"

#include "dlgselectreportimpl.h"


namespace {

//class ReportListItem : public Q3ListViewItem {
//public:
//	ReportListItem ( Q3ListView *list, const CReport *rep )
//		: Q3ListViewItem ( list ), m_report ( rep )
//	{ }

//	QString text ( int /*col*/ ) const
//	{
//		return m_report-> name ( );
//	}

//	const CReport *report ( ) const
//	{
//		return m_report;
//	}
	
//private:
//	const CReport *m_report;
//};

} //namspace


DlgSelectReportImpl::DlgSelectReportImpl ( QWidget *parent, const char *name, bool modal )
	: DlgSelectReport ( parent, name, modal )
{
	connect ( w_list, SIGNAL( selectionChanged ( Q3ListViewItem * )), this, SLOT( checkItem ( Q3ListViewItem * )));
	connect ( w_list, SIGNAL( doubleClicked ( Q3ListViewItem *, const QPoint &, int )), this, SLOT( activateItem ( Q3ListViewItem * )));
	connect ( w_list, SIGNAL( returnPressed ( Q3ListViewItem * )), this, SLOT( activateItem ( Q3ListViewItem * )));
	connect ( w_update, SIGNAL( clicked ( )), this, SLOT( updateList ( )));

	w_ok-> setEnabled ( false );

//	setReports ( CReportManager::inst ( )-> reports ( ));
}

DlgSelectReportImpl::~DlgSelectReportImpl ( )
{ }

void DlgSelectReportImpl::updateList ( )
{
//	CReportManager::inst ( )-> reload ( );
//	setReports ( CReportManager::inst ( )-> reports ( ));
}

void DlgSelectReportImpl::checkItem ( Q3ListViewItem *it )
{
	w_ok-> setEnabled (( it ));
}

void DlgSelectReportImpl::activateItem ( Q3ListViewItem *it )
{
	checkItem ( it );
	w_ok-> animateClick ( );
}

//void DlgSelectReportImpl::setReports ( const Q3PtrList <CReport> &reps )
//{
//	w_list-> clear ( );

//	for ( Q3PtrListIterator <CReport> it ( reps ); it. current ( ); ++it ) {
//		new ReportListItem ( w_list, it. current ( ));
//	}

//	if ( w_list-> childCount ( ))
//		w_list-> setSelected ( w_list-> firstChild ( ), true );
//}

//const CReport *DlgSelectReportImpl::report ( ) const
//{
//	Q3ListViewItem *it = w_list-> selectedItem ( );

//	return it ? static_cast <ReportListItem *> ( it )-> report ( ) : 0;
//}
