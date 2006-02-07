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
#ifndef __CWINDOW_H__
#define __CWINDOW_H__

#include <qwidget.h>
#include <qregexp.h>
#include <qptrlist.h>
#include <qptrdict.h>
#include <qguardedptr.h>
#include <qdom.h>

#include "citemview.h"
#include "bricklink.h"
#include "cconfig.h"
#include "cmoney.h"

class QToolButton;
class QComboBox;
class QListViewItem;
class CItemView;
class DlgAddItemImpl;
class CFrameWork;
class CUndoStack;
class QLabel;
class CDocument;


class CWindow : public QWidget {
	Q_OBJECT
public:
	CWindow ( CDocument *doc, QWidget *parent = 0, const char *name = 0 );
	~CWindow ( );

	CDocument *document ( ) { return m_doc; }

	enum MergeFlags {
		MergeAction_None         = 0x00000000,
		MergeAction_Force        = 0x00000001,
		MergeAction_Ask          = 0x00000002,
		MergeAction_Mask         = 0x0000ffff,

		MergeKeep_Old            = 0x00000000,
		MergeKeep_New            = 0x00010000,
		MergeKeep_Mask           = 0xffff0000
	};

	CDocument::ItemList sortedItems ( );

	uint setItems ( const BrickLink::InvItemList &items, int multiply = 1 );
	uint addItems ( const BrickLink::InvItemList &items, int multiply = 1, uint mergeflags = MergeAction_None, bool dont_change_sorting = false );
	void deleteItems ( const BrickLink::InvItemList &items );

	void mergeItems ( const CDocument::ItemList &items, int globalmergeflags = MergeAction_Ask );

	void subtractItems ( const BrickLink::InvItemList &items );

//	bool hasFilter ( ) const;
//	void setFilter ( const QRegExp &exp, Field f );
//	void setFilterExpression ( const QRegExp &exp );
//	void setFilterField ( Field f );
//	QRegExp filterExpression ( ) const;
//	Field filterField ( ) const;

//	InvItemList &selectedItems ( );
//	void setSelectedItems ( const InvItemList &items );

	bool isDifferenceMode ( ) const;

public slots:
	void setDifferenceMode ( bool );

	void fileSave ( );
	void fileSaveAs ( );
	void fileExportBrickLinkXML ( );
	void fileExportBrickLinkXMLClipboard ( );
	void fileExportBrickLinkUpdateClipboard ( );
	void fileExportBrickLinkInvReqClipboard ( );
	void fileExportBrickLinkWantedListClipboard ( );
	void fileExportBrikTrakInventory ( );

	void filePrint ( );

	void editCut ( );
	void editCopy ( );
	void editPaste ( );
	void editDelete ( );

	void editAddItems ( );
	void editSubtractItems ( );
	void editMergeItems ( );
	void editPartOutItems ( );

	void editResetDifferences ( );

	void selectAll ( );
	void selectNone ( );

	void editSetPrice ( );
	void editSetPriceToPG ( );
	void editPriceIncDec ( );
	void editSetSale ( );
	void editSetRemark ( );
	void editSetReserved ( );
	void editSetCondition ( );
	void editMultiplyQty ( );
	void editDivideQty ( );

	void addItem ( BrickLink::InvItem *, uint );

	void setPrice ( money_t );

	void showBLCatalog ( );
	void showBLPriceGuide ( );
	void showBLLotsForSale ( );

signals:
	void selectionChanged ( const BrickLink::InvItemList & );
	void statisticsChanged ( );

protected:
	void closeEvent ( QCloseEvent *e );

protected slots:
	void languageChange ( );

private slots:
	void applyFilter ( );
	void updateSelectionFromView ( );
	void updateSelectionFromDoc ( const CDocument::ItemList &itlist );
	void updateCaption ( );

	void contextMenu ( QListViewItem *, const QPoint & );
	void priceGuideUpdated ( BrickLink::PriceGuide * );
	void pictureUpdated ( BrickLink::Picture * );
	void itemModified ( CItemViewItem *item, bool grave );
	void updateErrorMask ( );

	void resetDifferences ( const CDocument::ItemList & );

	QDomElement createGuiStateXML ( QDomDocument doc );
	bool parseGuiStateXML ( QDomElement root );

	void itemsAddedToDocument ( const CDocument::ItemList & );
	void itemsRemovedFromDocument ( const CDocument::ItemList & );
	void itemsChangedInDocument ( const CDocument::ItemList &, bool );

private:
	CDocument * m_doc;
	QRegExp        m_filter_expression;
	int            m_filter_field;

	bool m_ignore_selection_update;

	QPtrDict<CItemViewItem>  m_lvitems;

	QToolButton *  w_filter_clear;
	QComboBox *    w_filter_expression;
	QComboBox *    w_filter_field;
	CItemView *	   w_list;
	QLabel *       w_filter_label;
	QLabel *       w_filter_field_label;

	QGuardedPtr <DlgAddItemImpl> m_add_dialog;

	uint                           m_settopg_failcnt;
	QPtrDict<BrickLink::InvItem> * m_settopg_list;
	BrickLink::PriceGuide::Time    m_settopg_time;
	BrickLink::PriceGuide::Price   m_settopg_price;
};

#endif
