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
#include "cundo.h"
#include "cdocument.h"

#include "dlgincdecpriceimpl.h"
#include "dlgsettopgimpl.h"
#include "dlgloadinventoryimpl.h"
#include "dlgloadorderimpl.h"
#include "dlgselectreportimpl.h"
#include "dlgincompleteitemimpl.h"
#include "dlgmergeimpl.h"
#include "dlgsubtractitemimpl.h"
#include "dlgsetconditionimpl.h"

#include "cwindow.h"



CWindow::CWindow ( CDocument *doc, QWidget *parent, const char *name )
	: QWidget ( parent, name, WDestructiveClose ), m_lvitems ( 503 )
{
	m_doc = doc;
	m_ignore_selection_update = false;

	m_filter_field = CItemView::All;

	m_settopg_failcnt = 0;
	m_settopg_list = 0;
	m_settopg_time = BrickLink::PriceGuide::AllTime;
	m_settopg_price = BrickLink::PriceGuide::Average;

	// m_items. setAutoDelete ( true ); NOT ANYMORE -> UndoRedo

	w_filter_clear = new QToolButton ( this );
	w_filter_clear-> setAutoRaise ( true );
	w_filter_clear-> setIconSet ( CResource::inst ( )-> iconSet ( "filter_clear" ));
	w_filter_label = new QLabel ( this );

	w_filter_expression = new QComboBox ( true, this );
	w_filter_field = new QComboBox ( false, this );
	w_filter_field_label = new QLabel ( this );

	w_list = new CItemView ( doc, this );
	setFocusProxy ( w_list );
	w_list-> setShowSortIndicator ( true );
	w_list-> setColumnsHideable ( true );
	w_list-> setDifferenceMode ( false );
	w_list-> setSimpleMode ( CConfig::inst ( )-> simpleMode ( ));

	{
		QMap <QString, QString> map;
		QStringList sl = CConfig::inst ( )-> entryList ( "/ItemView/List" );

		for ( QStringList::const_iterator it = sl. begin ( ); it != sl. end ( ); ++it ){
			QString val = CConfig::inst ( )-> readEntry ( "/ItemView/List/" + *it );

			if ( val. contains ( "^e" ))
				map [*it] = CConfig::inst ( )-> readListEntry ( "/ItemView/List/" + *it ). join ( "," );
			else
				map [*it] = val;
		}

		w_list-> loadSettings ( map );
	}

	connect ( w_list, SIGNAL( selectionChanged ( )), this, SLOT( updateSelectionFromView ( )));

	for ( int i = 0; i < ( CItemView::FilterCountSpecial + CDocument::FieldCount ); i++ )
		w_filter_field-> insertItem ( QString ( ));

	QBoxLayout *toplay = new QVBoxLayout ( this, 0, 0 );
	QBoxLayout *barlay = new QHBoxLayout ( toplay, 4 );
	barlay-> setMargin ( 4 );
	barlay-> addWidget ( w_filter_clear, 0 );
	barlay-> addWidget ( w_filter_label, 0 );
	barlay-> addWidget ( w_filter_expression, 15 );
	barlay-> addWidget ( w_filter_field_label, 0 );
	barlay-> addWidget ( w_filter_field, 5 );
	toplay-> addWidget ( w_list );

	connect ( BrickLink::inst ( ), SIGNAL( priceGuideUpdated ( BrickLink::PriceGuide * )), this, SLOT( priceGuideUpdated ( BrickLink::PriceGuide * )));
	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));

	connect ( w_list, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( contextMenu ( QListViewItem *, const QPoint & )));
	connect ( w_filter_clear, SIGNAL( clicked ( )), w_filter_expression, SLOT( clearEdit ( )));
	connect ( w_filter_expression, SIGNAL( textChanged ( const QString & )), this, SLOT( applyFilter ( )));
	connect ( w_filter_field, SIGNAL( activated ( int )), this, SLOT( applyFilter ( )));

	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), w_list, SLOT( setSimpleMode ( bool )));
	connect ( CConfig::inst ( ), SIGNAL( simpleModeChanged ( bool )), this, SLOT( updateErrorMask ( )));
	connect ( CConfig::inst ( ), SIGNAL( showInputErrorsChanged ( bool )), this, SLOT( updateErrorMask ( )));

	connect ( CConfig::inst ( ), SIGNAL( weightSystemChanged ( CConfig::WeightSystem )), w_list, SLOT( triggerUpdate ( )));
	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), w_list, SLOT( triggerUpdate ( )));

////////////////////////////////////////////////////////////////////////////////

	connect ( m_doc, SIGNAL( titleChanged ( const QString & )), this, SLOT( updateCaption ( )));
	connect ( m_doc, SIGNAL( modificationChanged ( bool )), this, SLOT( updateCaption ( )));

	connect ( m_doc, SIGNAL( itemsAdded( const CDocument::ItemList & )), this, SLOT( itemsAddedToDocument ( const CDocument::ItemList & )));
	connect ( m_doc, SIGNAL( itemsRemoved( const CDocument::ItemList & )), this, SLOT( itemsRemovedFromDocument ( const CDocument::ItemList & )));
	connect ( m_doc, SIGNAL( itemsChanged( const CDocument::ItemList &, bool )), this, SLOT( itemsChangedInDocument ( const CDocument::ItemList &, bool )));

	m_doc-> addView ( this, this );
	itemsAddedToDocument ( m_doc-> items ( ));

	updateErrorMask ( );
	languageChange ( );

	w_list-> setFocus ( );
}

void CWindow::languageChange ( )
{
	w_filter_label-> setText ( tr( "Filter:" ));
	w_filter_field_label-> setText ( tr( "Field:" ));

	QToolTip::add ( w_filter_expression, tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));
	QToolTip::add ( w_filter_clear, tr( "Reset an active filter" ));
	QToolTip::add ( w_filter_field, tr( "Restrict the filter to this/these field(s)" ));

	int i, j;
	for ( i = 0; i < CItemView::FilterCountSpecial; i++ ) {
		QString s;
		switch ( -i - 1 ) {
			case CItemView::All       : s = tr( "All" ); break;
			case CItemView::Prices    : s = tr( "All Prices" ); break;
			case CItemView::Texts     : s = tr( "All Texts" ); break;
			case CItemView::Quantities: s = tr( "All Quantities" ); break;
		}
		w_filter_field-> changeItem ( s, i );
	}
	for ( j = 0; j < CDocument::FieldCount; j++ )
		w_filter_field-> changeItem ( w_list-> header ( )-> label ( j ), i + j );

	updateCaption ( );
}

void CWindow::updateCaption ( )
{
	QString cap = m_doc-> title ( );

	if ( cap. isEmpty ( ))
		cap = m_doc-> fileName ( );
	if ( cap. isEmpty ( ))
		cap = tr( "Untitled" );

	//TODO: make this a bit more intelligent
	if ( cap. length ( ) > 50 )
		cap = cap. left ( 16 ) + "..." + cap. right ( 32 );

	if ( m_doc-> isModified ( ))
		cap += " *";

	setCaption ( cap );
}

CWindow::~CWindow ( )
{
	m_doc-> deleteLater ( );
//	w_list-> saveSettings ( CConfig::inst ( ), "/ItemView/List" );
}

void CWindow::itemsAddedToDocument ( const CDocument::ItemList &items )
{
	QListViewItem *last_item = w_list-> lastItem ( );

	foreach ( CDocument::Item *item, items ) {
		CItemViewItem *ivi = new CItemViewItem ( item, w_list, last_item );
		m_lvitems. insert ( item, ivi );
		last_item = ivi;
	}

	w_list-> ensureItemVisible ( m_lvitems [items. front ( )] );
}

void CWindow::itemsRemovedFromDocument ( const CDocument::ItemList &items )
{
	foreach ( CDocument::Item *item, items )
		delete m_lvitems. take ( item );
}

void CWindow::itemsChangedInDocument ( const CDocument::ItemList &items, bool /*grave*/ )
{
	foreach ( CDocument::Item *item, items ) {
		CItemViewItem *ivi = m_lvitems [item];
		
		if ( ivi ) {
			ivi-> widthChanged ( );
			ivi-> repaint ( );
		}
	}
}


void CWindow::applyFilter ( )
{
	w_list-> applyFilter ( w_filter_expression-> lineEdit ( )-> text ( ), w_filter_field-> currentItem ( ), true );
}


void CWindow::pictureUpdated ( BrickLink::Picture *pic )
{
	if ( !pic )
		return;

	foreach ( CDocument::Item *item, m_doc-> items ( )) {
		if (( pic-> item ( ) == item-> item ( )) && ( pic-> color ( ) == item-> color ( )))
			m_lvitems [item]-> repaint ( );
	}
}

uint CWindow::addItems ( const BrickLink::InvItemList &items, int multiply, uint globalmergeflags, bool dont_change_sorting )
{
	bool waitcursor = ( items. count ( ) > 100 );
	bool was_empty = ( w_list-> childCount ( ) == 0 );
	uint dropped = 0;
	int merge_action_yes_no_to_all = MergeAction_Ask;
	int mergecount = 0, addcount = 0;

	CUndoCmd *macro = 0;
	if ( items. count ( ) > 1 )
		macro = m_doc-> macroBegin ( );

	// disable sorting: new items should appear at the end of the list
	if ( !dont_change_sorting )
		w_list-> setSorting ( w_list-> columns ( ) + 1 ); // +1 because of Qt-bug !!

	if ( waitcursor )
		QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CDisableUpdates disupd ( w_list );


	foreach ( const BrickLink::InvItem *origitem, items ) {
		uint mergeflags = globalmergeflags;

		CDocument::Item *newitem = new CDocument::Item ( *origitem );

		if ( newitem-> isIncomplete ( )) {
			DlgIncompleteItemImpl d ( newitem, this );

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

		CDocument::Item *olditem = 0;

		if ( mergeflags != MergeAction_None ) {
			foreach ( CDocument::Item *item, m_doc-> items ( )) {
				if (( newitem-> item ( ) == item-> item ( )) &&
				    ( newitem-> color ( ) == item-> color ( ))&& 
				    ( newitem-> condition ( ) == item-> condition ( ))) {
					olditem = item;
					break;
				}
			}
			if ( !olditem )
				mergeflags = MergeAction_None;
		}

		if (( mergeflags & MergeAction_Mask ) == MergeAction_Ask ) {
			if ( merge_action_yes_no_to_all != MergeAction_Ask ) {
				mergeflags = merge_action_yes_no_to_all;
			}
			else {
				DlgMergeImpl d ( olditem, newitem, (( mergeflags & MergeKeep_Mask ) == MergeKeep_Old ), this );

				int res = d. exec ( );

				mergeflags = ( res ? MergeAction_Force : MergeAction_None );

				if ( mergeflags != MergeAction_None )
					mergeflags |= ( d. attributesFromExisting ( ) ? MergeKeep_Old : MergeKeep_New );

				if ( d. yesNoToAll ( ))
					merge_action_yes_no_to_all = mergeflags;
			}
		}

		switch ( mergeflags & MergeAction_Mask ) {
			case MergeAction_Force: {
				newitem-> mergeFrom ( *olditem, (( mergeflags & MergeKeep_Mask ) == MergeKeep_New ));

				m_doc-> changeItem ( olditem, *newitem );
				delete newitem;
				mergecount++;
				break;
			}
			case MergeAction_None:
			default: {
				if ( multiply > 1 )
					newitem-> setQuantity ( newitem-> quantity ( ) * multiply );

				m_doc-> insertItem ( 0, newitem );
				addcount++;
				break;
			}
		}
	}
	disupd. reenable ( );

	if ( waitcursor )
		QApplication::restoreOverrideCursor ( );

	if ( macro )
		m_doc-> macroEnd ( macro, tr( "Added %1, Merged %2 Items" ). arg( addcount ). arg( mergecount ));

	if ( was_empty )
		w_list-> setCurrentItem ( w_list-> firstChild ( ));

	return items. count ( ) - dropped;
}


void CWindow::addItem ( BrickLink::InvItem *item, uint mergeflags )
{
	BrickLink::InvItemList tmplist;
	tmplist. append ( item );

	addItems ( tmplist, 1, mergeflags );

	delete item;
}


void CWindow::mergeItems ( const CDocument::ItemList &items, int globalmergeflags )
{
	if (( items. count ( ) < 2 ) || ( globalmergeflags & MergeAction_Mask ) == MergeAction_None )
		return;

	int merge_action_yes_no_to_all = MergeAction_Ask;
	uint mergecount = 0;

	CUndoCmd *macro = m_doc-> macroBegin ( );

	CDisableUpdates disupd ( w_list );

	foreach ( CDocument::Item *from, items ) {
		CDocument::Item *to = 0;

		foreach ( CDocument::Item *find_to, items ) {
			if (( from != find_to ) &&
			    ( from-> item ( ) == find_to-> item ( )) &&
			    ( from-> color ( ) == find_to-> color ( ))&& 
			    ( from-> condition ( ) == find_to-> condition ( )) &&
			    ( m_doc-> items ( ). find ( find_to ) != m_doc-> items ( ). end ( ))) {
				to = find_to;
				break;
			}
		}
		if ( !to )
			continue;

		uint mergeflags = globalmergeflags;

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

			CDocument::Item newitem = *to;
			newitem. mergeFrom ( *from, prefer_from );
			m_doc-> changeItem ( to, newitem );
			m_doc-> removeItem ( from );

			mergecount++;
		}
	} 
	m_doc-> macroEnd ( macro, tr( "Merged %1 Items" ). arg ( mergecount ));
}

void CWindow::updateSelectionFromView ( )
{
	CDocument::ItemList itlist;

	for ( QListViewItemIterator it ( w_list ); it. current ( ); ++it ) {
		CItemViewItem *ivi = static_cast <CItemViewItem *> ( it. current ( ));

		if ( ivi-> isSelected ( ) && ivi-> isVisible ( ))
			itlist. append ( ivi-> item ( ));
	}

	m_ignore_selection_update = true;
	m_doc-> setSelection ( itlist );
	m_ignore_selection_update = false;
}

void CWindow::updateSelectionFromDoc ( const CDocument::ItemList &itlist )
{
	if ( m_ignore_selection_update )
		return;

	w_list-> clearSelection ( );

	foreach ( CDocument::Item *item, itlist ) {
		m_lvitems [item]-> setSelected ( true );
	}
}

QDomElement CWindow::createGuiStateXML ( QDomDocument doc )
{
	int version = 1;

	QDomElement root = doc. createElement ( "GuiState" );
	root. setAttribute ( "Application", "BrickStore" );
	root. setAttribute ( "Version", version );

	if ( isDifferenceMode ( ))
		root. appendChild ( doc. createElement ( "DifferenceMode" ));

	QMap <QString, QString> list_map = w_list->saveSettings ( );

	if ( !list_map. isEmpty ( )) {
		QDomElement e = doc. createElement ( "ItemView" );
		root. appendChild ( e );

		for ( QMap <QString, QString>::const_iterator it = list_map. begin ( ); it != list_map. end ( ); ++it ) 
			e. appendChild ( doc. createElement ( it. key ( )). appendChild ( doc. createTextNode ( it. data ( ))). parentNode ( ));
	}

	return root;
}

bool CWindow::parseGuiStateXML ( QDomElement root )
{
	bool ok = true;

	if (( root. nodeName ( ) == "GuiState" ) && 
	    ( root. attribute ( "Application" ) == "BrickStore" ) && 
		( root. attribute ( "Version" ). toInt ( ) == 1 )) {
		for ( QDomNode n = root. firstChild ( ); !n. isNull ( ); n = n. nextSibling ( )) {
			if ( !n. isElement ( ))
				continue;
			QString tag = n. toElement ( ). tagName ( );

			if ( tag == "DifferenceMode" ) {
				setDifferenceMode ( true );
			}
			else if ( tag == "ItemView" ) {
				QMap <QString, QString> list_map;

				for ( QDomNode niv = n. firstChild ( ); !niv. isNull ( ); niv = niv. nextSibling ( )) {
					if ( !niv. isElement ( ))
						continue;
					list_map [niv. toElement ( ). tagName ( )] = niv. toElement ( ). text ( );
				}

				if ( !list_map. isEmpty ( ))
					w_list-> loadSettings ( list_map );				
			}
		}

	}

	return ok;
}


void CWindow::editCut ( )
{
	editCopy ( );
	editDelete ( );
}

void CWindow::editCopy ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( )) {
		BrickLink::InvItemList bllist;

		foreach ( CDocument::Item *item, m_doc-> selection ( ))
			bllist. append ( item );

		QApplication::clipboard ( )-> setData ( new BrickLink::InvItemDrag ( bllist ));
	}
}

void CWindow::editPaste ( )
{
	BrickLink::InvItemList bllist;

	if ( BrickLink::InvItemDrag::decode ( QApplication::clipboard ( )-> data ( ), bllist )) {
		if ( bllist. count ( )) {
			if ( !m_doc-> selection ( ). isEmpty ( )) {
				if ( CMessageBox::question ( this, tr( "Overwrite the currently selected items?" ), QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape ) == QMessageBox::Yes )
					m_doc-> removeItems ( m_doc-> selection ( ));
			}
			addItems ( bllist, 1, MergeAction_Ask | MergeKeep_New );
			qDeleteAll ( bllist );
		}
	}
}

void CWindow::editDelete ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( ))
		m_doc-> removeItems ( m_doc-> selection ( ));
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
	CDisableUpdates disupd ( w_list );

	m_doc-> resetDifferences ( m_doc-> selection ( ));
}

void CWindow::editSetPrice ( )
{
}

void CWindow::editSetPriceToPG ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	if ( m_settopg_list ) {
		CMessageBox::information ( this, tr( "Prices are currently updated to price guide values.<br /><br />Please wait until this operation has finished." ));
		return;
	}

	DlgSetToPGImpl dlg ( this );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		CDisableUpdates disupd ( w_list );

		m_settopg_list    = new QPtrDict<CDocument::Item> ( 251 );
		m_settopg_failcnt = 0;
		m_settopg_time    = dlg. time ( );
		m_settopg_price   = dlg. price ( );

		foreach ( CDocument::Item *item, m_doc-> selection ( )) {
			BrickLink::PriceGuide *pg = BrickLink::inst ( )-> priceGuide ( item-> item ( ), item-> color ( ));

			if ( pg && ( pg-> updateStatus ( ) == BrickLink::Updating )) {
				m_settopg_list-> insert ( pg, item );
				pg-> addRef ( );
			}
			else if ( pg && pg-> valid ( )) {
				money_t p = pg-> price ( m_settopg_time, item-> condition ( ), m_settopg_price );

				if ( p != item-> price ( )) {
					CDocument::Item newitem = *item;
					newitem. setPrice ( p );
					m_doc-> changeItem ( item, newitem );
				}
			}
			else {
				CDocument::Item newitem = *item;
				newitem. setPrice ( 0 );
				m_doc-> changeItem ( item, newitem );
				
				m_settopg_failcnt++;
			}
		}

		if ( m_settopg_list-> isEmpty ( ))
			priceGuideUpdated ( 0 );
	}
}

void CWindow::priceGuideUpdated ( BrickLink::PriceGuide *pg )
{
	if ( m_settopg_list && pg ) {
		CDocument::Item *item;

		while (( item = m_settopg_list-> find ( pg ))) {
			if ( pg-> valid ( )) {
				CItemViewItem *ivi = m_lvitems [item];

				if ( ivi ) { // still valid
					money_t p = pg-> price ( m_settopg_time, item-> condition ( ), m_settopg_price );

					if ( p != item-> price ( )) {
						CDocument::Item newitem = *item;
						newitem. setPrice ( p );
						m_doc-> changeItem ( item, newitem );
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
	}
}


void CWindow::editPriceIncDec ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	DlgIncDecPriceImpl dlg ( this, "", true );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		money_t fixed   = dlg. fixed ( );
		double percent = dlg. percent ( );
		double factor  = ( 1. + percent / 100. );
		bool tiers     = dlg. applyToTiers ( );

		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			CDocument::Item item = *pos;

			money_t p = item. price ( );

			if ( percent != 0 )     p *= factor;
			else if ( fixed != 0 )  p += fixed;

			item. setPrice ( p );

			if ( tiers ) {
				for ( uint i = 0; i < 3; i++ ) {
					p = item. tierPrice ( i );

					if ( percent != 0 )     p *= factor;
					else if ( fixed != 0 )  p += fixed;

					item. setTierPrice ( i, p );
				}
			}
			m_doc-> changeItem ( pos, item );
		}
	}
}

void CWindow::editDivideQty ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int divisor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisble without reminder, before this operation is performed.)" ), QString::null, divisor, new QIntValidator ( 1, 1000, 0 ))) {
		if ( divisor > 1 ) {
			int lots_with_errors = 0;
			
			foreach ( CDocument::Item *item, m_doc-> selection ( )) {
				if ( QABS( item-> quantity ( )) % divisor )
					lots_with_errors++;
			}

			if ( lots_with_errors ) {
				CMessageBox::information ( this, tr( "The quantities of %1 lots are not divisible without remainder by %2.<br /><br />Nothing has been modified." ). arg( lots_with_errors ). arg( divisor ));
			}
			else {
				CDisableUpdates disupd ( w_list );

				foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
					CDocument::Item item = *pos;

					item. setQuantity ( item. quantity ( ) / divisor );
					m_doc-> changeItem ( pos, item );
				}
			}
		}
	}
}

void CWindow::editMultiplyQty ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int factor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Multiply the quantities of all selected items with this factor." ), tr( "x" ), factor, new QIntValidator ( -1000, 1000, 0 ))) {
		if (( factor <= 1 ) || ( factor > 1 )) {
			CDisableUpdates disupd ( w_list );

			foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
				CDocument::Item item = *pos;

				item. setQuantity ( item. quantity ( ) * factor );
				m_doc-> changeItem ( pos, item );
			}
		}
	}
}

void CWindow::editSetSale ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int sale = 0;

	if ( CMessageBox::getInteger ( this, tr( "Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed." ), tr( "%" ), sale, new QIntValidator ( -1000, 99, 0 ))) {
		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			if ( pos-> sale ( ) != sale ) {
				CDocument::Item item = *pos;

				item. setSale ( sale );
				m_doc-> changeItem ( pos, item );
			}
		}
	}
}

void CWindow::editSetCondition ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	DlgSetConditionImpl d ( this );

	if ( d. exec ( ) == QDialog::Accepted ) {
		CDisableUpdates disupd ( w_list );

		BrickLink::Condition nc = d. newCondition ( );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			BrickLink::Condition oc = pos-> condition ( );

			if ( oc != nc ) {
				CDocument::Item item = *pos;

				item. setCondition ( nc != BrickLink::ConditionCount ? nc : ( oc == BrickLink::New ? BrickLink::Used : BrickLink::New ));
				m_doc-> changeItem ( pos, item );
			}
		}
	}
}

void CWindow::editSetRemark ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString remarks;
	
	if ( CMessageBox::getString ( this, tr( "Enter the new remark for all selected items:" ), remarks )) {
		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			if ( pos-> remarks ( ) != remarks ) {
				CDocument::Item item = *pos;

				item. setRemarks ( remarks );
				m_doc-> changeItem ( pos, item );
			}
		}
	}
}

void CWindow::editSetReserved ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString reserved;
	
	if ( CMessageBox::getString ( this, tr( "Reserve all selected items for this specific member:" ), reserved )) {
		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			if ( pos-> reserved ( ) != reserved ) {
				CDocument::Item item = *pos;

				item. setReserved ( reserved );
				m_doc-> changeItem ( pos, item );
			}
		}
	}
}

void CWindow::updateErrorMask ( )
{
	Q_UINT64 em = 0;

	if ( CConfig::inst ( )-> showInputErrors ( )) {
		if ( CConfig::inst ( )-> simpleMode ( )) {
			em = 1ULL << CDocument::Status     | \
				1ULL << CDocument::Picture     | \
				1ULL << CDocument::PartNo      | \
				1ULL << CDocument::Description | \
				1ULL << CDocument::Condition   | \
				1ULL << CDocument::Color       | \
				1ULL << CDocument::Quantity    | \
				1ULL << CDocument::Remarks     | \
				1ULL << CDocument::Category    | \
				1ULL << CDocument::ItemType    | \
				1ULL << CDocument::Weight      | \
				1ULL << CDocument::YearReleased;
		}
		else {
			em = ( 1ULL << CDocument::FieldCount ) - 1;
		}
	}

	m_doc-> setErrorMask ( em );
}

void CWindow::editSubtractItems ( )
{
	DlgSubtractItemImpl d ( this, "SubtractItemDlg" );

	if ( d. exec ( ) == QDialog::Accepted ) {
		BrickLink::InvItemList list = d. items ( );

		if ( !list. isEmpty ( ))
			subtractItems ( list );

		qDeleteAll ( list );
	}
}

void CWindow::subtractItems ( const BrickLink::InvItemList &items )
{
	if ( items. isEmpty ( ))
		return;

	CUndoCmd *macro = m_doc-> macroBegin ( );

	CDisableUpdates disupd ( w_list );

	foreach ( BrickLink::InvItem *ii, items ) {
		const BrickLink::Item *item   = ii-> item ( );
		const BrickLink::Color *color = ii-> color ( );
		BrickLink::Condition cond     = ii-> condition ( );
		int qty                       = ii-> quantity ( );

		if ( !item || !color || !qty )
			continue;

		CDocument::Item *last_match = 0;

		foreach ( CDocument::Item *pos, m_doc-> items ( )) {
			if (( pos-> item ( ) == item ) && ( pos-> color ( ) == color ) && ( pos-> condition ( ) == cond )) {
				CDocument::Item newitem = *pos;

				if ( pos-> quantity ( ) >= qty ) {
					newitem. setQuantity ( pos-> quantity ( ) - qty );
					qty = 0;
				}
				else {
					qty -= pos-> quantity ( );
					newitem. setQuantity ( 0 );
				}
				m_doc-> changeItem ( pos, newitem );
				last_match = pos;
			}
		}

		if ( qty ) { // still a qty left
			if ( last_match ) {
				CDocument::Item newitem = *last_match;
				newitem. setQuantity ( last_match-> quantity ( ) - qty );
				m_doc-> changeItem ( last_match, newitem );
			}
			else {
				CDocument::Item *newitem = new CDocument::Item ( );
				newitem-> setItem ( item );
				newitem-> setColor ( color );
				newitem-> setCondition ( cond );
				newitem-> setOrigQuantity ( 0 );
				newitem-> setQuantity ( -qty );

				m_doc-> insertItem ( 0, newitem );
			}
		}
	}
	m_doc-> macroEnd ( macro, tr( "Subtracted %1 Items" ). arg( items. count ( )));
}

void CWindow::editMergeItems ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( ))
		mergeItems ( m_doc-> selection ( ));
	else
		mergeItems ( m_doc-> items ( ));
}

void CWindow::editPartOutItems ( )
{
	if ( m_doc-> selection ( ). count ( ) >= 1 ) {
		foreach ( CDocument::Item *item, m_doc-> items ( ))
			CFrameWork::inst ( )-> fileImportBrickLinkInventory ( item-> item ( ));
	}
	else
		QApplication::beep ( );
}

void CWindow::setPrice ( money_t d )
{
	if ( m_doc-> selection ( ). count ( ) == 1 ) {
		CDocument::Item *pos = m_doc-> selection ( ). front ( );
		CDocument::Item item = *pos;

		item. setPrice ( d );
		m_doc-> changeItem ( pos, item );
	}
	else
		QApplication::beep ( );
}

void CWindow::contextMenu ( QListViewItem *it, const QPoint &p )
{
	CFrameWork::inst ( )-> showContextMenu (( it ), p );
}

void CWindow::closeEvent ( QCloseEvent *e )
{
	bool close_empty = ( m_doc-> items ( ). isEmpty ( ) && CConfig::inst ( )-> closeEmptyDocuments ( ));

	if ( m_doc-> isModified ( ) && !close_empty ) {
		switch ( CMessageBox::warning ( this, tr( "Save changes to %1?" ). arg ( CMB_BOLD( caption ( ). left ( caption ( ). length ( ) - 2 ))), CMessageBox::Yes | CMessageBox::Default, CMessageBox::No, CMessageBox::Cancel | CMessageBox::Escape )) {
		case CMessageBox::Yes:
			m_doc-> fileSave ( );

			if ( !m_doc-> isModified ( ))
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
	if ( !m_doc-> selection ( ). isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo, (*m_doc-> selection ( ). front ( )). item ( )));
}

void CWindow::showBLPriceGuide ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, (*m_doc-> selection ( ). front ( )). item ( ), (*m_doc-> selection ( ). front ( )). color ( )));
}

void CWindow::showBLLotsForSale ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale, (*m_doc-> selection ( ). front ( )). item ( ), (*m_doc-> selection ( ). front ( )). color ( )));
}

void CWindow::setDifferenceMode ( bool b )
{
	w_list-> setDifferenceMode ( b );
}

bool CWindow::isDifferenceMode ( ) const
{
	return w_list-> isDifferenceMode ( );
}

void CWindow::filePrint ( )
{
	DlgSelectReportImpl d ( this );

	if ( d. exec ( ) == QDialog::Accepted ) {
		const CReport *rep = d. report ( );
		uint pagecnt = rep-> pageCount ( m_doc-> items ( ). count ( ));

		QPrinter *prt = CReportManager::inst ( )-> printer ( );

		if ( rep && prt && pagecnt ) {
			//prt-> setOptionEnabled ( QPrinter::PrintToFile, false );
			//prt-> setOptionEnabled ( QPrinter::PrintSelection, true );
		
			prt-> setPageSize ( rep-> pageSize ( ));
			prt-> setFullPage ( true );

			prt-> setMinMax ( 1, pagecnt );
			prt-> setFromTo ( 1, prt-> maxPage ( ));
			prt-> setOptionEnabled ( QPrinter::PrintPageRange, true );

			if ( prt-> setup ( CFrameWork::inst ( ))) {
				QPainter p;

				if ( !p. begin ( prt ))
					return;
			
				CReportVariables add_vars;
				add_vars ["filename"] = m_doc-> fileName ( );

                if ( m_doc-> order ( )) {
					const BrickLink::Order *order = m_doc-> order ( );

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

void CWindow::fileSave ( )
{
	m_doc-> fileSave ( sortedItems ( ));
}

void CWindow::fileSaveAs ( )
{
	m_doc-> fileSaveAs ( sortedItems ( ));
}

void CWindow::fileExportBrickLinkXML ( )
{
	m_doc-> fileExportBrickLinkXML ( sortedItems ( ));
}

void CWindow::fileExportBrickLinkXMLClipboard ( )
{
	m_doc-> fileExportBrickLinkXMLClipboard ( sortedItems ( ));
}

void CWindow::fileExportBrickLinkUpdateClipboard ( )
{
	m_doc-> fileExportBrickLinkUpdateClipboard ( sortedItems ( ));
}

void CWindow::fileExportBrickLinkInvReqClipboard ( )
{
	m_doc-> fileExportBrickLinkInvReqClipboard ( sortedItems ( ));
}

void CWindow::fileExportBrickLinkWantedListClipboard ( )
{
	m_doc-> fileExportBrickLinkWantedListClipboard ( sortedItems ( ));
}

void CWindow::fileExportBrikTrakInventory ( )
{
	m_doc-> fileExportBrikTrakInventory ( sortedItems ( ));
}

CDocument::ItemList CWindow::sortedItems ( )
{
	CDocument::ItemList sorted;

	for ( QListViewItemIterator it ( w_list ); it. current ( ); ++it ) {
		CItemViewItem *ivi = static_cast <CItemViewItem *> ( it. current ( ));

		sorted. append ( ivi-> item ( ));
	}

	return sorted;
}

