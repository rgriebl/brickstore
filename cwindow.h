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
#include <qlistview.h>
#include <qguardedptr.h>

#include "citemview.h"
#include "bricklink.h"
#include "cconfig.h"
#include "cmoney.h"

class QToolButton;
class QComboBox;
class CItemView;
class DlgAddItemImpl;
class CFrameWork;

class CWindow : public QWidget {
	Q_OBJECT
public:
	CWindow ( QWidget *parent = 0, const char *name = 0 );
	~CWindow ( );

	bool isModified ( ) const;
	void setModified ( bool b );

	QString fileName ( ) const;
	void setFileName ( const QString &str );

	enum MergeFlags {
		MergeAction_None         = 0x00000000,
		MergeAction_Force        = 0x00000001,
		MergeAction_Ask          = 0x00000002,
		MergeAction_Mask         = 0x0000ffff,

		MergeKeep_Old            = 0x00000000,
		MergeKeep_New            = 0x00010000,
		MergeKeep_Mask           = 0xffff0000
	};

	const QPtrList<BrickLink::InvItem> &items ( ) const;
	QPtrList <BrickLink::InvItem> sortedItems ( );

	uint setItems ( const QPtrList<BrickLink::InvItem> &items, int multiply = 1 );
	uint addItems ( const QPtrList<BrickLink::InvItem> &items, int multiply = 1, uint mergeflags = MergeAction_None, bool dont_change_sorting = false );
	void deleteItems ( const QPtrList<BrickLink::InvItem> &items );

	void mergeItems ( const QPtrList<BrickLink::InvItem> &items, int globalmergeflags = MergeAction_Ask );

	void subtractItems ( const QPtrList <BrickLink::InvItem> &items );

	void calculateStatistics ( const QPtrList <BrickLink::InvItem> &list, int &lots, int &items, money_t &val, money_t &minval, double &weight, int &errors );

	enum ViewMode {
		ViewPlain,
		ViewHierarchic,
		ViewCompact
	};

//	void setViewMode ( ViewMode m );
//	ViewMode viewMode ( ) const;

//	bool hasFilter ( ) const;
//	void setFilter ( const QRegExp &exp, Field f );
//	void setFilterExpression ( const QRegExp &exp );
//	void setFilterField ( Field f );
//	QRegExp filterExpression ( ) const;
//	Field filterField ( ) const;

//	QPtrList<BrickLink::InvItem> &selectedItems ( );
//	void setSelectedItems ( const QPtrList<BrickLink::InvItem> &items );

	bool isDifferenceMode ( ) const;

public:
	bool fileOpen ( );
	bool fileOpen ( const QString &name );
	bool fileImportBrickLinkInventory ( const BrickLink::Item *preselect = 0 );
	bool fileImportBrickLinkOrder ( );
	bool fileImportBrickLinkStore ( );
	bool fileImportBrickLinkXML ( );
	bool fileImportLDrawModel ( );

public slots:
	void setDifferenceMode ( bool );

	void fileSave ( );
	void fileSaveAs ( );
	void fileExportBrickLinkXML ( );
	void fileExportBrickLinkXMLClipboard ( );
	void fileExportBrickLinkUpdateClipboard ( );
	void fileExportBrickLinkInvReqClipboard ( );
	void fileExportBrickLinkWantedListClipboard ( );

	void filePrint ( );

	void editUndo ( );
	void editRedo ( );

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

	void viewPlain ( bool );
	void viewHierarchic ( bool );
	void viewCompact ( bool );

	void editSetPrice ( );
	void editSetPriceToPG ( );
	void editPriceIncDec ( );
	void editSetSale ( );
	void editSetRemark ( );
	void editSetReserved ( );
	void editSetCondition ( );
	void editMultiplyQty ( );
	void editDivideQty ( );

	void triggerUpdate ( );
	void triggerSelectionUpdate ( );
	void triggerStatisticsUpdate ( );

	void addItem ( const BrickLink::InvItem *, uint );

	void setPrice ( money_t );

	void showBLCatalog ( );
	void showBLPriceGuide ( );
	void showBLLotsForSale ( );

signals:
	void selectionChanged ( const QPtrList<BrickLink::InvItem> & );
	void modificationChanged ( bool m );
	void statisticsChanged ( const QPtrList<BrickLink::InvItem> & );

protected:
	void closeEvent ( QCloseEvent *e );

private slots:
	void applyFilter ( );
	void updateSelection ( );
	void updateCaption ( );
	void updateStatistics ( );
	void contextMenu ( QListViewItem *, const QPoint & );
	void inventoryUpdated ( BrickLink::Inventory * );
	void priceGuideUpdated ( BrickLink::PriceGuide * );
	void pictureUpdated ( BrickLink::Picture * );
	void itemModified ( CItemViewItem *item, bool grave );
	void updateErrorMask ( );

private:
	bool fileLoadFrom ( const QString &s, char type, bool import_only = false );
	bool fileSaveTo ( const QString &s, char type, bool export_only = false );

	void resetDifferences ( const QPtrList<BrickLink::InvItem> & );

private:
	QRegExp        m_filter_expression;
	int            m_filter_field;

	ViewMode       m_viewmode;

	QString        m_filename;
	QString        m_caption;

	BrickLink::Inventory *m_inventory;
	int                   m_inventory_multiply;

	QPtrList<BrickLink::InvItem>  m_items;
	QPtrList<BrickLink::InvItem>  m_selection;

	QPtrDict<CItemViewItem>  m_lvitems;

	QToolButton *  w_filter_clear;
	QComboBox *    w_filter_expression;
	QComboBox *    w_filter_field;
	CItemView *	   w_list;

	QGuardedPtr <DlgAddItemImpl> m_add_dialog;

	bool           m_modified;

	uint                           m_settopg_failcnt;
	QPtrDict<BrickLink::InvItem> * m_settopg_list;
	BrickLink::PriceGuide::Time    m_settopg_time;
	BrickLink::PriceGuide::Price   m_settopg_price;
};

#endif
