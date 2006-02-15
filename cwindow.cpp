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
#include <float.h>

#include <qtoolbutton.h>
#include <qcombobox.h>
#include <qpainter.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qfiledialog.h>
#include <qlineedit.h>
#include <qheader.h>
#include <qvalidator.h>
#include <qclipboard.h>
#include <qcursor.h>
#include <qtooltip.h>

#include "cmessagebox.h"
#include "citemview.h"
#include "cresource.h"
#include "cconfig.h"
#include "cframework.h"
#include "cutility.h"
#include "creport.h"
#include "cmoney.h"

#include "dlgincdecpriceimpl.h"
#include "dlgsettopgimpl.h"
#include "dlgloadinventoryimpl.h"
#include "dlgloadorderimpl.h"
#include "dlgselectreportimpl.h"
#include "dlgincompleteitemimpl.h"
#include "dlgadditemimpl.h"
#include "dlgmergeimpl.h"
#include "dlgsubtractitemimpl.h"
#include "dlgsetconditionimpl.h"

#include "cwindow.h"


CWindow::CWindow ( QWidget *parent, const char *name )
	: QWidget ( parent, name, WDestructiveClose ), m_lvitems ( 503 )
{
	m_inventory = 0;
	m_inventory_multiply = 1;
	m_filter_field = CItemView::All;
	m_viewmode = ViewPlain;

	m_settopg_failcnt = 0;
	m_settopg_list = 0;
	m_settopg_time = BrickLink::PriceGuide::AllTime;
	m_settopg_price = BrickLink::PriceGuide::Average;

	m_items. setAutoDelete ( true );
	m_modified = false;

	w_filter_clear = new QToolButton ( this );
	w_filter_clear-> setAutoRaise ( true );
	QToolTip::add ( w_filter_clear, tr( "Reset an active filter" ));

	QIconSet is = CResource::inst ( )-> iconSet ( "filter_clear" );
	if ( is. isNull ( ))
		w_filter_clear-> setTextLabel ( tr( "Clear" ));
	else
		w_filter_clear-> setIconSet ( is );

	w_filter_expression = new QComboBox ( true, this );
	QToolTip::add ( w_filter_expression, tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));

	w_filter_field = new QComboBox ( false, this );
	QToolTip::add ( w_filter_field, tr( "Restrict the filter to this/these field(s)" ));

	w_list = new CItemView ( this );
	setFocusProxy ( w_list );
	w_list-> setShowSortIndicator ( true );
	w_list-> setColumnsHideable ( true );
	w_list-> loadSettings ( CConfig::inst ( ), "/ItemView/List" );
	w_list-> setDifferenceMode ( false );
	w_list-> setSimpleMode ( CConfig::inst ( )-> simpleMode ( ));
	
	connect ( w_list, SIGNAL( selectionChanged ( )), this, SLOT( updateSelection ( )));

	for ( int i = 0; i < CItemView::FilterCountSpecial; i++ ) {
		QString s;
		switch ( -i - 1 ) {
			case CItemView::All       : s = tr( "All" ); break;
			case CItemView::Prices    : s = tr( "All Prices" ); break;
			case CItemView::Texts     : s = tr( "All Texts" ); break;
			case CItemView::Quantities: s = tr( "All Quantities" ); break;
		}
		w_filter_field-> insertItem ( s );
	}

	for ( int i = 0; i < CItemView::FieldCount; i++ ) {
		w_filter_field-> insertItem ( w_list-> header ( )-> label ( i ));
	}

	QBoxLayout *toplay = new QVBoxLayout ( this, 0, 0 );
	QBoxLayout *barlay = new QHBoxLayout ( toplay, 4 );
	barlay-> setMargin ( 4 );
	barlay-> addWidget ( w_filter_clear, 0 );
	barlay-> addWidget ( new QLabel ( tr( "Filter:" ), this ), 0 );
	barlay-> addWidget ( w_filter_expression, 15 );
	barlay-> addWidget ( new QLabel ( tr( "Field:" ), this ), 0 );
	barlay-> addWidget ( w_filter_field, 5 );
	toplay-> addWidget ( w_list );

	connect ( BrickLink::inst ( ), SIGNAL( priceGuideUpdated ( BrickLink::PriceGuide * )), this, SLOT( priceGuideUpdated ( BrickLink::PriceGuide * )));
	connect ( BrickLink::inst ( ), SIGNAL( inventoryUpdated ( BrickLink::Inventory * )), this, SLOT( inventoryUpdated ( BrickLink::Inventory * )));
	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));

	connect ( w_list, SIGNAL( itemChanged ( CItemViewItem *, bool )), this, SLOT( itemModified ( CItemViewItem *, bool )));
	connect ( w_list, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( contextMenu ( QListViewItem *, const QPoint & )));
	connect ( w_filter_clear, SIGNAL( clicked ( )), w_filter_expression, SLOT( clearEdit ( )));
	connect ( w_filter_expression, SIGNAL( textChanged ( const QString & )), this, SLOT( applyFilter ( )));
	connect ( w_filter_field, SIGNAL( activated ( int )), this, SLOT( applyFilter ( )));

	connect ( this, SIGNAL( modificationChanged ( bool )), this, SLOT( updateCaption ( )));

	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), w_list, SLOT( setSimpleMode ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( updateErrorMask ( )));
	connect ( CConfig::inst ( ), SIGNAL( showInputErrorsChanged ( bool )), this, SLOT( updateErrorMask ( )));

	connect ( CConfig::inst ( ), SIGNAL( weightSystemChanged ( CConfig::WeightSystem )), this, SLOT( triggerUpdate ( )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( triggerUpdate ( )));

	updateErrorMask ( );
	updateStatistics ( );
	updateCaption ( );

	w_list-> setFocus ( );
}


CWindow::~CWindow ( )
{
	w_list-> saveSettings ( CConfig::inst ( ), "/ItemView/List" );

	if ( m_inventory ) {
		m_inventory-> release ( );
		m_inventory = 0;
	}
}

void CWindow::applyFilter ( )
{
	w_list-> applyFilter ( w_filter_expression-> lineEdit ( )-> text ( ), w_filter_field-> currentItem ( ), true );
}

void CWindow::updateCaption ( )
{
	QString s = m_caption;

	if ( s. isEmpty ( ))
		s = m_filename;
	if ( s. isEmpty ( ))
		s = tr( "Untitled" );

	if ( isModified ( ))
		s += " *";

	setCaption ( s );
}


void CWindow::pictureUpdated ( BrickLink::Picture *pic )
{
	if ( !pic )
		return;
	for ( QPtrListIterator <BrickLink::InvItem> it ( m_items ); it. current ( ); ++it ) {
		BrickLink::InvItem *ii = it. current ( );

		if (( pic-> item ( ) == ii-> item ( )) && ( pic-> color ( ) == ii-> color ( )))
			m_lvitems [ii]-> repaint ( );
	}
}

const QPtrList<BrickLink::InvItem> &CWindow::items ( ) const
{
	return m_items;
}


uint CWindow::setItems ( const QPtrList<BrickLink::InvItem> &items, int multiply )
{
	w_list-> clear ( );
	m_lvitems. clear ( );
	m_items. clear ( );

	return addItems ( items, multiply, MergeAction_None, true );
}

uint CWindow::addItems ( const QPtrList<BrickLink::InvItem> &items, int multiply, uint globalmergeflags, bool dont_change_sorting )
{
	bool waitcursor = ( items. count ( ) > 100 );
	bool was_empty = ( w_list-> childCount ( ) == 0 );
	uint dropped = 0;
	int merge_action_yes_no_to_all = MergeAction_Ask;


	// disable sorting: new items should appear at the end of the list
	if ( !dont_change_sorting )
		w_list-> setSorting ( w_list-> columns ( ) + 1 ); // +1 because of Qt-bug !!

	QListViewItem *last_item = w_list-> lastItem ( ); // cache the last added item, since QListView::lastItem() is very expensive

	if ( waitcursor )
		QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CDisableUpdates disupd ( w_list );

	for ( QPtrListIterator<BrickLink::InvItem> it ( items ); it. current ( ); ++it ) {
		BrickLink::InvItem *newit = it. current ( );
		BrickLink::InvItem *oldit = 0;

		uint mergeflags = globalmergeflags;

		if ( newit-> isIncomplete ( )) {
			DlgIncompleteItemImpl d ( newit, this );

			if ( waitcursor )
				QApplication::restoreOverrideCursor ( );

			bool drop_this = ( d. exec ( ) != QDialog::Accepted );

			if ( waitcursor )
				QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));
				
			if ( drop_this ) {
				dropped++;
				continue;
			}
		}

		if ( mergeflags != MergeAction_None ) {
			for ( QPtrListIterator<BrickLink::InvItem> it2 ( m_items ); it2. current ( ); ++it2 ) {
				if (( newit-> item ( ) == it2. current ( )-> item ( )) &&
				    ( newit-> color ( ) == it2. current ( )-> color ( ))&& 
				    ( newit-> condition ( ) == it2. current ( )-> condition ( ))) {
					oldit = it2. current ( );
				}
			}
			if ( !oldit )
				mergeflags = MergeAction_None;
		}

		if (( mergeflags & MergeAction_Mask ) == MergeAction_Ask ) {
			if ( merge_action_yes_no_to_all != MergeAction_Ask ) {
				mergeflags = merge_action_yes_no_to_all;
			}
			else {
				DlgMergeImpl d ( oldit, newit, (( mergeflags & MergeKeep_Mask ) == MergeKeep_Old ), this );

				int res = d. exec ( );

				mergeflags = ( res ? MergeAction_Force : MergeAction_None );

				if ( mergeflags != MergeAction_None )
					mergeflags |= ( d. attributesFromExisting ( ) ? MergeKeep_Old : MergeKeep_New );

				if ( d. yesNoToAll ( ))
					merge_action_yes_no_to_all = mergeflags;
			}
		}

		CItemViewItem *ivi = 0;

		switch ( mergeflags & MergeAction_Mask ) {
			case MergeAction_Force:
				oldit-> mergeFrom ( newit, (( mergeflags & MergeKeep_Mask ) == MergeKeep_New ));

				ivi = m_lvitems [oldit];
		
				w_list-> centerItem ( ivi );
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
				break;

			case MergeAction_None:
			default:
				BrickLink::InvItem *ii = new BrickLink::InvItem ( *newit );

				if ( multiply > 1 )
					ii-> setQuantity ( ii-> quantity ( ) * multiply );

				ivi = new CItemViewItem ( ii, w_list, last_item );

				m_items. append ( ii );
				m_lvitems. insert ( ii, ivi );
				last_item = ivi;
				break;
		}
	}

	disupd. reenable ( );

	if ( waitcursor )
		QApplication::restoreOverrideCursor ( );

	updateStatistics ( );
	setModified ( true );

	if ( was_empty )
		w_list-> setCurrentItem ( w_list-> firstChild ( ));

	return items. count ( ) - dropped;
}

void CWindow::addItem ( const BrickLink::InvItem *item, uint mergeflags )
{
	QPtrList <BrickLink::InvItem> tmplist;
	tmplist. append ( item );

	addItems ( tmplist, 1, mergeflags );

	w_list-> ensureItemVisible ( m_lvitems [m_items. last ( )] );
}

void CWindow::deleteItems ( const QPtrList<BrickLink::InvItem> &items )
{
	// If we supply m_selection as 'items' then we would change the
	// list while we are deleting items (since ~QListViewItem) updates
	// the selection! (classic shoot-yourself-in-the-foot...)

	QPtrList<BrickLink::InvItem> copylist ( items );

	for ( QPtrListIterator <BrickLink::InvItem> it ( copylist ); it. current ( ); ++it ) {
		delete m_lvitems. take ( it. current ( ));
		m_items. removeRef ( it. current ( )); // triggers autoDelete
	}
	updateSelection ( );
	updateStatistics ( );
	setModified ( true );
}

void CWindow::mergeItems ( const QPtrList<BrickLink::InvItem> &const_items, int globalmergeflags )
{
	if (( const_items. count ( ) < 2 ) || ( globalmergeflags & MergeAction_Mask ) == MergeAction_None )
		return;

	int merge_action_yes_no_to_all = MergeAction_Ask;


	CDisableUpdates disupd ( w_list );

	QPtrList<BrickLink::InvItem> items ( const_items );

	for ( QPtrListIterator<BrickLink::InvItem> it ( items ); it. current ( ); ) {
		BrickLink::InvItem *from = it. current ( );
		BrickLink::InvItem *to   = 0;

		uint mergeflags = globalmergeflags;
		bool valid = false;

		for ( QPtrListIterator<BrickLink::InvItem> it2 ( m_items ); it2. current ( ); ++it2 ) {
			if ( from == it2. current ( )) {
				valid = true;
				break;
			}
		}

		if ( !valid ) {
			++it;
			continue;
		}

		for ( QPtrListIterator<BrickLink::InvItem> it3 ( items ); it3. current ( ); ++it3 ) {
			BrickLink::InvItem *item = it3. current ( );

			if (( item != from ) &&
			    ( from-> item ( ) == item-> item ( )) &&
				( from-> color ( ) == item-> color ( ))&& 
				( from-> condition ( ) == item-> condition ( ))) {
				to = item;
			}
		}
		if ( !to ) {
			++it;
			continue;
		}

		if (( mergeflags & MergeAction_Mask ) == MergeAction_Ask ) {
			if ( merge_action_yes_no_to_all != MergeAction_Ask ) {
				mergeflags = merge_action_yes_no_to_all;
			}
			else {
				DlgMergeImpl d ( to, from, (( mergeflags & MergeKeep_Mask ) == MergeKeep_Old ), this );

				int res = d. exec ( );

				mergeflags = ( res ? MergeAction_Force : MergeAction_None );

				if ( mergeflags != MergeAction_None )
					mergeflags |= ( d. attributesFromExisting ( ) ? MergeKeep_Old : MergeKeep_New );

				if ( d. yesNoToAll ( ))
					merge_action_yes_no_to_all = mergeflags;
			}
		}

		if (( mergeflags & MergeAction_Mask ) == MergeAction_Force ) {
			bool prefer_from = (( mergeflags & MergeKeep_Mask ) == MergeKeep_New );

			to-> mergeFrom ( from, prefer_from );
	
			CItemViewItem *ivi = m_lvitems [to];
		
			w_list-> centerItem ( ivi );
			ivi-> checkForErrors ( );
			ivi-> repaint ( );

			bool from_is_first = it. atFirst ( );

			if ( !from_is_first )
				--it;

			delete m_lvitems. take ( from );
			m_items. removeRef ( from ); // triggers autoDelete
			items. removeRef ( from );

			if ( from_is_first )
				it. toFirst ( );
			else
				++it;
		}
	}

	disupd. reenable ( );

	updateSelection ( );
	updateStatistics ( );
	setModified ( true );
}


void CWindow::updateSelection ( )
{
	m_selection. clear ( );

	for ( QListViewItemIterator it ( w_list ); it. current ( ); ++it ) {
		CItemViewItem *ivi = static_cast <CItemViewItem *> ( it. current ( ));

		if ( ivi-> isSelected ( ) && ivi-> isVisible ( ))
			m_selection. append ( ivi-> invItem ( ));
	}
	emit selectionChanged ( m_selection ) ;
}

void CWindow::inventoryUpdated ( BrickLink::Inventory *inv )
{
	qWarning ( "got inv update %p, having %p", inv, m_inventory );

	if ( inv && ( inv == m_inventory )) {
		setItems ( inv-> inventory ( ), m_inventory_multiply );

		if ( m_items. isEmpty ( )) {
			if ( inv-> isOrder ( ))
				CMessageBox::information ( this, tr( "The order #%1 could not be retrieved." ). arg ( CMB_BOLD( static_cast <BrickLink::Order *> ( inv )-> id ( ))));
			else
				CMessageBox::information ( this, tr( "The inventory you requested could not be retrieved." ));
		}
		//m_inventory-> release ( );
		//m_inventory = 0;

		setModified ( true );

		resetDifferences ( m_items );
	}
	QApplication::restoreOverrideCursor ( );
}

void CWindow::resetDifferences ( const QPtrList <BrickLink::InvItem> &items )
{
	CDisableUpdates disupd ( w_list );

	for ( QPtrListIterator <BrickLink::InvItem> it ( items ); it. current ( ); ++it ) {
		BrickLink::InvItem *ii = it. current ( );
		bool redraw = false;

		if ( ii-> origQuantity ( ) != ii-> quantity ( )) {
			ii-> setOrigQuantity ( ii-> quantity ( ));
			redraw = true;
		}
		if ( ii-> origPrice ( ) != ii-> price ( )) {
			ii-> setOrigPrice ( ii-> price ( ));
			redraw = true;
		}

		if ( redraw )
			m_lvitems [ii]-> repaint ( );
	}
}

QPtrList <BrickLink::InvItem> CWindow::sortedItems ( )
{
	QPtrList <BrickLink::InvItem> sorted_items;

	for ( QListViewItemIterator it ( w_list ); it. current ( ); ++it )
		sorted_items. append ( static_cast <CItemViewItem *> ( it. current ( ))-> invItem ( ));

	return sorted_items;
}

void CWindow::filePrint ( )
{
	DlgSelectReportImpl d ( this );

	if ( d. exec ( ) == QDialog::Accepted ) {
		const CReport *rep = d. report ( );
		uint pagecnt = rep-> pageCount ( m_items. count ( ));

		QPrinter *prt = CReportManager::inst ( )-> printer ( );

		if ( rep && prt && pagecnt ) {
			//prt-> setOptionEnabled ( QPrinter::PrintToFile, false );
			//prt-> setOptionEnabled ( QPrinter::PrintSelection, true );
		
			prt-> setPageSize ( rep-> pageSize ( ));
			prt-> setFullPage ( true );

			prt-> setMinMax ( 1, pagecnt );
			prt-> setFromTo ( 1, prt-> maxPage ( ));
			prt-> setOptionEnabled ( QPrinter::PrintPageRange, true );

			if ( prt-> setup ( this )) {
				QPainter p;

				if ( !p. begin ( prt ))
					return;
			
				CReportVariables add_vars;
				add_vars ["filename"] = m_filename;

				if ( m_inventory && m_inventory-> isOrder ( )) {
					BrickLink::Order *order = static_cast <BrickLink::Order *> ( m_inventory );

					add_vars ["order-id"]            = order-> id ( );
					add_vars ["order-type"]          = ( order-> type ( ) == BrickLink::Order::Placed ? tr( "Placed" ) : tr( "Received" ));
					add_vars ["order-date"]          = order-> date ( ). toString ( Qt::TextDate );
					add_vars ["order-status-change"] = order-> statusChange ( ). toString ( Qt::TextDate );
					add_vars ["order-buyer"]         = order-> buyer ( );
					add_vars ["order-shipping"]      = order-> shipping ( ). toCString ( );
					add_vars ["order-insurance"]     = order-> insurance ( ). toCString ( );
					add_vars ["order-delivery"]      = order-> delivery ( ). toCString ( );
					add_vars ["order-credit"]        = order-> credit ( ). toCString ( );
					add_vars ["order-grand-total"]   = order-> grandTotal ( ). toCString ( );
					add_vars ["order-status"]        = order-> status ( );
					add_vars ["order-payment"]       = order-> payment ( );
					add_vars ["order-remarks"]       = order-> remarks ( );
				}
			
				rep-> render ( sortedItems ( ), add_vars, prt-> fromPage ( ), prt-> toPage ( ), &p );
			}
		}
	}
}

bool CWindow::fileOpen ( )
{
	QStringList filters;
	filters << tr( "BrikTrak Inventory" ) + " (*.bti)";
	filters << tr( "All Files" ) + "(*.*)";

	return fileOpen ( QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), this, "FileDialog", tr( "Open File" ), 0 ));
}

bool CWindow::fileOpen ( const QString &s )
{
	return ( !s. isEmpty ( ) && fileLoadFrom ( s, 'B' ));
}

bool CWindow::fileImportBrickLinkInventory ( const BrickLink::Item *preselect )
{
	DlgLoadInventoryImpl dlg ( this );

	if ( preselect )
		dlg. setItem ( preselect );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		const BrickLink::Item *it = dlg. item ( );
		int qty = dlg. quantity ( );

		if ( it && ( qty > 0 )) {
			if ( dlg. importFromPeeron ( ))
				m_inventory = BrickLink::inst ( )-> inventoryFromPeeron ( it );
			else
				m_inventory = BrickLink::inst ( )-> inventory ( it );

			if ( m_inventory ) {
				QApplication::setOverrideCursor ( QCursor ( Qt::BusyCursor ));

				m_inventory-> addRef ( );
				m_inventory_multiply = qty;

				if ( m_inventory-> valid ( ) && dlg. updateAlways ( ))
					m_inventory-> update ( );

				setModified ( true );

				if ( m_inventory-> updateStatus ( ) != BrickLink::Updating )
					inventoryUpdated ( m_inventory );

				m_caption = tr( "Inventory for %1" ). arg ( it-> id ( ));
				updateCaption ( );
				return true;
			}
			else
				CMessageBox::warning ( this, tr( "Internal error: Could not create an Inventory object for item %1" ). arg ( CMB_BOLD( it-> id ( ))));
		}
		else
			CMessageBox::warning ( this, tr( "Requested item was not found in the database." ));
	}
	return false;
}

bool CWindow::fileImportBrickLinkOrder ( )
{
	DlgLoadOrderImpl dlg ( this );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		QString id = dlg. orderId ( );
		BrickLink::Order::Type type = dlg. orderType ( );

		if ( id. length ( ) == 6 ) {
			m_inventory = BrickLink::inst ( )-> order ( id, type );

			if ( m_inventory ) {
				QApplication::setOverrideCursor ( QCursor ( Qt::BusyCursor ));

				m_inventory-> addRef ( );
				m_inventory_multiply = 1;

				setModified ( true );

				if ( m_inventory-> updateStatus ( ) != BrickLink::Updating )
					inventoryUpdated ( m_inventory );

				m_caption = tr( "Order #%1" ). arg ( id );
				updateCaption ( );
				return true;
			}
			else
				CMessageBox::warning ( this, tr( "Internal error: Could not create an Inventory object for order #%1" ). arg ( CMB_BOLD( id )));
		}
		else
			CMessageBox::warning ( this, tr( "Invalid order number." ));
	}
	return false;
}

bool CWindow::fileImportBrickLinkStore ( )
{
	m_inventory = BrickLink::inst ( )-> storeInventory ( );

	if ( m_inventory ) {
		QApplication::setOverrideCursor ( QCursor ( Qt::BusyCursor ));

		m_inventory-> addRef ( );
		m_inventory_multiply = 1;

		setModified ( true );

		if ( m_inventory-> updateStatus ( ) != BrickLink::Updating )
			inventoryUpdated ( m_inventory );

		m_caption = tr( "Store %1" ). arg ( QDate::currentDate ( ). toString ( Qt::LocalDate ));
		updateCaption ( );
		return true;
	}
	else
		CMessageBox::warning ( this, tr( "Internal error: Could not create an Inventory object for store inventory" ));
	return false;
}

bool CWindow::fileImportBrickLinkXML ( )
{
	QStringList filters;
	filters << tr( "BrickLink XML Inventory" ) + " (*.xml)";
	filters << tr( "All Files" ) + "(*.*)";

	QString s = QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), this, "FileDialog", tr( "Import File" ), 0 );

	bool b = ( !s. isEmpty ( ) && fileLoadFrom ( s, 'X', true ));

	if ( b ) {
		m_caption = tr( "Import of %1" ). arg ( QFileInfo ( s ). fileName ( ));
		updateCaption ( );
	}
	return b;
}

bool CWindow::fileLoadFrom ( const QString &name, char type, bool import_only )
{
	QFile f ( name );

	if ( !f. open ( IO_ReadOnly )) {
		CMessageBox::warning ( this, tr( "Could not open file %1 for reading." ). arg ( CMB_BOLD( name )));
		return false;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	uint invalid_items = 0;
	QPtrList<BrickLink::InvItem> *items = 0;

	QString emsg;
	int eline = 0, ecol = 0;
	QDomDocument doc;

	if ( doc. setContent ( &f, &emsg, &eline, &ecol )) {
		QDomElement root = doc. documentElement ( );

		items = BrickLink::inst ( )-> parseItemListXML ( root, type == 'B' ? BrickLink::XMLHint_BrikTrak : BrickLink::XMLHint_MassUpload, &invalid_items );
	}
	else {
		CMessageBox::warning ( this, tr( "Could not parse the XML data in file %1:<br /><i>Line %2, column %3: %4</i>" ). arg ( CMB_BOLD( name )). arg ( eline ). arg ( ecol ). arg ( emsg ));
		QApplication::restoreOverrideCursor ( );
		return false;
	}

	QApplication::restoreOverrideCursor ( );

	if ( items ) {
		if ( invalid_items )
			CMessageBox::information ( this, tr( "This file contains %1 unknown item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));

		setItems ( *items );
		delete items;

		setFileName ( import_only ? QString::null : name );
		setModified ( import_only );

		if ( !import_only )
			CFrameWork::inst ( )-> addToRecentFiles ( name );

		resetDifferences ( m_items );

		return true;
	}
	else {
		CMessageBox::warning ( this, tr( "Could not parse the XML data in file %1." ). arg ( CMB_BOLD( name )));
		return false;
	}
}

bool CWindow::fileImportLDrawModel ( )
{
	QStringList filters;
	filters << tr( "LDraw Models" ) + " (*.dat;*.ldr;*.mpd)";
	filters << tr( "All Files" ) + "(*.*)";

	QString s = QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), this, "FileDialog", tr( "Import File" ), 0 );

	if ( s. isEmpty ( ))
		return false;

	QFile f ( s );

	if ( !f. open ( IO_ReadOnly )) {
		CMessageBox::warning ( this, tr( "Could not open file %1 for reading." ). arg ( CMB_BOLD( s )));
		return false;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

    uint invalid_items = 0;
    QPtrList <BrickLink::InvItem> items;
    items. setAutoDelete ( true );

	bool b = BrickLink::inst ( )-> parseLDrawModel ( f, items, &invalid_items );

	QApplication::restoreOverrideCursor ( );

	if ( b && !items. isEmpty ( )) {
		if ( invalid_items )
			CMessageBox::information ( this, tr( "This file contains %1 unknown item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));

		setItems ( items );

		setFileName ( QString::null );
		setModified ( true );
		m_caption = tr( "Import of %1" ). arg ( QFileInfo ( s ). fileName ( ));
		updateCaption ( );

		resetDifferences ( m_items );

		return true;
	}
	else {
		CMessageBox::warning ( this, tr( "Could not parse the LDraw model in file %1." ). arg ( CMB_BOLD( s )));
		return false;
	}
}


void CWindow::fileSave ( )
{
	if ( fileName ( ). isEmpty ( ))
		fileSaveAs ( );
	else if ( isModified ( ))
		fileSaveTo ( fileName ( ), 'B' );
}

void CWindow::fileSaveAs ( )
{
	QStringList filters;
	filters << tr( "BrikTrak Inventory" ) + " (*.bti)";

	QString fn = fileName ( );

	if ( fn. isEmpty ( )) {
		QDir d ( CConfig::inst ( )-> documentDir ( ));

		if ( d. exists ( ))
			fn = d. filePath ( m_caption );
	}

	fn = QFileDialog::getSaveFileName ( fn, filters. join ( ";;" ), this, "FileDialog", tr( "Save File as" ), 0 );

	if ( !fn. isNull ( )) {
		if ( fn. right ( 4 ) != ".bti" )
			fn += ".bti";

		if ( QFile::exists ( fn ) &&
		     CMessageBox::question ( this, tr( "A file named %1 already exists. Are you sure you want to overwrite it?" ). arg( CMB_BOLD( fn )), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		     return;

		fileSaveTo ( fn, 'B' );
	}
}

bool CWindow::fileSaveTo ( const QString &s, char type, bool export_only )
{
	QFile f ( s );
	if ( f. open ( IO_WriteOnly )) {
		QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

		QDomDocument doc ( QString::null );
		QPtrList <BrickLink::InvItem> si = export_only ? sortedItems ( ) : items ( );

		doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, type == 'B' ? BrickLink::XMLHint_BrikTrak : BrickLink::XMLHint_MassUpload, &si ));

		QCString output = doc. toCString ( );
		bool ok = ( f. writeBlock ( output. data ( ), output. size ( ) - 1 ) ==  int( output. size ( ) - 1 )); // no 0-byte

		QApplication::restoreOverrideCursor ( );

		if ( ok ) {
			if ( !export_only ) {
				setModified ( false );
				setFileName ( s );

				CFrameWork::inst ( )-> addToRecentFiles ( s );
			}
			return true;
		}
		else
			CMessageBox::warning ( this, tr( "Failed to save data in file %1." ). arg ( CMB_BOLD( s )));
	}
	else
		CMessageBox::warning ( this, tr( "Failed to open file %1 for writing." ). arg ( CMB_BOLD( s )));

	return false;
}

void CWindow::updateErrorMask ( )
{
	Q_UINT64 em = 0;

	if ( CConfig::inst ( )-> showInputErrors ( )) {
		if ( CConfig::inst ( )-> simpleMode ( )) {
			em = 1ULL << CItemView::Status     | \
				1ULL << CItemView::Picture     | \
				1ULL << CItemView::PartNo      | \
				1ULL << CItemView::Description | \
				1ULL << CItemView::Condition   | \
				1ULL << CItemView::Color       | \
				1ULL << CItemView::Quantity    | \
				1ULL << CItemView::Remarks     | \
				1ULL << CItemView::Category    | \
				1ULL << CItemView::ItemType    | \
				1ULL << CItemView::Weight      | \
				1ULL << CItemView::YearReleased;
		}
		else {
			em = ( 1ULL << CItemView::FieldCount ) - 1;
		}
	}

	w_list-> setErrorMask ( em );
	triggerStatisticsUpdate ( );
}

void CWindow::fileExportBrickLinkInvReqClipboard ( )
{
	if ( w_list-> errorCount ( ) && CMessageBox::warning ( this, tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		return;

	QDomDocument doc ( QString::null );
	QPtrList <BrickLink::InvItem> si = sortedItems ( );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_Inventory, &si ));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryRequest ));
}

void CWindow::fileExportBrickLinkWantedListClipboard ( )
{
	if ( w_list-> errorCount ( ) && CMessageBox::warning ( this, tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		return;

	QString wantedlist;

	if ( CMessageBox::getString ( this, tr( "Enter the ID number of Wanted List (leave blank for the default Wanted List)" ), wantedlist )) {
		QMap <QString, QString> extra;
		extra. insert ( "WANTEDLISTID", wantedlist );

		QDomDocument doc ( QString::null );
		QPtrList <BrickLink::InvItem> si = sortedItems ( );
		doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_WantedList, &si ));

		QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

		if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_WantedListUpload ));
	}
}

void CWindow::fileExportBrickLinkXMLClipboard ( )
{
	if ( w_list-> errorCount ( ) && CMessageBox::warning ( this, tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		return;

	QDomDocument doc ( QString::null );
	QPtrList <BrickLink::InvItem> si = sortedItems ( );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_MassUpload, &si ));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryUpload ));
}

void CWindow::fileExportBrickLinkUpdateClipboard ( )
{
	if ( w_list-> errorCount ( ) && CMessageBox::warning ( this, tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		return;

	for ( QPtrListIterator <BrickLink::InvItem> it ( m_items ); it. current ( ); ++it ) {
		if ( !it. current ( )-> lotId ( )) {
			if ( CMessageBox::warning ( this, tr( "This list contains items without a BrickLink Lot-ID.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
				return;
			else
				break;
		}
	}

	QDomDocument doc ( QString::null );
	QPtrList <BrickLink::InvItem> si = sortedItems ( );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_MassUpdate, &si ));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryUpdate ));
}

void CWindow::fileExportBrickLinkXML ( )
{
	if ( w_list-> errorCount ( ) && CMessageBox::warning ( this, tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		return;

	QStringList filters;
	filters << tr( "BrickLink XML Inventory" ) + " (*.xml)";

	QString s = QFileDialog::getSaveFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), this, "FileDialog", tr( "Export File" ), 0 );

	if ( !s. isNull ( )) {
		if ( s. right ( 4 ) != ".xml" )
			s += ".xml";

		if ( QFile::exists ( s ) &&
		     CMessageBox::question ( this, tr( "A file named %1 already exists. Are you sure you want to overwrite it?" ). arg( CMB_BOLD( s )), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		     return;

		fileSaveTo ( s, 'X', true );
	}
}

void CWindow::editUndo ( )
{
}

void CWindow::editRedo ( )
{
}


void CWindow::editCut ( )
{
	editCopy ( );
	editDelete ( );
}

void CWindow::editCopy ( )
{
	if ( !m_selection. isEmpty ( ))
		QApplication::clipboard ( )-> setData ( new BrickLink::InvItemDrag ( m_selection ));
}

void CWindow::editPaste ( )
{
	QPtrList<BrickLink::InvItem> ii;
	ii. setAutoDelete ( true );

	if ( BrickLink::InvItemDrag::decode ( QApplication::clipboard ( )-> data ( ), ii )) {
		if ( ii. count ( )) {
			if ( !m_selection. isEmpty ( )) {
				if ( CMessageBox::question ( this, tr( "Overwrite the currently selected items?" ), QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape ) == QMessageBox::Yes )
					deleteItems ( m_selection );
			}
			addItems ( ii, 1, MergeAction_Ask | MergeKeep_New );
		}
	}
}

void CWindow::editDelete ( )
{
	if ( !m_selection. isEmpty ( ))
		deleteItems ( m_selection );
}

void CWindow::selectAll ( )
{
	w_list-> selectAll ( true );
}

void CWindow::selectNone ( )
{
	w_list-> selectAll ( false );
}

void CWindow::editResetDifferences ( )
{
	if ( !m_selection. isEmpty ( ))
		resetDifferences ( m_selection );
}

void CWindow::viewPlain ( bool )
{
}

void CWindow::viewHierarchic ( bool )
{
}

void CWindow::viewCompact ( bool )
{
}


#if 0
class UndoBuffer {
public:
	enum Action {
		Add,		// list of invitem
		Remove,     // list of invitem, positions
		Move,       // list if invitem, positions
		Change      // list of invitem
	};

	UndoBuffer ( CWindow *v, const QString &title, Action act, const QPtrList<BrickLink::InvItem> *list = 0 );

private:
	QString m_title;

	Action m_action;

	QValueVector m_invitems;
	QValueVector m_positions;
};

//                                        = QString::null          = 0
UndoBuffer *execute ( Action act, const QString &title, const QPtrList<BrickLink::InvItem> *list )
{
	if ( !list )
		list = &m_selection;

	if ( list-> isEmpty ( ))
		return 0;

	if ( !title ) {
		switch ( act ) {
			case Add   : title = QString ( "Added %1 items" ). arg ( list-> count ( )); break;
			case Remove: title = QString ( "Removed %1 items" ). arg ( list-> count ( )); break;
			case Change:
			case Move
		}
	}
}

#endif

void CWindow::editSetPrice ( )
{
}

void CWindow::editSetPriceToPG ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	if ( m_settopg_list ) {
		CMessageBox::information ( this, tr( "Prices are currently updated to price guide values.<br /><br />Please wait until this operation has finished." ));
		return;
	}

	DlgSetToPGImpl dlg ( this );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		CDisableUpdates disupd ( w_list );

		m_settopg_list    = new QPtrDict<BrickLink::InvItem> ( 251 );
		m_settopg_failcnt = 0;
		m_settopg_time    = dlg. time ( );
		m_settopg_price   = dlg. price ( );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );
			CItemViewItem *ivi = m_lvitems [ii];

			BrickLink::PriceGuide *pg = BrickLink::inst ( )-> priceGuide ( ii );

			if ( pg && ( pg-> updateStatus ( ) == BrickLink::Updating )) {
				m_settopg_list-> insert ( pg, ii );
				pg-> addRef ( );
			}
			else if ( pg && pg-> valid ( )) {
				money_t p = pg-> price ( m_settopg_time, ii-> condition ( ), m_settopg_price );

				if ( p != ii-> price ( )) {
					ii-> setPrice ( p );

					ivi-> checkForErrors ( );
					ivi-> repaint ( );
				}
			}
			else {
				ii-> setPrice ( 0 );
				
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
				m_settopg_failcnt++;
			}
		}

		if ( m_settopg_list-> isEmpty ( ))
			priceGuideUpdated ( 0 );

		updateStatistics ( );
		setModified ( true );
	}
}

void CWindow::priceGuideUpdated ( BrickLink::PriceGuide *pg )
{
	if ( m_settopg_list && pg ) {
		BrickLink::InvItem *ii;

		while (( ii = m_settopg_list-> find ( pg ))) {
			if ( pg-> valid ( )) {
				CItemViewItem *ivi = m_lvitems [ii];

				if ( ivi ) { // still valid
					money_t p = pg-> price ( m_settopg_time, ii-> condition ( ), m_settopg_price );

					if ( p != ii-> price ( )) {
						ii-> setPrice ( p );

						ivi-> checkForErrors ( );
						ivi-> repaint ( );

						updateStatistics ( );
					}
				}
			}
			m_settopg_list-> remove ( pg );
			pg-> release ( );
		}
	}

	if ( m_settopg_list && m_settopg_list-> isEmpty ( )) {
		QString s = tr( "Prices of the selected items have been updated to price guide values." );

		if ( m_settopg_failcnt )
			s += "<br /><br />" + tr( "%1 have been skipped, because of missing price guide records and/or network errors." ). arg ( CMB_BOLD( QString::number ( m_settopg_failcnt )));

		CMessageBox::information ( this, s );

		m_settopg_failcnt = 0;
		delete m_settopg_list;
		m_settopg_list = 0;

		setModified ( true );
	}
}


void CWindow::editPriceIncDec ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	DlgIncDecPriceImpl dlg ( this, "", true );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		money_t fixed   = dlg. fixed ( );
		double percent = dlg. percent ( );
		double factor  = ( 1. + percent / 100. );
		bool tiers     = dlg. applyToTiers ( );

		CDisableUpdates disupd ( w_list );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );

			money_t p = ii-> price ( );

			if ( percent != 0 )     p *= factor;
			else if ( fixed != 0 )  p += fixed;

			ii-> setPrice ( p );

			if ( tiers ) {
				for ( int i = 0; i < 3; i++ ) {
					p = ii-> tierPrice ( i );

					if ( percent != 0 )     p *= factor;
					else if ( fixed != 0 )  p += fixed;

					ii-> setPrice ( p );
				}
			}
			m_lvitems [ii]-> checkForErrors ( );
			m_lvitems [ii]-> repaint ( );
		}
		updateStatistics ( );
		setModified ( true );
	}
}

void CWindow::editDivideQty ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	int divisor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Divides the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisble without reminder, before this operation is performed.)" ), QString::null, divisor, new QIntValidator ( -1000, 1000, 0 ))) {
		if (( divisor <= -1 ) || ( divisor > 1 )) {
			int lots_with_errors = 0;
			int abs_divisor = QABS( divisor );
			
			for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
				BrickLink::InvItem *ii = it. current ( );

				if ( QABS( ii-> quantity ( )) % abs_divisor )
					lots_with_errors++;
			}

			if ( lots_with_errors ) {
				CMessageBox::information ( this, tr( "The quantities of %1 lots are not divisible without remainder by %2.<br /><br />Nothing has been modified." ). arg( lots_with_errors ). arg( divisor ));
			}
			else {
				CDisableUpdates disupd ( w_list );

				for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
					BrickLink::InvItem *ii = it. current ( );
		
					ii-> setQuantity ( ii-> quantity ( ) / divisor );

					CItemViewItem *ivi = m_lvitems [ii];
					ivi-> checkForErrors ( );
					ivi-> repaint ( );
				}
				updateStatistics ( );
				updateSelection ( );
				setModified ( true );
			}
		}
	}
}

void CWindow::editMultiplyQty ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	int factor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Multiplies the quantities of all selected items with this factor." ), tr( "x" ), factor, new QIntValidator ( -1000, 1000, 0 ))) {
		if (( factor <= 1 ) || ( factor > 1 )) {
			CDisableUpdates disupd ( w_list );

			for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
				BrickLink::InvItem *ii = it. current ( );
	
				ii-> setQuantity ( ii-> quantity ( ) * factor );

				CItemViewItem *ivi = m_lvitems [ii];
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
			}
			updateStatistics ( );
			updateSelection ( );
			setModified ( true );
		}
	}
}

void CWindow::editSetSale ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	int sale = 0;

	if ( CMessageBox::getInteger ( this, tr( "Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed." ), tr( "%" ), sale, new QIntValidator ( -1000, 99, 0 ))) {
		CDisableUpdates disupd ( w_list );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );

			if ( ii-> sale ( ) != sale ) {
				ii-> setSale ( sale );

				CItemViewItem *ivi = m_lvitems [ii];
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
			}
		}
		updateStatistics ( );
		updateSelection ( );
		setModified ( true );
	}
}

void CWindow::editSetCondition ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	DlgSetConditionImpl d ( this );

	if ( d. exec ( ) == QDialog::Accepted ) {
		CDisableUpdates disupd ( w_list );

		BrickLink::Condition nc = d. newCondition ( );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );

			BrickLink::Condition oc = ii-> condition ( );

			if ( oc != nc ) {
				ii-> setCondition ( nc != BrickLink::ConditionCount ? nc : ( oc == BrickLink::New ? BrickLink::Used : BrickLink::New ));

				CItemViewItem *ivi = m_lvitems [ii];
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
			}
		}
		updateStatistics ( );
		updateSelection ( );
		setModified ( true );
	}
}

void CWindow::editSetRemark ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	QString remark;
	
	if ( CMessageBox::getString ( this, tr( "Enter the new remark for all selected items:" ), remark )) {
		CDisableUpdates disupd ( w_list );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );

			if ( ii-> remarks ( ) != remark ) {
				ii-> setRemarks ( remark );

				CItemViewItem *ivi = m_lvitems [ii];
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
			}
		}
		updateStatistics ( );
		setModified ( true );
	}
}

void CWindow::editSetReserved ( )
{
	if ( m_selection. isEmpty ( ))
		return;

	QString reserved;
	
	if ( CMessageBox::getString ( this, tr( "Reserve this item for a specific member:" ), reserved )) {
		CDisableUpdates disupd ( w_list );

		for ( QPtrListIterator<BrickLink::InvItem> it ( m_selection ); it. current ( ); ++it ) {
			BrickLink::InvItem *ii = it. current ( );

			if ( ii-> reserved ( ) != reserved ) {
				ii-> setReserved ( reserved );

				CItemViewItem *ivi = m_lvitems [ii];
				ivi-> checkForErrors ( );
				ivi-> repaint ( );
			}
		}
		updateStatistics ( );
		setModified ( true );
	}
}

bool CWindow::isModified ( ) const
{
	return m_modified;
}

void CWindow::setModified ( bool b )
{
	if ( m_modified != b ) {
		m_modified = b;
		emit modificationChanged ( b );
	}
}

QString CWindow::fileName ( ) const
{
	return m_filename;
}

void CWindow::setFileName ( const QString &str )
{
	m_filename = str;

	QFileInfo fi ( str );

	if ( fi. exists ( )) {
		m_caption = QDir::convertSeparators ( fi. absFilePath ( ));

		//TODO: make this a bit more intelligent
		if ( m_caption. length ( ) > 50 )
			m_caption = str. left ( 16 ) + "..." + str. right ( 32 );
	}
	updateCaption ( );
}

void CWindow::triggerUpdate ( )
{
	w_list-> triggerUpdate ( );

	triggerSelectionUpdate ( );
	triggerStatisticsUpdate ( );
}

void CWindow::triggerSelectionUpdate ( )
{
	updateSelection ( );
}

void CWindow::triggerStatisticsUpdate ( )
{
	updateStatistics ( );
}

void CWindow::updateStatistics ( )
{
	emit statisticsChanged ( m_items );
}

void CWindow::calculateStatistics ( const QPtrList <BrickLink::InvItem> &list, int &lots, int &items, money_t &val, money_t &minval, double &weight, int &errors )
{
	lots = list. count ( );
	items = 0;
	val = minval = 0.;
	weight = .0;
	errors = 0;
	bool weight_missing = false;

	for ( QPtrListIterator<BrickLink::InvItem> it ( list ); it. current ( ); ++it ) {
		BrickLink::InvItem *ii = it. current ( );

		int qty = ii-> quantity ( );
		money_t price = ii-> price ( );

		val += ( qty * price );

		for ( int i = 0; i < 3; i++ ) {
			if ( ii-> tierQuantity ( i ) && ( ii-> tierPrice ( i ) != 0 ))
				price = ii-> tierPrice ( i );
		}
		minval += ( qty * price * ( 1.0 - double( ii-> sale ( )) / 100.0 ));
		items += qty;

		if ( ii-> weight ( ) > 0 )
			weight += ii-> weight ( );
		else 
			weight_missing = true;
	
		errors += static_cast <CItemViewItem *> ( m_lvitems [ii] )-> errorCount ( );
	}
	if ( weight_missing )
		weight = ( weight == 0. ) ? -DBL_MIN : -weight;
}

void CWindow::editAddItems ( )
{
	if ( m_add_dialog ) {
		m_add_dialog-> setActiveWindow ( );
	}
	else {
		m_add_dialog = new DlgAddItemImpl ( this, "AddItemDlg", false, Qt::WDestructiveClose );

		connect ( m_add_dialog, SIGNAL( addItem ( const BrickLink::InvItem *, uint )), this, SLOT( addItem ( const BrickLink::InvItem *, uint )));

		m_add_dialog-> show ( );
	}
}

void CWindow::editSubtractItems ( )
{
	DlgSubtractItemImpl d ( this, "SubtractItemDlg" );

	if ( d. exec ( ) == QDialog::Accepted ) {
		QPtrList <BrickLink::InvItem> *list = d. items ( );

		if ( list && !list-> isEmpty ( ))
			subtractItems ( *list );

		delete list;
	}
}

void CWindow::subtractItems ( const QPtrList <BrickLink::InvItem> &items )
{
	if ( items. isEmpty ( ))
		return;

	QPtrList <QListViewItem> update_list;

	CDisableUpdates disupd ( w_list );

	for ( QPtrListIterator <BrickLink::InvItem> it ( items ); it. current ( ); ++it ) {
		const BrickLink::Item *item   = it. current ( )-> item ( );
		const BrickLink::Color *color = it. current ( )-> color ( );
		BrickLink::Condition cond     = it. current ( )-> condition ( );
		int qty                       = it. current ( )-> quantity ( );

		if ( !item || !color )
			continue;

		BrickLink::InvItem *last_match = 0;

		for ( QPtrListIterator <BrickLink::InvItem> it2 ( m_items ); it2. current ( ); ++it2 ) {
			BrickLink::InvItem *ii = it2. current ( );

			if (( ii-> item ( ) == item ) && ( ii-> color ( ) == color ) && ( ii-> condition ( ) == cond )) {
				CItemViewItem *ivi = m_lvitems [ii];

				if ( ii-> quantity ( ) >= qty ) {
					ii-> setQuantity ( ii-> quantity ( ) - qty );
					qty = 0;
				}
				else {
					qty -= ii-> quantity ( );
					ii-> setQuantity ( 0 );
				}
				last_match = ii;
				ivi-> repaint ( );
			}
		}

		if ( qty ) { // still a qty left
			if ( last_match ) {
				CItemViewItem *ivi = m_lvitems [last_match];

				last_match-> setQuantity ( last_match-> quantity ( ) - qty );
				ivi-> repaint ( );
			}
			else {
				BrickLink::InvItem ii ( color, item );
				ii. setCondition ( cond );
				ii. setOrigQuantity ( 0 );
				ii. setQuantity ( -qty );

				addItem ( &ii, MergeAction_None );
			}
		}
	}
	updateStatistics ( );
	updateSelection ( );
	setModified ( true );
}

void CWindow::editMergeItems ( )
{
	if ( !m_selection. isEmpty ( ))
		mergeItems ( m_selection );
	else
		mergeItems ( m_items );
}

void CWindow::editPartOutItems ( )
{
	if ( m_selection. count ( ) == 1 )
		CFrameWork::inst ( )-> fileImportBrickLinkInventory ( m_selection. first ( )-> item ( ));
	else
		QApplication::beep ( );
}

void CWindow::setPrice ( money_t d )
{
	if ( m_selection. count ( ) == 1 ) {
		m_selection. first ( )-> setPrice ( d );

		CItemViewItem *ivi = m_lvitems [m_selection. first ( )];
		
		ivi-> checkForErrors ( );
		ivi-> repaint ( );

		updateStatistics ( );
		setModified ( true );
	}
	else
		QApplication::beep ( );
}

void CWindow::contextMenu ( QListViewItem *it, const QPoint &p )
{
	CFrameWork::inst ( )-> showContextMenu (( it ), p );
}

void CWindow::itemModified ( CItemViewItem * /*item*/, bool grave )
{
	setModified ( true );
	updateStatistics ( );

	if ( grave )
		updateSelection ( );
}

void CWindow::closeEvent ( QCloseEvent *e )
{
	if ( isModified ( )) {
		switch ( CMessageBox::warning ( this, tr( "Save changes to %1?" ). arg ( CMB_BOLD( caption ( ). left ( caption ( ). length ( ) - 2 ))), CMessageBox::Yes | CMessageBox::Default, CMessageBox::No, CMessageBox::Cancel | CMessageBox::Escape )) {
		case CMessageBox::Yes:
			fileSave ( );

			if ( !isModified ( ))
				e-> accept ( );
			else
				e-> ignore ( );
			break;

		case CMessageBox::No:
			e-> accept ( );
			break;

		default:
			e-> ignore ( );
			break;
		}
	}
	else {
		e-> accept ( );
	}

}

void CWindow::showBLCatalog ( )
{
	if ( !m_selection. isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo, m_selection. first ( )-> item ( )));
}

void CWindow::showBLPriceGuide ( )
{
	if ( !m_selection. isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, m_selection. first ( )-> item ( ), m_selection. first ( )-> color ( )));
}

void CWindow::showBLLotsForSale ( )
{
	if ( !m_selection. isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale, m_selection. first ( )-> item ( ), m_selection. first ( )-> color ( )));
}

void CWindow::setDifferenceMode ( bool b )
{
	w_list-> setDifferenceMode ( b );
}

bool CWindow::isDifferenceMode ( ) const
{
	return w_list-> isDifferenceMode ( );
}

