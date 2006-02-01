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
#include <qlistview.h>
#include <qtoolbutton.h>

#include "creport.h"

#include "dlgselectreportimpl.h"


namespace {

class ReportListItem : public QListViewItem {
public:
	ReportListItem ( QListView *list, const CReport *rep )
		: QListViewItem ( list ), m_report ( rep )
	{ }

	QString text ( int index ) const
	{
		if ( m_report && ( index == 0 ))
			return ps2str ( m_report-> pageSize ( ));
		else if ( m_report && ( index == 1 ))
			return m_report-> name ( );
		else
			return QString::null;
	}

	const CReport *report ( ) const
	{
		return m_report;
	}
	
private:
	static QString ps2str ( QPrinter::PageSize ps )
	{
		switch ( ps ) {
			case QPrinter::A4:
				return DlgSelectReportImpl::tr( "A4" );
			case QPrinter::Letter:
				return DlgSelectReportImpl::tr( "Letter" );
			default:
				return DlgSelectReportImpl::tr( "Custom" );
		}
	}

private:
	const CReport *m_report;
};

} //namspace


DlgSelectReportImpl::DlgSelectReportImpl ( QWidget *parent, const char *name, bool modal )
	: DlgSelectReport ( parent, name, modal )
{
	connect ( w_list, SIGNAL( selectionChanged ( QListViewItem * )), this, SLOT( checkItem ( QListViewItem * )));
	connect ( w_list, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( activateItem ( QListViewItem * )));
	connect ( w_list, SIGNAL( returnPressed ( QListViewItem * )), this, SLOT( activateItem ( QListViewItem * )));
	connect ( w_update, SIGNAL( clicked ( )), this, SLOT( updateList ( )));

	w_ok-> setEnabled ( false );

	static bool first_time = true;

	if ( first_time ) {
		CReportManager::inst ( )-> reload ( );
		first_time = false;
	}
	setReports ( CReportManager::inst ( )-> reports ( ));
}

DlgSelectReportImpl::~DlgSelectReportImpl ( )
{ }

void DlgSelectReportImpl::updateList ( )
{
	CReportManager::inst ( )-> reload ( );
	setReports ( CReportManager::inst ( )-> reports ( ));
}

void DlgSelectReportImpl::checkItem ( QListViewItem *it )
{
	w_ok-> setEnabled (( it ));
}

void DlgSelectReportImpl::activateItem ( QListViewItem *it )
{
	checkItem ( it );
	w_ok-> animateClick ( );
}

void DlgSelectReportImpl::setReports ( const QPtrList <CReport> &reps )
{
	w_list-> clear ( );

	for ( QPtrListIterator <CReport> it ( reps ); it. current ( ); ++it ) {
		new ReportListItem ( w_list, it. current ( ));
	}

	if ( w_list-> childCount ( ))
		w_list-> setSelected ( w_list-> firstChild ( ), true );
}

const CReport *DlgSelectReportImpl::report ( ) const
{
	QListViewItem *it = w_list-> selectedItem ( );

	return it ? static_cast <ReportListItem *> ( it )-> report ( ) : 0;
}
