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
#ifndef __CSELECTITEM_H__
#define __CSELECTITEM_H__

#include <qdialog.h>

#include "bricklink.h"

class CListView;
class CItemTypeCombo;

class QLabel;
class QComboBox;
class QToolButton;
class QPushButton;
class QPopupMenu;
class QIconView;
class QIconViewItem;
class QListViewItem;
class QWidgetStack;


class CSelectItem : public QWidget {
	Q_OBJECT
public:
	CSelectItem ( QWidget *parent, const char *name = 0, WFlags fl = 0 );

	bool setItemType ( const BrickLink::ItemType * );
	bool setItem ( const BrickLink::Item * );
	bool setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter );

	const BrickLink::Item *item ( ) const;

	void setOnlyWithInventory ( bool b );
	bool isOnlyWithInventory ( ) const;
	
	enum ViewMode { ViewMode_List, ViewMode_ListWithImages, ViewMode_Thumbnails };

	virtual QSize sizeHint ( ) const;

signals:
	void hasColors ( bool );
	void itemSelected ( const BrickLink::Item *, bool );

protected slots:
	void itemConfirmed ( );
	void itemChangedList ( );
	void itemChangedIcon ( );
	void itemTypeChanged ( );
	void categoryChanged ( );
	void applyFilter ( );
	void viewModeChanged ( int );
	void pictureUpdated ( BrickLink::Picture * );
	void findItem ( );
	void languageChange ( );

protected:
	virtual void showEvent ( QShowEvent * );

private:
	bool fillCategoryView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat = 0 );
	bool fillItemView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );

	const BrickLink::Item *fillItemListView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );
	const BrickLink::Item *fillItemIconView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );

	bool setViewMode ( ViewMode ivm, const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );
	ViewMode checkViewMode ( ViewMode ivm, const BrickLink::ItemType *itt, const BrickLink::Category *cat );

	void ensureSelectionVisible ( );

protected:
	CItemTypeCombo *m_type_combo;

	QLabel *w_item_types_label;
	QComboBox *w_item_types;
	CListView *w_categories;

	QWidgetStack *w_stack;
	CListView *w_items;
	QIconView *w_thumbs;

	QToolButton *w_goto;
	QLabel *w_filter_label;
	QToolButton *w_filter_clear;
	QComboBox *w_filter_expression;

	QToolButton *w_viewmode;
	QPopupMenu *w_viewpopup;

	ViewMode m_viewmode;
	bool m_filter_active;
	bool m_inv_only;
	const BrickLink::Item *m_selected;
};

class CSelectItemDialog : public QDialog {
	Q_OBJECT
public:
	explicit CSelectItemDialog ( bool only_with_inventory, QWidget *parent = 0, const char *name = 0, bool modal = true, WFlags fl = 0 );

	bool setItemType ( const BrickLink::ItemType * );
	bool setItem ( const BrickLink::Item * );
	bool setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter );

	const BrickLink::Item *item ( ) const;

	virtual int exec ( const QRect &pos = QRect ( ));

private slots:
	void checkItem ( const BrickLink::Item *, bool );
	
private:
	CSelectItem *w_si;

	QPushButton *w_ok;
	QPushButton *w_cancel;
};

#endif
