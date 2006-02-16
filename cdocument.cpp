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

#include <qapplication.h>
#include <qcursor.h>
#include <qfiledialog.h>
#include <qclipboard.h>
#include <qprinter.h>
#include <qpainter.h>

#include "cundo.h"
#include "cutility.h"
#include "cconfig.h"
#include "cframework.h"
#include "cmessagebox.h"
#include "creport.h"

#include "dlgloadorderimpl.h"
#include "dlgloadinventoryimpl.h"
#include "dlgselectreportimpl.h"
#include "dlgincompleteitemimpl.h"

#include "cimport.h"

#include "cdocument.h"
#include "cdocument_p.h"

namespace {

template <typename T> static inline T pack ( typename T::const_reference item )
{
	T list;
	list. append ( item );
	return list;
}

};

CChangeCmd::CChangeCmd ( CDocument *doc, CDocument::Item *pos, const CDocument::Item &item, bool can_merge )
	: CUndoCmd ( tr( "Modified Item" ), can_merge ), m_doc ( doc ), m_position ( pos ), m_item ( item )
{ }

void CChangeCmd::redo ( )
{
	m_doc-> changeItemDirect ( m_position, m_item );
}

void CChangeCmd::undo ( )
{
	redo ( );
}

bool CChangeCmd::mergeMeWith ( CUndoCmd *other )
{
	CChangeCmd *that = ::qt_cast <CChangeCmd *> ( other );

	if (( m_doc == that-> m_doc ) &&
		( m_position == that-> m_position )) 
	{
		return true;
	}
	return false;
}


CAddRemoveCmd::CAddRemoveCmd ( Type t, CDocument *doc, const CDocument::ItemList &positions, const CDocument::ItemList &items, bool can_merge )
	: CUndoCmd ( genDesc ( t == Add, QMAX( items. count ( ), positions. count ( ))), can_merge ), 
	  m_doc ( doc ), m_positions ( positions ), m_items ( items ), m_type ( t )
{ }

CAddRemoveCmd::~CAddRemoveCmd ( )
{
	if ( m_type == Add )
		qDeleteAll ( m_items );
}

void CAddRemoveCmd::redo ( )
{
	if ( m_type == Add ) {
		// CDocument::insertItemsDirect() needs to update the itlist iterators to point directly to the items!
		m_doc-> insertItemsDirect ( m_items, m_positions );
		m_positions. clear ( );
		m_type = Remove;
	}
	else {
		// CDocument::removeItems() needs to update the itlist iterators to point directly past the items
		//                                    and fill the m_item_list with the items
		m_doc-> removeItemsDirect ( m_items, m_positions );
		m_type = Add;
	}
}

void CAddRemoveCmd::undo ( )
{
	redo ( );
}

bool CAddRemoveCmd::mergeMeWith ( CUndoCmd *other )
{
	CAddRemoveCmd *that = ::qt_cast <CAddRemoveCmd *> ( other );

	if ( m_type == that-> m_type ) {
		m_items     += that-> m_items;
		m_positions += that-> m_positions;
		setDescription ( genDesc ( m_type == Remove, QMAX( m_items. count ( ), m_positions. count ( ))));

		that-> m_items. clear ( );
		that-> m_positions. clear ( );
		return true;
	}
	return false;
}

QString CAddRemoveCmd::genDesc ( bool is_add, uint count )
{
	if ( is_add )
		return ( count > 1 ) ? tr( "Added %1 Items" ). arg( count ) : tr( "Added an Item" );
	else
		return ( count > 1 ) ? tr( "Removed %1 Items" ). arg( count ) : tr( "Removed an Item" );
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************

CDocument::Statistics::Statistics ( const CDocument *doc, const ItemList &list )
{
	m_lots = list. count ( );
	m_items = 0;
	m_val = m_minval = 0.;
	m_weight = .0;
	m_errors = 0;
	bool weight_missing = false;

	foreach ( const Item *item, list ) {
		int qty = item-> quantity ( );
		money_t price = item-> price ( );

		m_val += ( qty * price );

		for ( int i = 0; i < 3; i++ ) {
			if ( item-> tierQuantity ( i ) && ( item-> tierPrice ( i ) != 0 ))
				price = item-> tierPrice ( i );
		}
		m_minval += ( qty * price * ( 1.0 - double( item-> sale ( )) / 100.0 ));
		m_items += qty;

		if ( item-> weight ( ) > 0 )
			m_weight += item-> weight ( );
		else 
			weight_missing = true;	

		if ( item-> errors ( )) {
			Q_UINT64 errors = item-> errors ( ) & doc-> m_error_mask;

			for ( uint i = 1ULL << ( FieldCount - 1 ); i;  i >>= 1 ) {
				if ( errors & i )
					m_errors++;
			}
		}
	}
	if ( weight_missing )
		m_weight = ( m_weight == 0. ) ? -DBL_MIN : -m_weight;
}


// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************


CDocument::Item::Item ( )
	: BrickLink::InvItem ( 0, 0 ), m_errors ( 0 )
{ }

CDocument::Item::Item ( const BrickLink::InvItem &copy )
	: BrickLink::InvItem ( copy ), m_errors ( 0 )
{ }

CDocument::Item::Item ( const Item &copy )
	: BrickLink::InvItem ( copy ), m_errors ( copy. m_errors )
{ }

CDocument::Item &CDocument::Item::operator = ( const Item &copy )
{
	BrickLink::InvItem::operator = ( copy );

	m_errors = copy. m_errors;

	return *this;
}

bool CDocument::Item::operator == ( const Item &cmp ) const
{
	// ignore errors for now!
	return BrickLink::InvItem::operator == ( cmp );
}

// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************
// *****************************************************************************************


CDocument::CDocument ( )
{
	m_undo = new CUndoStack ( this );
	m_order = 0;
	m_error_mask = 0;

	connect ( m_undo, SIGNAL( cleanChanged ( bool )), this, SLOT( clean2Modified ( bool )));
}

CDocument::~CDocument ( )
{ 
	delete m_order;
	qDeleteAll ( m_items );
}

const CDocument::ItemList &CDocument::items ( ) const
{
	return m_items;
}

const CDocument::ItemList &CDocument::selection ( ) const
{
	return m_selection;
}

void CDocument::setSelection ( const ItemList &selection )
{
	m_selection = selection;

	emit selectionChanged ( m_selection );
}

CDocument::Statistics CDocument::statistics ( const ItemList &list ) const
{
	return Statistics ( this, list );
}

CUndoCmd *CDocument::macroBegin ( const QString &label )
{
	CUndoCmd *cmd = new CUndoCmd ( CUndoCmd::MacroBegin, label );

	m_undo-> push ( cmd );
	return cmd;
}
	
void CDocument::macroEnd ( CUndoCmd *cmd, const QString &label )
{
	if ( cmd && ( cmd-> type ( ) == CUndoCmd::MacroBegin ))
		m_undo-> push ( new CUndoCmd ( CUndoCmd::MacroEnd, label ));
}

void CDocument::addView ( QWidget *view, IDocumentView *docview )
{ 
	CUndoManager::inst ( )-> associateView ( view, m_undo ); 

	if ( docview ) {
		m_views. append ( docview );

		docview-> parseGuiStateXML ( m_gui_state );
	}
}

bool CDocument::clear ( )
{
	m_undo-> push ( new CAddRemoveCmd ( CAddRemoveCmd::Remove, this, ItemList ( ), m_items ));
	return true;
}

bool CDocument::insertItems ( const ItemList &positions, const ItemList &items )
{
	m_undo-> push ( new CAddRemoveCmd ( CAddRemoveCmd::Add, this, positions, items /*, true*/ ));
	return true;
}

bool CDocument::removeItems ( const ItemList &positions )
{
	m_undo-> push ( new CAddRemoveCmd ( CAddRemoveCmd::Remove, this, ItemList ( ), positions /*, true*/ ));
	return true;
}

bool CDocument::insertItem ( Item *position, Item *item )
{
	return insertItems ( pack<ItemList> ( position ), pack<ItemList> ( item ));
}

bool CDocument::removeItem ( Item *position )
{
	return removeItems ( pack<ItemList> ( position ));
}

bool CDocument::changeItem ( Item *position, const Item &item )
{
	if (!( item == *position ))
		m_undo-> push ( new CChangeCmd ( this, position, item /*, true*/ ));
	return true;
}

void CDocument::insertItemsDirect ( ItemList &items, ItemList &positions )
{
	ItemList::iterator pos = positions. begin ( );

	foreach ( Item *item, items ) {
		if ( pos != positions. end ( )) {
			m_items. insert ( m_items. find ( *pos ), item );
			++pos;
		}
		else {
			m_items. append ( item );
		}
		updateErrors ( item );
	}

	emit itemsAdded ( items );
	emit statisticsChanged ( );
}

void CDocument::removeItemsDirect ( ItemList &items, ItemList &positions )
{
	positions. clear ( );

	emit itemsAboutToBeRemoved ( items );

	bool selection_changed = false;

	foreach ( Item *item, items ) {
		ItemList::iterator next = m_items. erase ( m_items. find ( item ));

		positions. append (( next != m_items. end ( )) ? *next : 0 );

		ItemList::iterator selit = m_selection. find ( item );
		
		if ( selit != m_selection. end ( )) {
			m_selection.erase ( selit );
			selection_changed = true;
		}
	}

	if ( selection_changed )
		emit selectionChanged ( m_selection );

	emit itemsRemoved ( items );
	emit statisticsChanged ( );
}

void CDocument::changeItemDirect ( Item *position, Item &item )
{
	qSwap( *position, item );

	bool grave = ( position-> item ( ) != item. item ( )) || ( position->color ( ) != item. color ( ));

	emit itemsChanged ( pack<ItemList> ( position ), grave );
	emit statisticsChanged ( );

	if ( m_selection. find ( position ) != m_selection. end ( ))
		emit selectionChanged ( m_selection );

}

void CDocument::updateErrors ( Item *item )
{
	Q_UINT64 errors = 0;

	if ( item-> price ( ) <= 0 )
		errors |= ( 1ULL << Price );

	if ( item-> quantity ( ) <= 0 )
		errors |= ( 1ULL << Quantity );

	if (( item-> color ( )-> id ( ) != 0 ) && ( !item-> itemType ( )-> hasColors ( )))
		errors |= ( 1ULL << Color );

	if ( item-> tierQuantity ( 0 ) && (( item-> tierPrice ( 0 ) <= 0 ) || ( item-> tierPrice ( 0 ) >= item-> price ( ))))
		errors |= ( 1ULL << TierP1 );

	if ( item-> tierQuantity ( 1 ) && (( item-> tierPrice ( 1 ) <= 0 ) || ( item-> tierPrice ( 1 ) >= item-> tierPrice ( 0 ))))
		errors |= ( 1ULL << TierP2 );

	if ( item-> tierQuantity ( 1 ) && ( item-> tierQuantity ( 1 ) <= item-> tierQuantity ( 0 )))
		errors |= ( 1ULL << TierQ2 );

	if ( item-> tierQuantity ( 2 ) && (( item-> tierPrice ( 2 ) <= 0 ) || ( item-> tierPrice ( 2 ) >= item-> tierPrice ( 1 ))))
		errors |= ( 1ULL << TierP3 );

	if ( item-> tierQuantity ( 2 ) && ( item-> tierQuantity ( 2 ) <= item-> tierQuantity ( 1 )))
		errors |= ( 1ULL << TierQ3 );

	if ( errors != item-> errors ( )) {
		item-> setErrors ( errors );
		emit errorsChanged ( item );
		emit statisticsChanged ( );
	}
}


CDocument *CDocument::fileNew ( )
{
	CDocument *doc = new CDocument ( );
	doc-> setTitle ( tr( "Untitled" ));
	return doc;
}

CDocument *CDocument::fileOpen ( )
{
	QStringList filters;
	filters << tr( "BrickStore XML Data" ) + " (*.bsx)";
	filters << tr( "All Files" ) + "(*.*)";

	return fileOpen ( QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Open File" ), 0 ));
}

CDocument *CDocument::fileOpen ( const QString &s )
{
	if ( !s. isEmpty ( ))
		return fileLoadFrom ( s, "bsx" );
	else
		return 0;
}

CDocument *CDocument::fileImportBrickLinkInventory ( const BrickLink::Item *preselect )
{
	DlgLoadInventoryImpl dlg ( CFrameWork::inst ( ));

	if ( preselect )
		dlg. setItem ( preselect );

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		const BrickLink::Item *it = dlg. item ( );
		int qty = dlg. quantity ( );

		if ( it && ( qty > 0 )) {
			BrickLink::InvItemList items = it-> consistsOf ( );

			if ( !items. isEmpty ( )) {
				CDocument *doc = new CDocument ( );

				doc-> setBrickLinkItems ( items, qty );
				doc-> setTitle ( tr( "Inventory for %1" ). arg ( it-> id ( )));
				return doc;
			}
			else
				CMessageBox::warning ( CFrameWork::inst ( ), tr( "Internal error: Could not create an Inventory object for item %1" ). arg ( CMB_BOLD( it-> id ( ))));
		}
		else
			CMessageBox::warning ( CFrameWork::inst ( ), tr( "Requested item was not found in the database." ));
	}
	return 0;
}

CDocument *CDocument::fileImportBrickLinkOrder ( )
{
	DlgLoadOrderImpl dlg ( CFrameWork::inst ( ));

	if ( dlg. exec ( ) == QDialog::Accepted ) {
		QString id = dlg. orderId ( );
		BrickLink::Order::Type type = dlg. orderType ( );

		if ( id. length ( ) == 6 ) {
			CProgressDialog d ( CFrameWork::inst ( ));
			CImportBLOrder import ( id, type, &d );

			if ( d. exec ( ) == QDialog::Accepted ) {
				CDocument *doc = new CDocument ( );

				doc-> setTitle ( tr( "Order #%1" ). arg ( id ));
				doc-> setBrickLinkItems ( import. items ( ));	
				doc-> m_order = import. order ( );
				return doc;
			}
		}
		else
			CMessageBox::warning ( CFrameWork::inst ( ), tr( "Invalid order number." ));
	}
	return 0;
}

CDocument *CDocument::fileImportBrickLinkStore ( )
{
	CProgressDialog d ( CFrameWork::inst ( ));
	CImportBLStore import ( &d );

	if ( d. exec ( ) == QDialog::Accepted ) {
		CDocument *doc = new CDocument ( );

		doc-> setTitle ( tr( "Store %1" ). arg ( QDate::currentDate ( ). toString ( Qt::LocalDate )));
		doc-> setBrickLinkItems ( import. items ( ));	
		return doc;
	}
	return 0;
}

CDocument *CDocument::fileImportBrickLinkXML ( )
{
	QStringList filters;
	filters << tr( "BrickLink XML File" ) + " (*.xml)";
	filters << tr( "All Files" ) + "(*.*)";

	QString s = QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Import File" ), 0 );

	if ( !s. isEmpty ( )) {
		CDocument *doc = fileLoadFrom ( s, "xml", true );

		if ( doc )
			doc-> setTitle ( tr( "Import of %1" ). arg ( QFileInfo ( s ). fileName ( )));
		return doc;
	}
	else
		return 0;
}

CDocument *CDocument::fileImportPeeronInventory ( )
{
	QString peeronid;

	if ( CMessageBox::getString ( CFrameWork::inst ( ), tr( "Enter the set ID of the Peeron inventory:" ), peeronid )) {
		CProgressDialog d ( CFrameWork::inst ( ));
		CImportPeeronInventory import ( peeronid, &d );

		if ( d. exec ( ) == QDialog::Accepted ) {
			CDocument *doc = new CDocument ( );

			doc-> setBrickLinkItems ( import. items ( ));	
			doc-> setTitle ( tr( "Peeron Inventory for %1" ). arg ( peeronid ));
			return doc;
		}
	}
	return 0;
}

CDocument *CDocument::fileImportBrikTrakInventory ( const QString &fn )
{
	QString s = fn;

	if ( s. isNull ( )) {
		QStringList filters;
		filters << tr( "BrikTrak Inventory" ) + " (*.bti)";
		filters << tr( "All Files" ) + "(*.*)";

		s = QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Import File" ), 0 );
	}

	if ( !s. isEmpty ( )) {
		CDocument *doc = fileLoadFrom ( s, "bti", true );

		if ( doc )
			doc-> setTitle ( tr( "Import of %1" ). arg ( QFileInfo ( s ). fileName ( )));
		return doc;
	}
	else
		return 0;
}

CDocument *CDocument::fileLoadFrom ( const QString &name, const char *type, bool import_only )
{
	BrickLink::ItemListXMLHint hint;

	if ( qstrcmp ( type, "bsx" ) == 0 )
		hint = BrickLink::XMLHint_BrickStore;
	else if ( qstrcmp ( type, "bti" ) == 0 )
		hint = BrickLink::XMLHint_BrikTrak;
	else if ( qstrcmp ( type, "xml" ) == 0 )
		hint = BrickLink::XMLHint_MassUpload;
	else
		return false;


	QFile f ( name );

	if ( !f. open ( IO_ReadOnly )) {
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Could not open file %1 for reading." ). arg ( CMB_BOLD( name )));
		return false;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	uint invalid_items = 0;
	BrickLink::InvItemList *items = 0;

	QString emsg;
	int eline = 0, ecol = 0;
	QDomDocument doc;
	QDomElement gui_state;

	if ( doc. setContent ( &f, &emsg, &eline, &ecol )) {
		QDomElement root = doc. documentElement ( );
		QDomElement item_elem;

		if ( hint == BrickLink::XMLHint_BrickStore ) {
			for ( QDomNode n = root. firstChild ( ); !n. isNull ( ); n = n. nextSibling ( )) {
				if ( !n. isElement ( ))
					continue;

				if ( n. nodeName ( ) == "Inventory" )
					item_elem = n. toElement ( );
				else if ( n. nodeName ( ) == "GuiState" )
					gui_state = n. cloneNode ( true ). toElement ( );
			}
		}
		else {
			item_elem = root;
		}

		items = BrickLink::inst ( )-> parseItemListXML ( item_elem, hint, &invalid_items );		
	}
	else {
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Could not parse the XML data in file %1:<br /><i>Line %2, column %3: %4</i>" ). arg ( CMB_BOLD( name )). arg ( eline ). arg ( ecol ). arg ( emsg ));
		QApplication::restoreOverrideCursor ( );
		return false;
	}

	QApplication::restoreOverrideCursor ( );

	if ( items ) {
		CDocument *doc = new CDocument ( );

		if ( invalid_items )
			CMessageBox::information ( CFrameWork::inst ( ), tr( "This file contains %1 unknown item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));

		doc-> setBrickLinkItems ( *items );
		delete items;

		doc-> setFileName ( import_only ? QString::null : name );

		if ( !import_only )
			CFrameWork::inst ( )-> addToRecentFiles ( name );

		doc-> m_gui_state = gui_state;
		return doc;
	}
	else {
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Could not parse the XML data in file %1." ). arg ( CMB_BOLD( name )));
		return false;
	}
}

CDocument *CDocument::fileImportLDrawModel ( )
{
	QStringList filters;
	filters << tr( "LDraw Models" ) + " (*.dat;*.ldr;*.mpd)";
	filters << tr( "All Files" ) + "(*.*)";

	QString s = QFileDialog::getOpenFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Import File" ), 0 );

	if ( s. isEmpty ( ))
		return false;

	QFile f ( s );

	if ( !f. open ( IO_ReadOnly )) {
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Could not open file %1 for reading." ). arg ( CMB_BOLD( s )));
		return false;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

    uint invalid_items = 0;
	BrickLink::InvItemList items;

	bool b = BrickLink::inst ( )-> parseLDrawModel ( f, items, &invalid_items );

	QApplication::restoreOverrideCursor ( );

	if ( b && !items. isEmpty ( )) {
		CDocument *doc = new CDocument ( );

		if ( invalid_items )
			CMessageBox::information ( CFrameWork::inst ( ), tr( "This file contains %1 unknown item(s)." ). arg ( CMB_BOLD( QString::number ( invalid_items ))));

		doc-> setBrickLinkItems ( items );
		doc-> setTitle ( tr( "Import of %1" ). arg ( QFileInfo ( s ). fileName ( )));
		return doc;
	}
	else
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Could not parse the LDraw model in file %1." ). arg ( CMB_BOLD( s )));

	return 0;
}

void CDocument::setBrickLinkItems ( const BrickLink::InvItemList &bllist, uint multiply )
{
	ItemList items;
	ItemList positions;

	foreach ( const BrickLink::InvItem *blitem, bllist ) {
		Item *item = new Item ( *blitem );

		if ( item-> isIncomplete ( )) {
			DlgIncompleteItemImpl d ( item, CFrameWork::inst ( ));

//			if ( waitcursor )
//				QApplication::restoreOverrideCursor ( );

			bool drop_this = ( d. exec ( ) != QDialog::Accepted );

//			if ( waitcursor )
//				QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));
				
			if ( drop_this )
				continue;
		}

		item-> setQuantity ( item-> quantity ( ) * multiply );
		items. append ( item );
	}
	insertItemsDirect ( items, positions );

	// reset difference WITHOUT a command

	foreach ( Item *pos, m_items ) {
		if (( pos-> origQuantity ( ) != pos-> quantity ( )) ||
		    ( pos-> origPrice ( ) != pos-> price ( ))) 
		{
			pos-> setOrigQuantity ( pos-> quantity ( ));
			pos-> setOrigPrice ( pos-> price ( ));
		}
	}
}

QString CDocument::fileName ( ) const
{
	return m_filename;
}

void CDocument::setFileName ( const QString &str )
{
	m_filename = str;

	QFileInfo fi ( str );

	if ( fi. exists ( ))
		setTitle ( QDir::convertSeparators ( fi. absFilePath ( )));

	emit fileNameChanged ( m_filename );
}

QString CDocument::title ( ) const
{
	return m_title;
}

void CDocument::setTitle ( const QString &str )
{
	m_title = str;
	emit titleChanged ( m_title );
}

bool CDocument::isModified ( ) const
{
	return !m_undo-> isClean ( );
}

void CDocument::fileSave ( const ItemList &sorted )
{
	if ( fileName ( ). isEmpty ( ))
		fileSaveAs ( sorted );
	else if ( isModified ( ))
		fileSaveTo ( fileName ( ), "bsx", false, sorted );
}


void CDocument::fileSaveAs ( const ItemList &sorted )
{
	QStringList filters;
	filters << tr( "BrickStore XML Data" ) + " (*.bsx)";

	QString fn = fileName ( );

	if ( fn. isEmpty ( )) {
		QDir d ( CConfig::inst ( )-> documentDir ( ));

		if ( d. exists ( ))
			fn = d. filePath ( m_title );
	}
	if (( fn. right ( 4 ) == ".xml" ) || ( fn. right ( 4 ) == ".bti" ))
		fn. truncate ( fn.length ( ) - 4 );

	fn = QFileDialog::getSaveFileName ( fn, filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Save File as" ), 0 );

	if ( !fn. isNull ( )) {
		if ( fn. right ( 4 ) != ".bsx" )
			fn += ".bsx";

		if ( QFile::exists ( fn ) &&
		     CMessageBox::question ( CFrameWork::inst ( ), tr( "A file named %1 already exists. Are you sure you want to overwrite it?" ). arg( CMB_BOLD( fn )), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		     return;

		fileSaveTo ( fn, "bsx", false, sorted );
	}
}


bool CDocument::fileSaveTo ( const QString &s, const char *type, bool export_only, const ItemList &sorted )
{
	BrickLink::ItemListXMLHint hint;

	if ( qstrcmp ( type, "bsx" ) == 0 )
		hint = BrickLink::XMLHint_BrickStore;
	else if ( qstrcmp ( type, "bti" ) == 0 )
		hint = BrickLink::XMLHint_BrikTrak;
	else if ( qstrcmp ( type, "xml" ) == 0 )
		hint = BrickLink::XMLHint_MassUpload;
	else
		return false;


	QFile f ( s );
	if ( f. open ( IO_WriteOnly )) {
		QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

		const ItemList *itemlist = ( sorted. count ( ) == m_items. count ( )) ? &sorted : &m_items;

		QDomDocument doc (( hint == BrickLink::XMLHint_BrickStore ) ? QString( "BrickStoreXML" ) : QString::null );
		doc. appendChild ( doc. createProcessingInstruction ( "xml", "version=\"1.0\" encoding=\"UTF-8\"" ));

		QDomElement item_elem = BrickLink::inst ( )-> createItemListXML ( doc, hint, reinterpret_cast<const BrickLink::InvItemList *> ( itemlist ));

		if ( hint == BrickLink::XMLHint_BrickStore ) {
			QDomElement root = doc. createElement ( "BrickStoreXML" );

			root. appendChild ( item_elem );

			foreach ( IDocumentView *docview, m_views ) {
				QDomNode guinode = docview-> createGuiStateXML ( doc );
				if ( !guinode. isNull ( ))
					root. appendChild ( guinode );
			}

			doc. appendChild ( root );
		}
		else {
			doc. appendChild ( item_elem );
		}

		// directly writing to an QTextStream would be way more efficient,
		// but we could not handle any error this way :(
		QCString output = doc. toCString ( );
		bool ok = ( f. writeBlock ( output. data ( ), output. size ( ) - 1 ) ==  int( output. size ( ) - 1 )); // no 0-byte

		QApplication::restoreOverrideCursor ( );

		if ( ok ) {
			if ( !export_only ) {
				m_undo-> setClean ( );
				setFileName ( s );

				CFrameWork::inst ( )-> addToRecentFiles ( s );
			}
			return true;
		}
		else
			CMessageBox::warning ( CFrameWork::inst ( ), tr( "Failed to save data in file %1." ). arg ( CMB_BOLD( s )));
	}
	else
		CMessageBox::warning ( CFrameWork::inst ( ), tr( "Failed to open file %1 for writing." ). arg ( CMB_BOLD( s )));

	return false;
}

void CDocument::fileExportBrickLinkInvReqClipboard ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	const ItemList *itemlist = ( sorted. count ( ) == m_items. count ( )) ? &sorted : &m_items;

	QDomDocument doc ( QString::null );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_Inventory, reinterpret_cast<const BrickLink::InvItemList *> ( itemlist )));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryRequest ));
}

void CDocument::fileExportBrickLinkWantedListClipboard ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	QString wantedlist;

	if ( CMessageBox::getString ( CFrameWork::inst ( ), tr( "Enter the ID number of Wanted List (leave blank for the default Wanted List)" ), wantedlist )) {
		QMap <QString, QString> extra;
		extra. insert ( "WANTEDLISTID", wantedlist );

		const ItemList *itemlist = ( sorted. count ( ) == m_items. count ( )) ? &sorted : &m_items;

		QDomDocument doc ( QString::null );
		doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_WantedList, reinterpret_cast<const BrickLink::InvItemList *> ( itemlist )));

		QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

		if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_WantedListUpload ));
	}
}

void CDocument::fileExportBrickLinkXMLClipboard ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	const ItemList *itemlist = ( sorted. count ( ) == m_items. count ( )) ? &sorted : &m_items;

	QDomDocument doc ( QString::null );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_MassUpload, reinterpret_cast<const BrickLink::InvItemList *> ( itemlist )));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryUpload ));
}

void CDocument::fileExportBrickLinkUpdateClipboard ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	foreach ( const Item *item, m_items ) {
		if ( !item-> lotId ( )) {
			if ( CMessageBox::warning ( CFrameWork::inst ( ), tr( "This list contains items without a BrickLink Lot-ID.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
				return;
			else
				break;
		}
	}

	const ItemList *itemlist = ( sorted. count ( ) == m_items. count ( )) ? &sorted : &m_items;

	QDomDocument doc ( QString::null );
	doc. appendChild ( BrickLink::inst ( )-> createItemListXML ( doc, BrickLink::XMLHint_MassUpdate, reinterpret_cast<const BrickLink::InvItemList *> ( itemlist )));

	QApplication::clipboard ( )-> setText ( doc. toString ( ), QClipboard::Clipboard );

	if ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ))
			CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_InventoryUpdate ));
}

void CDocument::fileExportBrickLinkXML ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	QStringList filters;
	filters << tr( "BrickLink XML File" ) + " (*.xml)";

	QString s = QFileDialog::getSaveFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Export File" ), 0 );

	if ( !s. isNull ( )) {
		if ( s. right ( 4 ) != ".xml" )
			s += ".xml";

		if ( QFile::exists ( s ) &&
		     CMessageBox::question ( CFrameWork::inst ( ), tr( "A file named %1 already exists. Are you sure you want to overwrite it?" ). arg( CMB_BOLD( s )), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		     return;

		fileSaveTo ( s, "xml", true, sorted );
	}
}

void CDocument::fileExportBrikTrakInventory ( const ItemList &sorted )
{
	if ( !exportCheck ( ))
		return;

	QStringList filters;
	filters << tr( "BrikTrak Inventory" ) + " (*.bti)";

	QString s = QFileDialog::getSaveFileName ( CConfig::inst ( )-> documentDir ( ), filters. join ( ";;" ), CFrameWork::inst ( ), "FileDialog", tr( "Export File" ), 0 );

	if ( !s. isNull ( )) {
		if ( s. right ( 4 ) != ".bti" )
			s += ".bti";

		if ( QFile::exists ( s ) &&
		     CMessageBox::question ( CFrameWork::inst ( ), tr( "A file named %1 already exists. Are you sure you want to overwrite it?" ). arg( CMB_BOLD( s )), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes )
		     return;

		fileSaveTo ( s, "bti", true, sorted );
	}
}

bool CDocument::exportCheck ( ) const
{
	return !( statistics ( items ( )). errors ( ) && CMessageBox::warning ( CFrameWork::inst ( ), tr( "This list contains items with errors.<br /><br />Do you really want to export this list?" ), CMessageBox::Yes, CMessageBox::No ) != CMessageBox::Yes );
}

void CDocument::clean2Modified ( bool b )
{
	emit modificationChanged ( !b );
}

Q_UINT64 CDocument::errorMask ( ) const
{
	return m_error_mask;
}

void CDocument::setErrorMask ( Q_UINT64 em )
{
	m_error_mask = em;
	emit statisticsChanged ( );
	emit itemsChanged ( items ( ), false );
}

const BrickLink::Order *CDocument::order ( ) const
{
	return m_order;
}

void CDocument::resetDifferences ( const ItemList &items )
{
	CUndoCmd *macro = macroBegin ( tr( "Reset Differences" ));

	foreach ( Item *pos, items ) {
		if (( pos-> origQuantity ( ) != pos-> quantity ( )) ||
		    ( pos-> origPrice ( ) != pos-> price ( ))) 
		{
			Item item = *pos;

			item. setOrigQuantity ( item. quantity ( ));
			item. setOrigPrice ( item. price ( ));
			changeItem ( pos, item );
		}
	}
	macroEnd ( macro );
}

