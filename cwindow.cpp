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

#include "cselectcolor.h"
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

#include "cwindow.h"

namespace {

// should be a function, but MSVC.Net doesn't accept default 
// template arguments for template functions...

template <typename TG, typename TS = TG> class setOrToggle {
public:
	static void toggle ( CWindow *w, const QString &actiontext, TG (CDocument::Item::* getter ) ( ) const, void (CDocument::Item::* setter ) ( TS t ), TS val1, TS val2 )
	{ set_or_toggle ( w, true, actiontext, getter, setter, val1, val2 ); }
	static void set ( CWindow *w, const QString &actiontext, TG (CDocument::Item::* getter ) ( ) const, void (CDocument::Item::* setter ) ( TS t ), TS val )
	{ set_or_toggle ( w, false, actiontext, getter, setter, val, TG ( )); }

private:
	static void set_or_toggle ( CWindow *w, bool toggle, const QString &actiontext, TG (CDocument::Item::* getter ) ( ) const, void (CDocument::Item::* setter ) ( TS t ), TS val1, TS val2 = TG ( ))
	{
		CDocument *doc = w-> document ( );

		if ( w-> document ( )-> selection ( ). isEmpty ( ))
			return;

		//CDisableUpdates disupd ( w_list );
		CUndoCmd *macro = doc-> macroBegin ( );
		uint count = 0;

		// Apples gcc4 has problems compiling this line (internal error)
		//foreach ( CDocument::Item *pos, doc-> selection ( )) {
		for ( CDocument::ItemList::const_iterator it = doc-> selection ( ). begin  () ; it != doc-> selection ( ). end ( ); ++it ) {
			CDocument::Item *pos = *it;
			if ( toggle ) {
				TG val = ( pos->* getter ) ( );

				if ( val == val1 )
					val = val2;
				else if ( val == val2 )
					val = val1;

				if ( val != ( pos->* getter ) ( )) {
					CDocument::Item item = *pos;

					( item.* setter ) ( val );
					doc-> changeItem ( pos, item );

					count++;
				}
			}
			else {
				if (( pos->* getter ) ( ) != val1 ) {
					CDocument::Item item = *pos;

					( item.* setter ) ( val1 );
					doc-> changeItem ( pos, item );

					count++;
				}
			}
		}
		doc-> macroEnd ( macro, actiontext. arg( count ));
	}
};

} // namespace

CWindow::CWindow ( CDocument *doc, QWidget *parent, const char *name )
	: QWidget ( parent, name, WDestructiveClose ), m_lvitems ( 503 )
{
	m_doc = doc;
	m_ignore_selection_update = false;
	
	m_settopg_failcnt = 0;
	m_settopg_list = 0; 
	m_settopg_time = BrickLink::PriceGuide::AllTime;
	m_settopg_price = BrickLink::PriceGuide::Average;

	// m_items. setAutoDelete ( true ); NOT ANYMORE -> UndoRedo
	
	w_filter_label = new QLabel ( this );

	w_list = new CItemView ( doc, this );
	setFocusProxy ( w_list );
	w_list-> setShowSortIndicator ( true );
	w_list-> setColumnsHideable ( true );
	w_list-> setDifferenceMode ( false );
	w_list-> setSimpleMode ( CConfig::inst ( )-> simpleMode ( ));

	if ( doc-> doNotSortItems ( ))
		w_list-> setSorting ( w_list-> columns ( ) + 1 );

	connect ( w_list, SIGNAL( selectionChanged ( )), this, SLOT( updateSelectionFromView ( )));

	w_filter = w_list-> createFilterWidget ( this );

	QBoxLayout *toplay = new QVBoxLayout ( this, 0, 0 );
	QBoxLayout *barlay = new QHBoxLayout ( toplay, 4 );
	barlay-> setMargin ( 4 );
	barlay-> addWidget ( w_filter_label, 0 );
	barlay-> addWidget ( w_filter );
	toplay-> addWidget ( w_list );

	connect ( BrickLink::inst ( ), SIGNAL( priceGuideUpdated ( BrickLink::PriceGuide * )), this, SLOT( priceGuideUpdated ( BrickLink::PriceGuide * )));
	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));

	connect ( w_list, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( contextMenu ( QListViewItem *, const QPoint & )));

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
//	w_filter_field_label-> setText ( tr( "Field:" ));
#if 0
	QToolTip::add ( m_filter-> w_expression, tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));
//	QToolTip::add ( w_filter_clear, tr( "Reset an active filter" ));
	QToolTip::add ( m_filter-> w_fields, tr( "Restrict the filter to this/these field(s)" ));

	int i, j;
	for ( i = 0; i < CItemView::FilterCountSpecial; i++ ) {
		QString s;
		switch ( -i - 1 ) {
			case CItemView::All       : s = tr( "All" ); break;
			case CItemView::Prices    : s = tr( "All Prices" ); break;
			case CItemView::Texts     : s = tr( "All Texts" ); break;
			case CItemView::Quantities: s = tr( "All Quantities" ); break;
		}
		m_filter-> w_fields-> changeItem ( s, i );
	}
	for ( j = 0; j < CDocument::FieldCount; j++ )
		m_filter-> w_fields-> changeItem ( w_list-> header ( )-> label ( j ), i + j );

    for ( Filter *f = m_filter-> m_next; f; f = f-> m_next )
        f-> copyUiTextsFrom ( m_filter );
#endif
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
}

void CWindow::saveDefaultColumnLayout ( )
{
	w_list-> saveDefaultLayout ( );
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
				    ( newitem-> color ( ) == item-> color ( )) && 
				    ( newitem-> status ( ) == item-> status ( )) &&
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
		m_doc-> macroEnd ( macro, tr( "Added %1, merged %2 items" ). arg( addcount ). arg( mergecount ));

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
				(( from-> status ( ) == BrickLink::InvItem::Exclude ) == ( find_to-> status ( ) == BrickLink::InvItem::Exclude )) &&
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
	m_doc-> macroEnd ( macro, tr( "Merged %1 items" ). arg ( mergecount ));
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


void CWindow::editStatusInclude ( )
{
	setOrToggle<BrickLink::InvItem::Status>::set ( this, tr( "Set 'include' status on %1 items" ), &CDocument::Item::status, &CDocument::Item::setStatus, BrickLink::InvItem::Include );
}

void CWindow::editStatusExclude ( )
{
	setOrToggle<BrickLink::InvItem::Status>::set ( this, tr( "Set 'exclude' status on %1 items" ), &CDocument::Item::status, &CDocument::Item::setStatus, BrickLink::InvItem::Exclude );
}

void CWindow::editStatusExtra ( )
{
	setOrToggle<BrickLink::InvItem::Status>::set ( this, tr( "Set 'extra' status on %1 items" ), &CDocument::Item::status, &CDocument::Item::setStatus, BrickLink::InvItem::Extra );
}

void CWindow::editStatusToggle ( )
{
	setOrToggle<BrickLink::InvItem::Status>::toggle ( this, tr( "Toggled status on %1 items" ), &CDocument::Item::status, &CDocument::Item::setStatus, BrickLink::InvItem::Include, BrickLink::InvItem::Exclude );
}

void CWindow::editConditionNew ( )
{
	setOrToggle<BrickLink::Condition>::set ( this, tr( "Set 'new' condition on %1 items" ), &CDocument::Item::condition, &CDocument::Item::setCondition, BrickLink::New );
}

void CWindow::editConditionUsed ( )
{
	setOrToggle<BrickLink::Condition>::set ( this, tr( "Set 'used' condition on %1 items" ), &CDocument::Item::condition, &CDocument::Item::setCondition, BrickLink::Used );
}

void CWindow::editConditionToggle ( )
{
	setOrToggle<BrickLink::Condition>::toggle ( this, tr( "Toggled condition on %1 items" ), &CDocument::Item::condition, &CDocument::Item::setCondition, BrickLink::New, BrickLink::Used );
}

void CWindow::editRetainYes ( )
{
	setOrToggle<bool>::set ( this, tr( "Set 'retain' flag on %1 items" ), &CDocument::Item::retain, &CDocument::Item::setRetain, true );
}

void CWindow::editRetainNo ( )
{
	setOrToggle<bool>::set ( this, tr( "Cleared 'retain' flag on %1 items" ), &CDocument::Item::retain, &CDocument::Item::setRetain, false );
}

void CWindow::editRetainToggle ( )
{
	setOrToggle<bool>::toggle ( this, tr( "Toggled 'retain' flag on %1 items" ), &CDocument::Item::retain, &CDocument::Item::setRetain, true, false );
}

void CWindow::editStockroomYes ( )
{
	setOrToggle<bool>::set ( this, tr( "Set 'stockroom' flag on %1 items" ), &CDocument::Item::stockroom, &CDocument::Item::setStockroom, true );
}

void CWindow::editStockroomNo ( )
{
	setOrToggle<bool>::set ( this, tr( "Cleared 'stockroom' flag on %1 items" ), &CDocument::Item::stockroom, &CDocument::Item::setStockroom, false );
}

void CWindow::editStockroomToggle ( )
{
	setOrToggle<bool>::toggle ( this, tr( "Toggled 'stockroom' flag on %1 items" ), &CDocument::Item::stockroom, &CDocument::Item::setStockroom, true, false );
}


void CWindow::editPrice ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString price = m_doc-> selection ( ). front ( )-> price ( ). toLocalizedString ( false );

	if ( CMessageBox::getString ( this, tr( "Enter the new price for all selected items:" ), CMoney::inst ( )-> currencySymbol ( ), price, new CMoneyValidator ( 0, 10000, 3, 0 ))) {
		setOrToggle<money_t>::set ( this, tr( "Set price on %1 items" ), &CDocument::Item::price, &CDocument::Item::setPrice, money_t::fromLocalizedString ( price ));
	}
}

void CWindow::editPriceRound ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	uint roundcount = 0;
	CUndoCmd *macro = m_doc-> macroBegin ( );

	CDisableUpdates disupd ( w_list );

	foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
		money_t p = (( pos-> price ( ) + money_t ( 0.005 )) / 10 ) * 10;

		if ( p != pos-> price ( )) {
			CDocument::Item item = *pos;

			item. setPrice ( p );
			m_doc-> changeItem ( pos, item );
		
			roundcount++;
		}
	}
	m_doc-> macroEnd ( macro, tr( "Price change on %1 items" ). arg ( roundcount ));
}


void CWindow::editPriceToPG ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	if ( m_settopg_list ) {
		CMessageBox::information ( this, tr( "Prices are currently updated to Price Guide values.<br /><br />Please wait until this operation has finished." ));
		return;
	}

	DlgSetToPGImpl dlg ( this );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		CDisableUpdates disupd ( w_list );

		m_settopg_list    = new QPtrDict<CDocument::Item> ( 251 );
		m_settopg_failcnt = 0;
		m_settopg_time    = dlg. time ( );
		m_settopg_price   = dlg. price ( );
		bool force_update = dlg. forceUpdate ( );

		foreach ( CDocument::Item *item, m_doc-> selection ( )) {
			BrickLink::PriceGuide *pg = BrickLink::inst ( )-> priceGuide ( item-> item ( ), item-> color ( ));

			if ( force_update && pg && ( pg-> updateStatus ( ) != BrickLink::Updating ))
				pg-> update ( );

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

		if ( m_settopg_list && m_settopg_list-> isEmpty ( ))
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
		QString s = tr( "Prices of the selected items have been updated to Price Guide values." );

		if ( m_settopg_failcnt )
			s += "<br /><br />" + tr( "%1 have been skipped, because of missing Price Guide records and/or network errors." ). arg ( CMB_BOLD( QString::number ( m_settopg_failcnt )));

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
		uint incdeccount = 0;

		CUndoCmd *macro = m_doc-> macroBegin ( );

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
			incdeccount++;
		}

		m_doc-> macroEnd ( macro, tr( "Price change on %1 items" ). arg ( incdeccount ));
	}
}

void CWindow::editQtyDivide ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int divisor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisible without reminder, before this operation is performed.)" ), QString::null, divisor, new QIntValidator ( 1, 1000, 0 ))) {
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
				uint divcount = 0;
				CUndoCmd *macro = m_doc-> macroBegin ( );
				
				CDisableUpdates disupd ( w_list );

				foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
					CDocument::Item item = *pos;

					item. setQuantity ( item. quantity ( ) / divisor );
					m_doc-> changeItem ( pos, item );
					
					divcount++;
				}
				m_doc-> macroEnd ( macro, tr( "Quantity divide by %1 on %2 Items" ). arg ( divisor ). arg ( divcount ));
			}
		}
	}
}

void CWindow::editQtyMultiply ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int factor = 1;

	if ( CMessageBox::getInteger ( this, tr( "Multiply the quantities of all selected items with this factor." ), tr( "x" ), factor, new QIntValidator ( -1000, 1000, 0 ))) {
		if (( factor <= 1 ) || ( factor > 1 )) {
			uint mulcount = 0;
			CUndoCmd *macro = m_doc-> macroBegin ( );
			
			CDisableUpdates disupd ( w_list );

			foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
				CDocument::Item item = *pos;

				item. setQuantity ( item. quantity ( ) * factor );
				m_doc-> changeItem ( pos, item );
				
				mulcount++;
			}
			m_doc-> macroEnd ( macro, tr( "Quantity multiply by %1 on %2 Items" ). arg ( factor ). arg ( mulcount ));
		}
	}
}

void CWindow::editSale ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int sale = m_doc-> selection ( ). front ( )-> sale ( );

	if ( CMessageBox::getInteger ( this, tr( "Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed." ), tr( "%" ), sale, new QIntValidator ( -1000, 99, 0 ))) 
		setOrToggle<int>::set ( this, tr( "Set sale on %1 items" ), &CDocument::Item::sale, &CDocument::Item::setSale, sale );
}

void CWindow::editBulk ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	int bulk = m_doc-> selection ( ). front ( )-> bulkQuantity ( );

	if ( CMessageBox::getInteger ( this, tr( "Set bulk quantity for the selected items:"), QString ( ), bulk, new QIntValidator ( 1, 99999, 0 ))) 
		setOrToggle<int>::set ( this, tr( "Set bulk quantity on %1 items" ), &CDocument::Item::bulkQuantity, &CDocument::Item::setBulkQuantity, bulk );
}

void CWindow::editColor ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	CSelectColorDialog d ( this );
	d. setCaption ( CItemView::tr( "Modify Color" ));
	d. setColor ( m_doc-> selection ( ). front ( )-> color ( ));

	if ( d. exec ( ) == QDialog::Accepted )
		setOrToggle<const BrickLink::Color *>::set ( this, tr( "Set color on %1 items" ), &CDocument::Item::color, &CDocument::Item::setColor, d. color ( ));
}

void CWindow::editRemark ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString remarks = m_doc-> selection ( ). front ( )-> remarks ( );
	
	if ( CMessageBox::getString ( this, tr( "Enter the new remark for all selected items:" ), remarks ))
		setOrToggle<QString, const QString &>::set ( this, tr( "Set remark on %1 items" ), &CDocument::Item::remarks, &CDocument::Item::setRemarks, remarks );
}

void CWindow::addRemark ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString addremarks;
	
	if ( CMessageBox::getString ( this, tr( "Enter the text, that should be added to the remarks of all selected items:" ), addremarks )) {
		uint remarkcount = 0;
		CUndoCmd *macro = m_doc-> macroBegin ( );	                             
	                             
		QRegExp regexp ( QRegExp( "\\b" + QRegExp::escape ( addremarks ) + "\\b" ));

		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			QString str = pos-> remarks ( );
			
			if ( str. isEmpty ( ))
				str = addremarks;
			else if ( str. find ( regexp ) != -1 )
				;
			else if ( addremarks. find ( QRegExp( "\\b" + QRegExp::escape ( str ) + "\\b" )) != -1 )
				str = addremarks;
			else			
				str = str + " " + addremarks;
			
			if ( str != pos-> remarks ( )) {
				CDocument::Item item = *pos;

				item. setRemarks ( str );
				m_doc-> changeItem ( pos, item );
			
				remarkcount++;
			}
		}
		m_doc-> macroEnd ( macro, tr( "Modified remark on %1 items" ). arg ( remarkcount ));
	}
}

void CWindow::removeRemark ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString remremarks;
	
	if ( CMessageBox::getString ( this, tr( "Enter the text, that should be removed from the remarks of all selected items:" ), remremarks )) {
		uint remarkcount = 0;
		CUndoCmd *macro = m_doc-> macroBegin ( );

		QRegExp regexp ( QRegExp( "\\b" + QRegExp::escape ( remremarks ) + "\\b" ));

		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			QString str = pos-> remarks ( );
			str. remove ( regexp );
			str = str. simplifyWhiteSpace ( );
			
			if ( str != pos-> remarks ( )) {
				CDocument::Item item = *pos;

				item. setRemarks ( str );
				m_doc-> changeItem ( pos, item );
				
				remarkcount++;
			}
		}
		m_doc-> macroEnd ( macro, tr( "Modified remark on %1 items" ). arg ( remarkcount ));
	}
}


void CWindow::editComment ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString comments = m_doc-> selection ( ). front ( )-> comments ( );
	
	if ( CMessageBox::getString ( this, tr( "Enter the new comment for all selected items:" ), comments ))
		setOrToggle<QString, const QString &>::set ( this, tr( "Set comment on %1 items" ), &CDocument::Item::comments, &CDocument::Item::setComments, comments );
}

void CWindow::addComment ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString addcomments;
	
	if ( CMessageBox::getString ( this, tr( "Enter the text, that should be added to the comments of all selected items:" ), addcomments )) {
		uint commentcount = 0;
		CUndoCmd *macro = m_doc-> macroBegin ( );	                             
	       
		QRegExp regexp ( QRegExp( "\\b" + QRegExp::escape ( addcomments ) + "\\b" ));
	                             
		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			QString str = pos-> comments ( );
			
			if ( str. isEmpty ( ))
				str = addcomments;
			else if ( str. find ( regexp ) != -1 )
				;
			else if ( addcomments. find ( QRegExp( "\\b" + QRegExp::escape ( str ) + "\\b" )) != -1 )
				str = addcomments;
			else			
				str = str + " " + addcomments;
			
			if ( str != pos-> comments ( )) {
				CDocument::Item item = *pos;

				item. setComments ( str );
				m_doc-> changeItem ( pos, item );
			
				commentcount++;
			}
		}
		m_doc-> macroEnd ( macro, tr( "Modified comment on %1 items" ). arg ( commentcount ));
	}
}

void CWindow::removeComment ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString remcomments;
	
	if ( CMessageBox::getString ( this, tr( "Enter the text, that should be removed from the comments of all selected items:" ), remcomments )) {
		uint commentcount = 0;
		CUndoCmd *macro = m_doc-> macroBegin ( );
		
		QRegExp regexp ( QRegExp( "\\b" + QRegExp::escape ( remcomments ) + "\\b" ));
	                             
		CDisableUpdates disupd ( w_list );

		foreach ( CDocument::Item *pos, m_doc-> selection ( )) {
			QString str = pos-> comments ( );
			str. remove ( regexp );
			str = str. simplifyWhiteSpace ( );
			
			if ( str != pos-> comments ( )) {
				CDocument::Item item = *pos;

				item. setComments ( str );
				m_doc-> changeItem ( pos, item );
				
				commentcount++;
			}
		}
		m_doc-> macroEnd ( macro, tr( "Modified comment on %1 items" ). arg ( commentcount ));
	}
}


void CWindow::editReserved ( )
{
	if ( m_doc-> selection ( ). isEmpty ( ))
		return;

	QString reserved = m_doc-> selection ( ). front ( )-> reserved ( );
	
	if ( CMessageBox::getString ( this, tr( "Reserve all selected items for this specific buyer (BrickLink username):" ), reserved ))
		setOrToggle<QString, const QString &>::set ( this, tr( "Set reservation on %1 items" ), &CDocument::Item::reserved, &CDocument::Item::setReserved, reserved );
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

void CWindow::editCopyRemarks ( )
{
	DlgSubtractItemImpl d ( tr( "Please choose the document that should serve as a source to fill in the remarks fields of the current document:" ), this, "CopyRemarkDlg" );

	if ( d. exec ( ) == QDialog::Accepted ) {
		BrickLink::InvItemList list = d. items ( );

		if ( !list. isEmpty ( ))
			copyRemarks ( list );

		qDeleteAll ( list );
	}
}

void CWindow::copyRemarks ( const BrickLink::InvItemList &items )
{
	if ( items. isEmpty ( ))
		return;

	CUndoCmd *macro = m_doc-> macroBegin ( );

	CDisableUpdates disupd ( w_list );

	int copy_count = 0;

	foreach ( CDocument::Item *pos, m_doc-> items ( )) {
		BrickLink::InvItem *match = 0;

		foreach ( BrickLink::InvItem *ii, items ) {
			const BrickLink::Item *item   = ii-> item ( );
			const BrickLink::Color *color = ii-> color ( );
			BrickLink::Condition cond     = ii-> condition ( );
			int qty                       = ii-> quantity ( );

			if ( !item || !color || !qty )
				continue;

			if (( pos-> item ( ) == item ) && ( pos-> condition ( ) == cond )) {
				if ( pos-> color ( ) == color )
					match = ii;
				else if ( !match )
					match = ii;
			}
		}

		if ( match && !match-> remarks ( ). isEmpty ( )) {
			CDocument::Item newitem = *pos;
			newitem. setRemarks ( match-> remarks ( ));
			m_doc-> changeItem ( pos, newitem );

			copy_count++;
		}
	}
	m_doc-> macroEnd ( macro, tr( "Copied Remarks for %1 Items" ). arg( copy_count ));
}

void CWindow::editSubtractItems ( )
{
	DlgSubtractItemImpl d ( tr( "Which items should be subtracted from the current document:" ), this, "SubtractItemDlg" );

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
		foreach ( CDocument::Item *item, m_doc-> selection ( ))
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
			m_doc-> fileSave ( sortedItems ( ));

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

void CWindow::showBLMyInventory ( )
{
	if ( !m_doc-> selection ( ). isEmpty ( )) {
		uint lotid = (*m_doc-> selection ( ). front ( )). lotId ( );
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_StoreItemDetail, &lotid ));
	}
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
	if ( m_doc-> items ( ). isEmpty ( ))
		return;

	if ( CReportManager::inst ( )-> reports ( ). isEmpty ( )) {
		CReportManager::inst ( )->reload ( );

		if ( CReportManager::inst ( )-> reports ( ). isEmpty ( ))
			return;
	}

	QPrinter *prt = CReportManager::inst ( )-> printer ( );

	if ( !prt )
		return;

	//prt-> setOptionEnabled ( QPrinter::PrintToFile, false );
	prt-> setOptionEnabled ( QPrinter::PrintSelection, !m_doc->selection ( ). isEmpty ( ));
	prt-> setOptionEnabled ( QPrinter::PrintPageRange, false );
	prt-> setPrintRange ( m_doc-> selection ( ). isEmpty ( ) ? QPrinter::AllPages : QPrinter::Selection );

    QString doctitle = m_doc-> title ( );
    if ( doctitle == m_doc-> fileName ( ))
        doctitle = QFileInfo ( doctitle ). baseName ( );

    prt-> setDocName ( doctitle );

	prt-> setFullPage ( true );
	

	if ( !prt-> setup ( CFrameWork::inst ( )))
		return;

	DlgSelectReportImpl d ( this );

	if ( d. exec ( ) != QDialog::Accepted )
		return;

	const CReport *rep = d. report ( );

	if ( !rep )
		return;

	rep-> print ( prt, m_doc, sortedItems ( prt-> printRange ( ) == QPrinter::Selection ));
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
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrickLinkXML ( items );
}

void CWindow::fileExportBrickLinkXMLClipboard ( )
{
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrickLinkXMLClipboard ( items );
}

void CWindow::fileExportBrickLinkUpdateClipboard ( )
{
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrickLinkUpdateClipboard ( items );
}

void CWindow::fileExportBrickLinkInvReqClipboard ( )
{
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrickLinkInvReqClipboard ( items );
}

void CWindow::fileExportBrickLinkWantedListClipboard ( )
{
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrickLinkWantedListClipboard ( items );
}

void CWindow::fileExportBrikTrakInventory ( )
{
	CDocument::ItemList items = exportCheck ( );

	if ( !items. isEmpty ( ))
		m_doc-> fileExportBrikTrakInventory ( items );
}

CDocument::ItemList CWindow::exportCheck ( ) const
{
	CDocument::ItemList items;
	bool selection_only = false;

	if ( !m_doc-> selection ( ). isEmpty ( ) && ( m_doc-> selection ( ). count ( ) != m_doc-> items ( ). count ( ))) {
		if ( CMessageBox::question ( CFrameWork::inst ( ), tr( "There are %1 items selected.<br /><br />Do you want to export only these items?" ). arg ( m_doc-> selection ( ). count ( )), CMessageBox::Yes, CMessageBox::No ) == CMessageBox::Yes ) {
			selection_only = true;
		}
	}

	if ( m_doc-> statistics ( selection_only ? m_doc-> selection ( ) : m_doc-> items ( )). errors ( )) {
		if ( CMessageBox::warning ( CFrameWork::inst ( ), tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
			return CDocument::ItemList ( );
	}	

	return sortedItems ( selection_only );
}

CDocument::ItemList CWindow::sortedItems ( bool selection_only ) const
{
	CDocument::ItemList sorted;

	for ( QListViewItemIterator it ( w_list ); it. current ( ); ++it ) {
		CItemViewItem *ivi = static_cast <CItemViewItem *> ( it. current ( ));

		if ( selection_only && ( m_doc-> selection ( ). find ( ivi-> item ( )) == m_doc-> selection ( ). end ( )))
			continue;

		sorted. append ( ivi-> item ( ));
	}

	return sorted;
}

