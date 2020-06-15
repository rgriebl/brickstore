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
#include <qlineedit.h>
#include <qvalidator.h>
#include <qpushbutton.h>
#include <q3buttongroup.h>
#include <q3datetimeedit.h>
#include <q3widgetstack.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <q3header.h>
//Added by qt3to4:
#include <Q3ValueList>

#include "clistview.h"
#include "cimport.h"
#include "cprogressdialog.h"
#include "cutility.h"

#include "dlgloadorderimpl.h"


namespace {

template <typename T> static int cmp ( const T &a, const T &b )
{
	if ( a < b )
		return -1;
	else if ( a == b )
		return 0;
	else
		return 1;
}


class OrderListItem : public CListViewItem {
public:
	OrderListItem ( CListView *parent, const QPair<BrickLink::Order *, BrickLink::InvItemList *> &order )
		: CListViewItem ( parent ), m_order ( order )
	{ }

	virtual ~OrderListItem ( )
	{ 
		delete m_order. first;
		delete m_order. second;
	}

	virtual QString text ( int col ) const
	{
		switch ( col ) {
			case 0: return isReceived ( ) ? DlgLoadOrderImpl::tr( "Received" ) : DlgLoadOrderImpl::tr( "Placed" );
			case 1: return m_order. first-> id ( );
			case 2: return m_order. first-> date ( ). date ( ). toString ( Qt::LocalDate );
            case 3: {
                int firstline = m_order. first-> address( ). find ( '\n' );

                if ( firstline <= 0 )
                    return m_order. first-> other ( );
                else
                    return QString( "%1 (%2)" ). arg ( m_order. first-> address( ). left ( firstline )). arg ( m_order. first-> other ( ));
            }
			case 4: return m_order. first-> grandTotal ( ). toLocalizedString ( true );
			default: return QString ( );
		}
	}

    QString toolTip ( int col ) const
    {
        QString tt = text ( col );

        if ( col == 3 && !m_order.first->other().isEmpty()) {
            tt = tt + "\n\n" + m_order.first->address();
        }
        return tt;
    }

	virtual int width ( const QFontMetrics &fm, const Q3ListView *lv, int column ) const
	{
		return CListViewItem::width ( fm, lv, column ) + 20;
	}

	virtual void paintCell ( QPainter *p, const QColorGroup &cg, int col, int w, int align )
	{

		if ( col == 0 ) {
			QColorGroup _cg = cg;
			_cg. setColor ( QColorGroup::Base, CUtility::gradientColor ( backgroundColor ( ), isReceived ( ) ? Qt::green : Qt::blue, 0.2f ));
			_cg. setColor ( QColorGroup::Highlight, CUtility::gradientColor ( cg. highlight ( ), isReceived ( ) ? Qt::green : Qt::blue, 0.2f ));
			
			Q3ListViewItem::paintCell ( p, _cg, col, w, align );
		}
		else {
			CListViewItem::paintCell ( p, cg, col, w, align );
		}
	}

	virtual int compare ( Q3ListViewItem *i, int col, bool ascending ) const 
	{
		OrderListItem *oli = static_cast<OrderListItem *> ( i );

		switch ( col ) {
			case  2: return cmp( m_order. first-> date ( ), oli-> m_order. first-> date ( ));
			case  4: return cmp( m_order. first-> grandTotal ( ), oli-> m_order. first-> grandTotal ( ));
			default: return CListViewItem::compare ( i, col, ascending );
		}
	}

	QPair<BrickLink::Order *, BrickLink::InvItemList *> &order ( )
	{
		return m_order;
	}

private:
	bool isReceived ( ) const 
	{
		return ( m_order. first-> type ( ) == BrickLink::Order::Received );
	}

	QPair<BrickLink::Order *, BrickLink::InvItemList *> m_order;
};

class OrderListToolTip : public QObject/*, public QToolTip*/ {

public:
	OrderListToolTip ( QWidget *parent, CListView *lv )
        : QObject(parent), /*QToolTip( parent ), */
          m_lv ( lv )
    { }
	
	virtual ~OrderListToolTip ( )
	{ }

    void maybeTip ( const QPoint &pos )
	{
        if ( !((QWidget *)QObject::parent()) || !m_lv /*|| !m_iv-> showToolTips ( )*/ )
			return;

		OrderListItem *item = static_cast <OrderListItem *> ( m_lv-> itemAt ( pos ));
		QPoint contentpos = m_lv-> viewportToContents ( pos );
		
		if ( !item )
			return;

		int col = m_lv-> header ( )-> sectionAt ( contentpos. x ( ));
		QString text = item-> toolTip ( col );

		if ( text. isEmpty ( ))
			return;

		QRect r = m_lv-> itemRect ( item );
		int headerleft = m_lv-> header ( )-> sectionPos ( col ) - m_lv-> contentsX ( );
		r. setLeft ( headerleft );
		r. setRight ( headerleft + m_lv-> header ( )-> sectionSize ( col ));
        QToolTip::add((QWidget *)QObject::parent(), r, text);
	}

    void hideTip ( )
    {
        QToolTip::remove ( (QWidget *)QObject::parent());
    }

private:
    CListView *m_lv;
};


} // namespace



int   DlgLoadOrderImpl::s_last_select   = 1;
QDate DlgLoadOrderImpl::s_last_from     = QDate::currentDate ( ). addDays ( -7 );
QDate DlgLoadOrderImpl::s_last_to       = QDate::currentDate ( );
int   DlgLoadOrderImpl::s_last_type     = 1;


DlgLoadOrderImpl::DlgLoadOrderImpl ( QWidget *parent, const char *name, bool modal )
	: DlgLoadOrder ( parent, name, modal )
{
	w_order_number-> setValidator ( new QIntValidator ( 1, 9999999, w_order_number ));

	connect ( w_order_number, SIGNAL( textChanged ( const QString & )), this, SLOT( checkId ( )));
	connect ( w_select_by, SIGNAL( clicked ( int )), this, SLOT( checkId ( )));
	connect ( w_order_from, SIGNAL( valueChanged ( const QDate & )), this, SLOT( checkId ( )));
	connect ( w_order_to, SIGNAL( valueChanged ( const QDate & )), this, SLOT( checkId ( )));

	connect ( w_order_list, SIGNAL( selectionChanged ( )), this, SLOT( checkSelected ( )));
	connect ( w_order_list, SIGNAL( doubleClicked ( Q3ListViewItem *, const QPoint &, int )), this, SLOT( activateItem ( Q3ListViewItem * )));
	connect ( w_order_list, SIGNAL( returnPressed ( Q3ListViewItem * )), this, SLOT( activateItem ( Q3ListViewItem * )));

	w_order_list-> addColumn ( tr( "Type" ));
	w_order_list-> addColumn ( tr( "Order #" ));
	w_order_list-> addColumn ( tr( "Date" ));
	w_order_list-> addColumn ( tr( "Buyer/Seller" ));
	w_order_list-> addColumn ( tr( "Total" ));
	w_order_list-> setColumnAlignment ( 4, Qt::AlignRight );
//	w_order_list-> setShowSortIndicator ( true );
    w_order_list-> setSelectionMode ( Q3ListView::Extended );
	w_order_list-> setShowToolTips ( false );
	(void) new OrderListToolTip ( w_order_list-> viewport ( ), w_order_list );


	connect ( w_next, SIGNAL( clicked ( )), this, SLOT( download ( )));
	connect ( w_back, SIGNAL( clicked ( )), this, SLOT( start ( )));

	w_order_from-> setDate ( s_last_from );
	w_order_to-> setDate ( s_last_to );
	w_order_type-> setCurrentItem ( s_last_type );
	w_select_by-> setButton ( s_last_select );

	start ( );
	resize ( sizeHint ( ));
}

DlgLoadOrderImpl::~DlgLoadOrderImpl ( )
{ }

void DlgLoadOrderImpl::accept ( )
{
	s_last_select = w_select_by-> selectedId ( );
	s_last_from   = w_order_from-> date ( );
	s_last_to     = w_order_to-> date ( );
	s_last_type   = w_order_type-> currentItem ( );

	DlgLoadOrder::accept ( );
}

void DlgLoadOrderImpl::start ( )
{
	w_stack-> raiseWidget ( 0 );

	if ( w_select_by-> selectedId ( ) == 0 )
		w_order_number-> setFocus ( );
	else
		w_order_from-> setFocus ( );

	w_ok-> hide ( );
	w_back-> hide ( );
	w_next-> show ( );
	w_next-> setDefault ( true );

	checkId ( );
}

void DlgLoadOrderImpl::download ( )
{
	CProgressDialog progress ( this );
	CImportBLOrder *import;

	if ( w_select_by-> selectedId ( ) == 0 )
		import = new CImportBLOrder ( w_order_number-> text ( ), orderType ( ), &progress );
	else 
		import = new CImportBLOrder ( w_order_from-> date ( ), w_order_to-> date ( ), orderType ( ), &progress );

	bool ok = ( progress. exec ( ) == QDialog::Accepted );

	if ( ok && !import-> orders ( ). isEmpty ( )) {
		w_stack-> raiseWidget ( 2 );

		const Q3ValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &orderlist = import-> orders ( );

		w_order_list-> clear ( );

		//foreach ( const QPair<BrickLink::Order *, BrickLink::InvItemList *> &order, orderlist ) { // doesn't compile with MSVC.NET
		for ( Q3ValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> >::const_iterator it = orderlist. begin ( ); it != orderlist. end ( ); ++it )
			(void) new OrderListItem ( w_order_list, *it );

		w_order_list-> setSorting ( 2 );
		w_order_list-> setSelected ( w_order_list-> firstChild ( ), true );
		w_order_list-> ensureItemVisible ( w_order_list-> firstChild ( ));
		w_order_list-> setFocus ( );

		w_ok-> show ( );
		w_ok-> setDefault ( true );
		w_next-> hide ( );
		w_back-> show ( );

		checkSelected ( );
	}
	else {
		w_message-> setText ( tr( "There was a problem downloading the data for the specified order(s). This could have been caused by three things:<ul><li>a network error occured.</li><li>the order number and/or type you entered is invalid.</li><li>there are no orders of the specified type in the given time period.</li></ul>" ));
		w_stack-> raiseWidget ( 1 );

		w_ok-> hide ( );
		w_next-> hide ( );
		w_back-> show ( );
		w_back-> setDefault ( true );
	}

	delete import;
}

Q3ValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > DlgLoadOrderImpl::orders ( ) const
{
    Q3ValueList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > list;
    
    for ( Q3ListViewItemIterator it ( w_order_list, Q3ListViewItemIterator::Selected ); it. current ( ); ++it ) {
        list. append ( static_cast<OrderListItem *> ( it. current ( ))-> order ( ));
    }

    return list;
}

BrickLink::Order::Type DlgLoadOrderImpl::orderType ( ) const
{
	switch ( w_order_type-> currentItem ( )) {
		case 2 : return BrickLink::Order::Placed;
		case 1 : return BrickLink::Order::Received;
		case 0 : 
		default: return BrickLink::Order::Any;
	}
}

void DlgLoadOrderImpl::checkId ( )
{
	bool ok = true;

	if ( w_select_by-> selectedId ( ) == 0 ) 
		ok = w_order_number-> hasAcceptableInput ( ) && ( w_order_number-> text ( ). length ( ) >= 6 ) && ( w_order_number-> text ( ). length ( ) <= 7 );
	else
		ok = ( w_order_from-> date ( ) <= w_order_to-> date ( )) && ( w_order_to-> date ( ) <= QDate::currentDate ( ));      

	w_next-> setEnabled ( ok );
}

void DlgLoadOrderImpl::checkSelected ( )
{
    Q3ListViewItemIterator it ( w_order_list, Q3ListViewItemIterator::Selected );
	w_ok-> setEnabled ( it. current ( ));
}

void DlgLoadOrderImpl::activateItem ( Q3ListViewItem * )
{
	checkSelected ( );
	w_ok-> animateClick ( );
}
