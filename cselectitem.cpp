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
#include <qcombobox.h>
#include <qpainter.h>
#include <qlistbox.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qheader.h>
#include <qtoolbutton.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qregexp.h>
#include <qpopupmenu.h>
#include <qiconview.h>
#include <qwidgetstack.h>
#include <qtooltip.h>

#include "citemtypecombo.h"
#include "cresource.h"
#include "clistview.h"
#include "cselectitem.h"
#include "cutility.h"
#include "cmessagebox.h"


namespace {

// this hack is needed since 0 means 'no selection at all'
static const BrickLink::Category *Cat_AllParts = reinterpret_cast <const BrickLink::Category *> ( 1 );


static QIconViewItem *QIconView__selectedItem ( QIconView *iv )
{
	QIconViewItem *ivi = 0;
	for ( ivi = iv-> firstItem ( ); ivi; ivi = ivi-> nextItem ( )) {
		if ( ivi-> isSelected ( ))
			break;
	}
	return ivi;
}


class CatListItem : public CListViewItem {
public:
	CatListItem ( CListView *lv, const BrickLink::Category *cat )
		: CListViewItem ( lv ), m_cat ( cat )
	{ }

	virtual QString text ( int /*col*/ ) const
	{ return ( m_cat != Cat_AllParts ) ? QString ( m_cat-> name ( )) : QString ( "[%1]" ). arg ( CSelectItem::tr( "All Items" )); }

	const BrickLink::Category *category ( ) const
	{ return m_cat; }

	virtual int compare ( QListViewItem * i, int /*col*/, bool /*ascending*/ ) const
	{
		if (( m_cat != Cat_AllParts ) && ( static_cast<CatListItem *> ( i )-> m_cat != Cat_AllParts ))
			return ::qstrcmp ( m_cat-> name ( ), static_cast <CatListItem *> ( i )-> m_cat-> name ( ));
		else
			return ( m_cat != Cat_AllParts ) ? +1 : -1;
	}

	virtual void paintCell ( QPainter *p, const QColorGroup &cg, int column, int width, int align )
	{
		if ( m_cat == Cat_AllParts ) {
			QFont f = p-> font ( );
			f. setBold ( !f. bold ( ));
			p-> setFont ( f );
		}

		CListViewItem::paintCell ( p, cg, column, width, align );
	}

private:
	const BrickLink::Category *m_cat;
};


class ItemListItem : public CListViewItem {
public:
	ItemListItem ( CListView *lv, const BrickLink::Item *item, const CSelectItem::ViewMode &viewmode, const bool &invonly )
		: CListViewItem ( lv ), m_item ( item ), m_viewmode ( viewmode ), m_invonly ( invonly ), m_picture ( 0 )
	{ }

	virtual ~ItemListItem ( )
	{ 
		if ( m_picture )
			m_picture-> release ( );
	}

	void pictureChanged ( )
	{
		if ( m_viewmode != CSelectItem::ViewMode_Thumbnails )
			repaint ( );
	}

	virtual QString text ( int col ) const
	{ 
		switch ( col ) {
			case  1: return m_item-> id ( );
			case  2: return m_item-> name ( );
			default: return QString::null;
		}
	}

	virtual const QPixmap *pixmap ( int col ) const
	{
		if (( col == 0 ) && ( m_viewmode == CSelectItem::ViewMode_ListWithImages )) {
			BrickLink::Picture *pic = picture ( );

			if ( pic && pic-> valid ( )) {
				static QPixmap val2ptr;
			
				val2ptr = pic-> pixmap ( );
				return &val2ptr;
			}
		}
		return 0;
	}

	BrickLink::Picture *picture ( ) const
	{ 
		if ( m_picture && (( m_picture-> item ( ) != m_item ) || ( m_picture-> color ( ) != m_item-> defaultColor ( )))) {
			m_picture-> release ( );
			m_picture = 0;
		}

		if ( !m_picture && m_item && m_item-> defaultColor ( )) {
			m_picture = BrickLink::inst ( )-> picture ( m_item, m_item-> defaultColor ( ), true );

			if ( m_picture )
				m_picture-> addRef ( );
		}
		return m_picture; 
	}


	void setup ( )
	{
		CListViewItem::setup ( );

		if ( m_viewmode == CSelectItem::ViewMode_ListWithImages ) {
			int h = QMAX( 30, listView ( )-> fontMetrics ( ). height ( )) + 2 * listView ( )-> itemMargin ( );

			if ( h & 1 )
				h++;

			setHeight ( h );
		}
	}

	virtual void paintCell ( QPainter *p, const QColorGroup &cg, int col, int w, int align )
	{
		if ( m_viewmode != CSelectItem::ViewMode_ListWithImages ) {
			if ( !isSelected ( ) && m_invonly && m_item && !m_item-> hasInventory ( )) {
				QColorGroup _cg ( cg );
				_cg. setColor ( QColorGroup::Text, CUtility::gradientColor ( cg. base ( ), cg. text ( ), 0.5f ));

				CListViewItem::paintCell ( p, _cg, col, w, align );
			}
			else
				CListViewItem::paintCell ( p, cg, col, w, align );
			return;
		}

		int h = height ( );
		int margin = listView ( )-> itemMargin ( );

		const QPixmap *pix = pixmap ( col );
		QString str = text ( col );

		QColor bg;
		QColor fg;

		if ( CListViewItem::isSelected ( )) {
			bg = cg. highlight ( );
			fg = cg. highlightedText ( );
		}
		else {
			bg = backgroundColor ( );
			fg = cg. text ( );
			
			if ( m_invonly && m_item && !m_item-> hasInventory ( ))
				fg = CUtility::gradientColor ( fg, bg, 0.5f );
		}

		p-> fillRect ( 0, 0, w, h, bg );
		p-> setPen ( fg );

		int x = 0, y = 0;

		if ( pix && !pix-> isNull ( )) {
			// clip the pixmap here .. this is cheaper than a cliprect

			int rw = w - 2 * margin;
			int rh = h - 2 * margin;

			int sw, sh;

			if ( pix-> height ( ) <= rh ) {
				sw = QMIN( rw, pix-> width ( ));
				sh = QMIN( rh, pix-> height ( ));
			}
			else {
				sw = pix-> width ( ) * rh / pix-> height ( );
				sh = rh;
			}

			if ( pix-> height ( ) <= rh )
				p-> drawPixmap ( x + margin, y + margin, *pix, 0, 0, sw, sh );
			else
				p-> drawPixmap ( QRect ( x + margin, y + margin, sw, sh ), *pix );

			x += ( sw + margin );
			w -= ( sw + margin );
		}
		if ( !str. isEmpty ( )) {
			int rw = w - 2 * margin;

			if ( !( align & AlignTop || align & AlignBottom ))
				align |= AlignVCenter;

			const QFontMetrics &fm = p-> fontMetrics ( );

			if ( fm. width ( str ) > rw )
				str = CUtility::ellipsisText ( str, fm, w, align );

			p-> drawText ( x + margin, y, rw, h, align, str );
		}
	}

	QString toolTip ( ) const
	{
		QString str = "<table><tr><td rowspan=\"2\">%1</td><td><b>%2</b></td></tr><tr><td>%3</td></tr></table>";
		QString left_cell;

		BrickLink::Picture *pic = picture ( );

		if ( pic && pic-> valid ( )) {
			QMimeSourceFactory::defaultFactory ( )-> setPixmap ( "add_item_tooltip_picture", pic-> pixmap ( ));
			
			left_cell = "<img src=\"add_item_tooltip_picture\" />";
		}
		else if ( pic && ( pic-> updateStatus ( ) == BrickLink::Updating )) {
			left_cell = "<i>" + CSelectItem::tr( "[Image is loading]" ) + "</i>";
		}

		return str. arg( left_cell ). arg( m_item-> id ( )). arg( m_item-> name ( ));
	}

	const BrickLink::Item *item ( ) const
	{ 
		return m_item; 
	}

private:
	const BrickLink::Item *       m_item;
	const CSelectItem::ViewMode & m_viewmode;
	const bool &                  m_invonly;
	mutable BrickLink::Picture *  m_picture;
};


class ItemListToolTip : public QObject, public QToolTip {
	Q_OBJECT
	
public:
	ItemListToolTip ( CListView *list )
		: QObject ( list-> viewport ( )), QToolTip ( list-> viewport ( ), new QToolTipGroup ( list-> viewport ( ))), 
		  m_list ( list ), m_tip_item ( 0 )
	{ 
		connect ( group ( ), SIGNAL( removeTip ( )), this, SLOT( tipHidden ( )));
		
		connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));
	}
	
	virtual ~ItemListToolTip ( )
	{ }

	void maybeTip ( const QPoint &pos )
	{
		if ( !parentWidget ( ) || !m_list )
			return;

		ItemListItem *item = static_cast <ItemListItem *> ( m_list-> itemAt ( pos ));
	
		if ( item ) {
			m_tip_item = item;
			tip ( m_list-> itemRect ( item ), item-> toolTip ( ));
		}
	}

private slots:
	void tipHidden ( )
	{
		m_tip_item = 0;
	}
	
	void pictureUpdated ( BrickLink::Picture *pic )
	{
		if ( m_tip_item && m_tip_item-> picture ( ) == pic )
			maybeTip ( parentWidget ( )-> mapFromGlobal ( QCursor::pos ( )));
	}

private:
	CListView *m_list;
	ItemListItem *m_tip_item;
};



class ItemIconItem : public QIconViewItem {
public:
	ItemIconItem ( QIconView *iv, const BrickLink::Item *item, const CSelectItem::ViewMode &viewmode, const bool &invonly )
		: QIconViewItem ( iv ), m_item ( item ), m_viewmode ( viewmode ), m_invonly ( invonly ), m_picture ( 0 )
	{ 
		setDragEnabled ( false );
		// QT-BUG: text() is declared virtual, but never called...
		setText ( item-> id ( ));
	}

	virtual ~ItemIconItem ( )
	{ 
		if ( m_picture )
			m_picture-> release ( );
	}

	void pictureChanged ( )
	{
		if ( m_viewmode == CSelectItem::ViewMode_Thumbnails ) {
			//calcRect ( );
			//iconView ( )-> arrangeItemsInGrid ( );
			repaint ( );
		}
	}

	virtual QPixmap *pixmap ( ) const
	{
		if ( m_viewmode == CSelectItem::ViewMode_Thumbnails ) {
			if ( m_picture && (( m_picture-> item ( ) != m_item ) || ( m_picture-> color ( ) != m_item-> defaultColor ( )))) {
				m_picture-> release ( );
				m_picture = 0;
			}

			if ( !m_picture && m_item && m_item-> defaultColor ( )) {
				m_picture = BrickLink::inst ( )-> picture ( m_item, m_item-> defaultColor ( ));

				if ( m_picture ) {
					m_picture-> addRef ( );
					const_cast<ItemIconItem *> ( this )-> calcRect ( ); // EVIL
				}
			}
			if ( m_picture && m_picture-> valid ( )) {
				static QPixmap val2ptr;
				
				val2ptr = m_picture-> pixmap ( );
				return &val2ptr;
			}

		}
		return const_cast <QPixmap *> ( BrickLink::inst ( )-> noImage ( m_item-> itemType ( )-> imageSize ( )));
	}

	virtual void paintItem ( QPainter *p, const QColorGroup &cg ) 
	{
		if ( m_viewmode == CSelectItem::ViewMode_Thumbnails ) {
			if ( !isSelected ( ) && m_invonly && m_item && !m_item-> hasInventory ( )) {
				QColorGroup _cg ( cg );
				_cg. setColor ( QColorGroup::Text, CUtility::gradientColor ( cg. base ( ), cg. text ( ), 0.5f ));

				QIconViewItem::paintItem ( p, _cg );
			}
			else
				QIconViewItem::paintItem ( p, cg );
		}
	}

	const BrickLink::Item *item ( ) const
	{ 
		return m_item; 
	}

private:
	const BrickLink::Item *      m_item;
	const CSelectItem::ViewMode &m_viewmode;
	const bool &                 m_invonly;
	mutable BrickLink::Picture * m_picture;
};



class ItemIconToolTip : public QToolTip {
public:
	ItemIconToolTip ( QIconView *iv )
		: QToolTip ( iv-> viewport ( )), m_iv ( iv )
	{ }
	
	virtual ~ItemIconToolTip ( ) 
	{ }

protected:
	virtual void maybeTip ( const QPoint &p )
	{
		ItemIconItem *i = static_cast <ItemIconItem *> ( m_iv-> findItem ( m_iv-> viewportToContents ( p )));

		if ( i && i-> item ( )) {
			QRect r = i-> rect ( );
			r. setTopLeft ( m_iv-> contentsToViewport ( r. topLeft ( )));

			tip ( r, i-> item ( )-> name ( ));
		}
	}

private:
		QIconView *m_iv;
};

} // namespace


CSelectItem::CSelectItem ( QWidget *parent, const char *name, WFlags fl )
	: QWidget ( parent, name, fl )
{
	m_inv_only = false;

	w_item_types_label = new QLabel ( this );
	w_item_types = new QComboBox ( false, this );
	setFocusProxy ( w_item_types );

	w_categories = new CListView ( this );
	w_categories-> setShowSortIndicator ( true );
	w_categories-> setAlwaysShowSelection ( true );
	w_categories-> header ( )-> setMovingEnabled ( false );
	w_categories-> header ( )-> setResizeEnabled ( false );
	w_categories-> addColumn ( QString ( ));
	w_categories-> setResizeMode ( QListView::LastColumn );

	w_goto = new QToolButton ( this );
	w_goto-> setAutoRaise ( true );
	w_goto-> setIconSet ( CResource::inst ( )-> iconSet ( "edit_find" ));

	w_filter_label = new QLabel ( this );
	w_filter_clear = new QToolButton ( this );
	w_filter_clear-> setAutoRaise ( true );
	w_filter_clear-> setIconSet ( CResource::inst ( )-> iconSet ( "filter_clear" ));

	w_filter_expression = new QComboBox ( true, this );

	w_viewpopup = new QPopupMenu ( this );
	w_viewpopup-> setCheckable ( true );
	w_viewpopup-> insertItem ( CResource::inst ( )-> iconSet ( "viewmode_list" ),   QString ( ), ViewMode_List );
	w_viewpopup-> insertItem ( CResource::inst ( )-> iconSet ( "viewmode_images" ), QString ( ), ViewMode_ListWithImages );
	w_viewpopup-> insertItem ( CResource::inst ( )-> iconSet ( "viewmode_thumbs" ), QString ( ), ViewMode_Thumbnails );
	w_viewpopup-> setItemChecked ( ViewMode_List, true );

	w_viewmode = new QToolButton ( this );
	w_viewmode-> setAutoRaise ( true );
	w_viewmode-> setPopupDelay ( 1 );
	w_viewmode-> setPopup ( w_viewpopup );
	w_viewmode-> setIconSet ( CResource::inst ( )-> iconSet ( "viewmode" ));

	w_stack = new QWidgetStack ( this );

	w_items = new CListView ( w_stack );
	w_stack-> addWidget ( w_items );
	w_items-> setShowSortIndicator ( true );
	w_items-> setAlwaysShowSelection ( true );
	w_items-> header ( )-> setMovingEnabled ( false );
	w_items-> header ( )-> setResizeEnabled ( false );

	w_items-> addColumn ( QString ( ));
	w_items-> addColumn ( QString ( ));
	w_items-> addColumn ( QString ( ));
	w_items-> setResizeMode ( QListView::LastColumn );
	w_items-> setSortColumn ( 2 );
	w_items-> setColumnWidthMode ( 0, QListView::Manual );
	w_items-> setColumnWidth ( 0, 0 );
	(void) new ItemListToolTip ( w_items );

	w_thumbs = new QIconView ( w_stack );
	w_stack-> addWidget ( w_thumbs );
	w_thumbs-> setResizeMode ( QIconView::Adjust );
	w_thumbs-> setItemsMovable ( false );
	w_thumbs-> setShowToolTips ( false );
	(void) new ItemIconToolTip ( w_thumbs );

	m_type_combo = new CItemTypeCombo ( w_item_types, m_inv_only );

	connect ( w_goto, SIGNAL( clicked ( )), this, SLOT( findItem ( )));
	connect ( w_filter_clear, SIGNAL( clicked ( )), w_filter_expression, SLOT( clearEdit ( )));
	connect ( w_filter_expression, SIGNAL( textChanged ( const QString & )), this, SLOT( applyFilter ( )));

	connect ( m_type_combo, SIGNAL( itemTypeActivated ( const BrickLink::ItemType * )), this, SLOT( itemTypeChanged ( )));
	connect ( w_categories, SIGNAL( selectionChanged ( )), this, SLOT( categoryChanged ( )));
	connect ( w_items, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedList ( )));
	connect ( w_items, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( itemConfirmed ( )));
	connect ( w_items, SIGNAL( returnPressed ( QListViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( w_thumbs, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedIcon ( )));
	connect ( w_thumbs, SIGNAL( doubleClicked ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( w_thumbs, SIGNAL( returnPressed ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( w_viewpopup, SIGNAL( activated ( int )), this, SLOT( viewModeChanged ( int )));

	QGridLayout *toplay = new QGridLayout ( this, 1, 1, 0, 6 );
	toplay-> setColStretch ( 0, 25 );
	toplay-> setColStretch ( 1, 75 );
	toplay-> setRowStretch ( 0, 0 );
	toplay-> setRowStretch ( 1, 100 );
	
	QBoxLayout *lay = new QHBoxLayout ( 6 );
	lay-> addWidget ( w_item_types_label, 0 );
	lay-> addWidget ( w_item_types, 1 );

	toplay-> addLayout ( lay, 0, 0 );
	toplay-> addWidget ( w_categories, 1, 0 );

	lay = new QHBoxLayout ( 6 );
	lay-> addWidget ( w_goto, 0 );
	lay-> addSpacing ( 6 );
	lay-> addWidget ( w_filter_clear, 0 );
	lay-> addWidget ( w_filter_label, 0 );
	lay-> addWidget ( w_filter_expression, 15 );
	lay-> addSpacing ( 6 );
	lay-> addWidget ( w_viewmode );

	toplay-> addLayout ( lay, 0, 1 );
	toplay-> addWidget ( w_stack, 1, 1 );
	
	m_filter_active = false;

	w_stack-> raiseWidget ( w_items );
	m_viewmode = ViewMode_List;

	m_selected = 0;

	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));

	languageChange ( );

//	activating the [All Items] category takes too long
//	w_categories-> setSelected ( w_categories-> firstChild ( ), true );
}

void CSelectItem::languageChange ( )
{
	w_item_types_label-> setText ( tr( "Item type:" ));
	w_filter_label-> setText ( tr( "Filter:" )); 

	w_goto-> setAccel ( tr( "Ctrl+F", "Find Item" ));
	QToolTip::add ( w_goto, tr( "Find Item..." ) + " (" + QString( w_goto-> accel ( )) + ")");

	QToolTip::add ( w_filter_expression, tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));
	QToolTip::add ( w_filter_clear, tr( "Reset an active filter" ));
	QToolTip::add ( w_viewmode, tr( "View" ));

	w_categories-> setColumnText ( 0, tr( "Category" ));
	w_categories-> firstChild ( )-> repaint ( );
	w_categories-> lastItem ( )-> repaint ( );

	w_items-> setColumnText ( 1, tr( "Part #" ));
	w_items-> setColumnText ( 2, tr( "Description" ));

	w_viewpopup-> changeItem ( ViewMode_List,           tr( "List" ));
	w_viewpopup-> changeItem ( ViewMode_ListWithImages, tr( "List with images" ));
	w_viewpopup-> changeItem ( ViewMode_Thumbnails,     tr( "Thumbnails" ));
}

void CSelectItem::setOnlyWithInventory ( bool b )
{
	if ( b != m_inv_only ) {
		m_type_combo-> reset ( b );
		m_inv_only = b;
	}
}

bool CSelectItem::isOnlyWithInventory ( ) const
{
	return m_inv_only;
}

void CSelectItem::pictureUpdated ( BrickLink::Picture *pic )
{
	if ( !pic )
		return;

	for ( QListViewItemIterator it ( w_items ); it. current ( ); ++it ) {
		ItemListItem *ii = static_cast <ItemListItem *> ( it. current ( ));

		if (( pic-> item ( ) == ii-> item ( )) && ( pic-> color ( ) == ii-> item ( )-> defaultColor ( )))
			ii-> pictureChanged ( );
	}
	for ( QIconViewItem *it = w_thumbs-> firstItem ( ); it; it = it-> nextItem ( )) {
		ItemIconItem *ii = static_cast <ItemIconItem *> ( it );

		if (( pic-> item ( ) == ii-> item ( )) && ( pic-> color ( ) == ii-> item ( )-> defaultColor ( )))
			ii-> pictureChanged ( );
	}
}


void CSelectItem::viewModeChanged ( int ivm )
{
	if ( ivm == m_viewmode )
		return;

	const BrickLink::ItemType *itt = m_type_combo-> currentItemType ( );
	CatListItem *cli = static_cast <CatListItem *> ( w_categories-> selectedItem ( ));
	const BrickLink::Category *cat = cli ? cli-> category ( ) : 0;

	if ( itt && cat )
		setViewMode ( checkViewMode ((ViewMode) ivm, itt, cat ), itt, cat );
}

void CSelectItem::findItem ( )
{
	const BrickLink::ItemType *itt = m_type_combo-> currentItemType ( );

	if ( !itt )
		return;

	QString str;

	if ( CMessageBox::getString ( this, tr( "Please enter the complete part number of the item:" ), str )) {
		const BrickLink::Item *item = BrickLink::inst ( )-> item ( itt-> id ( ), str. latin1 ( ));

		if ( item ) {
			setItem ( item );
			ensureSelectionVisible ( );
		}
		else
			QApplication::beep ( );
	}
}

bool CSelectItem::setItem ( const BrickLink::Item *item )
{
	if ( !item )
		return false;

	bool found = false;
	const BrickLink::ItemType *itt = item ? item-> itemType ( ) : 0;
	const BrickLink::Category *cat = item ? item-> category ( ) : 0;

	m_type_combo-> setCurrentItemType ( itt ? itt : BrickLink::inst ( )-> itemType ( 'P' ));
	if ( fillCategoryView ( itt, cat )) {
		found = fillItemView ( itt, cat, item );
	}
	return found;
}

bool CSelectItem::setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter )
{
	if ( setItemType ( itt )) {
		if ( !cat )
			cat = Cat_AllParts;

		if ( fillCategoryView ( itt, cat ))
			fillItemView ( itt, cat, 0 );

		w_filter_expression-> setEditText ( filter );
		applyFilter ( );
		return true;
	}
	return false;
}

bool CSelectItem::setItemType ( const BrickLink::ItemType *itt )
{
	if ( !itt )
		return false;

	m_type_combo-> setCurrentItemType ( itt ? itt : BrickLink::inst ( )-> itemType ( 'P' ));
	itemTypeChanged ( );
	return true;
}

// needed for a syncronous update:
// updateContents is protected and triggerUpdate is asyncronous
class HackListView : public CListView {
public:
	HackListView ( ) : CListView ( 0 ) { }
	void hackUpdateContents ( ) { updateContents ( ); }
};



void CSelectItem::itemTypeChanged ( )
{
	fillCategoryView ( m_type_combo-> currentItemType ( ));
}

void CSelectItem::categoryChanged ( )
{
	const BrickLink::ItemType *itt = m_type_combo-> currentItemType ( );
	CatListItem *cli = static_cast <CatListItem *> ( w_categories-> selectedItem ( ));
	const BrickLink::Category *cat = cli ? cli-> category ( ) : 0;

	fillItemView ( itt, cat );
}

bool CSelectItem::fillCategoryView ( const BrickLink::ItemType *itype, const BrickLink::Category *select )
{
	w_categories-> clearSelection ( );
	w_categories-> clear ( );

	if ( !itype ) {
		// SPERREN
		return false;
	}
	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CatListItem *cat;
	bool found = false;

	cat = new CatListItem ( w_categories, Cat_AllParts );
	if ( select == Cat_AllParts )
		cat-> setSelected ( found = true );

	for ( const BrickLink::Category **catp = itype-> categories ( ); *catp; catp++ ) {
		cat = new CatListItem ( w_categories, *catp );
		
		if ( *catp == select )
			w_categories-> setSelected ( cat, found = true );
	}

	w_categories-> sort ( );

	emit hasColors ( itype-> hasColors ( ));
	
	QApplication::restoreOverrideCursor ( );

	return select ? found : true;
}

bool CSelectItem::fillItemView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	w_thumbs-> clearSelection ( );
	w_items-> clearSelection ( );
	w_thumbs-> clear ( );
	w_items-> clear ( );
	w_items-> setColumnWidthMode ( 1, QListView::Manual );
	w_items-> setColumnWidth ( 1, 0 );
	w_items-> setColumnWidthMode ( 1, QListView::Maximum );

	bool found = false;

	if ( itt && cat )
		found = setViewMode ( checkViewMode ( m_viewmode, itt, cat ), itt, cat, select );

	return select ? found : true;
}

CSelectItem::ViewMode CSelectItem::checkViewMode ( ViewMode ivm, const BrickLink::ItemType *, const BrickLink::Category *cat )
{
	bool ok = true;

	if (( cat == Cat_AllParts ) && ( ivm != ViewMode_List ))
		ok = ( CMessageBox::question ( this, tr( "Viewing all items with images is a bandwidth- and memory-hungry operation.<br />Are you sure you want to continue?" ), QMessageBox::Yes, QMessageBox::No ) == QMessageBox::Yes );

	return ok ? ivm : ViewMode_List;
}

bool CSelectItem::setViewMode ( ViewMode ivm, const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	const BrickLink::Item *found = 0;
	bool same_vm = ( ivm == m_viewmode );
	ViewMode oldvm = m_viewmode;

	m_viewmode = ivm;

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	switch ( ivm ) {
		case ViewMode_Thumbnails:
			if ( w_thumbs-> count ( ) == 0 )
				found = fillItemIconView ( itt, cat, select );

			if ( !same_vm )
				w_stack-> raiseWidget ( w_thumbs );
			break;

		case ViewMode_ListWithImages:
		case ViewMode_List:
			w_items-> setColumnWidth ( 0, ( ivm == ViewMode_ListWithImages ) ? 40 + 2 * w_items-> itemMargin ( ) : 0 );
			w_items-> header ( )-> adjustHeaderSize ( );

			if ( w_items-> childCount ( ) == 0 )
				found = fillItemListView ( itt, cat, select );

			if ( !same_vm ) {
				if ( oldvm != ViewMode_Thumbnails ) {
					for ( QListViewItemIterator it ( w_items ); it. current ( ); ++it )
						it. current ( )-> setup ( );
				}
				w_stack-> raiseWidget ( w_items );
			}
			break;
	}
	w_filter_expression-> setEnabled ( ivm != ViewMode_Thumbnails );
	w_filter_clear-> setEnabled ( ivm != ViewMode_Thumbnails );

	applyFilter ( );

	QApplication::restoreOverrideCursor ( );

	if ( !same_vm ) {
		w_viewpopup-> setItemChecked ( ViewMode_List, false );
		w_viewpopup-> setItemChecked ( ViewMode_ListWithImages, false );
		w_viewpopup-> setItemChecked ( ViewMode_Thumbnails, false );
		w_viewpopup-> setItemChecked ( ivm, true );
	}
	return select ? ( found != 0 ) : true;
}

void CSelectItem::showEvent ( QShowEvent * )
{
	ensureSelectionVisible ( );
}

void CSelectItem::ensureSelectionVisible ( )
{
	w_categories-> centerItem ( w_categories-> selectedItem ( ));

	if ( m_viewmode == ViewMode_Thumbnails ) {
		QIconViewItem *ivi = QIconView__selectedItem ( w_thumbs );

		if ( ivi ) {
			QPoint p = ivi-> rect ( ). center ( );
			w_thumbs-> center ( p. x ( ), p. y ( ), 1.f, 1.f );
		}
	}
	else
		w_items-> centerItem ( w_items-> selectedItem ( ));
}

const BrickLink::Item *CSelectItem::fillItemIconView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	const BrickLink::Item *found = 0;
	const BrickLink::Item * const *itemp = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	CDisableUpdates disupd ( this );

	for ( uint i = 0; i < count; i++, itemp++ ) {
		const BrickLink::Item *item = *itemp;

		if (( item-> itemType ( ) == itt ) && (( cat == Cat_AllParts ) || ( item-> hasCategory ( cat )))) {
			ItemIconItem *iii = new ItemIconItem ( w_thumbs, item, m_viewmode, m_inv_only );
			if ( item == select ) {
				found = item;
				w_thumbs-> setSelected ( iii, true );
			}
		}
	}
	w_thumbs-> arrangeItemsInGrid ( );

	return found;
}

const BrickLink::Item *CSelectItem::fillItemListView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	const BrickLink::Item *found = 0;
	const BrickLink::Item * const *itemp = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	CDisableUpdates disupd ( w_items );

	for ( uint i = 0; i < count; i++, itemp++ ) {
		const BrickLink::Item *item = *itemp;

		if (( item-> itemType ( ) == itt ) && (( cat == Cat_AllParts ) || ( item-> hasCategory ( cat )))) {
			ItemListItem *ili = new ItemListItem ( w_items, item, m_viewmode, m_inv_only );
			if ( item == select ) {
				found = item;
				w_items-> setSelected ( ili, true );
			}
		}
	}
	disupd. reenable ( );
	static_cast <HackListView *> ( w_items )-> hackUpdateContents ( );

	return found;
}


void CSelectItem::applyFilter ( )
{
	if ( m_viewmode == ViewMode_Thumbnails )
		return;

	QRegExp regexp ( w_filter_expression-> lineEdit ( )-> text ( ), false, true );

	bool regexp_ok = !regexp. isEmpty ( ) && regexp. isValid ( );

	if ( !regexp_ok && !m_filter_active ) {
		//qDebug ( "ignoring filter...." );
		return;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CDisableUpdates disupd ( w_items );

	for ( QListViewItemIterator it ( w_items ); it. current ( ); ++it ) {
		QListViewItem *ivi = it. current ( );

		bool b = !regexp_ok || ( regexp. search ( ivi-> text ( 1 )) >= 0 ) || ( regexp. search ( ivi-> text ( 2 )) >= 0 );

		if ( !b && ivi-> isSelected ( ))
			w_items-> setSelected ( ivi, false );

		ivi-> setVisible ( b );
	}
	m_filter_active = regexp_ok;

	QApplication::restoreOverrideCursor ( );
}

const BrickLink::Item *CSelectItem::item ( ) const
{
	return m_selected;
}

void CSelectItem::itemChangedList ( )
{
	QListViewItem *lvi = w_items-> selectedItem ( );
	const BrickLink::Item *newitem = lvi ? static_cast <ItemListItem *> ( lvi )-> item ( ) : 0;

	if ( lvi )
		w_items-> ensureItemVisible ( lvi );

	if ( newitem != m_selected ) {
		m_selected = newitem;
		emit itemSelected ( m_selected, false );
	}
}

void CSelectItem::itemChangedIcon ( )
{
	QIconViewItem *ivi = QIconView__selectedItem ( w_thumbs );
	const BrickLink::Item *newitem = ivi ? static_cast <ItemIconItem *> ( ivi )-> item ( ) : 0;

	if ( ivi )
		w_thumbs-> ensureItemVisible ( ivi );

	if ( newitem != m_selected ) {
		m_selected = newitem;
		emit itemSelected ( m_selected, false );
	}
}

void CSelectItem::itemConfirmed ( )
{
	if ( m_selected )
		emit itemSelected ( m_selected, true );
}

QSize CSelectItem::sizeHint ( ) const
{
	QFontMetrics fm = fontMetrics ( );

	return QSize ( 120 * fm. width ( 'x' ), 20 * fm. height ( ));
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

CSelectItemDialog::CSelectItemDialog ( bool only_with_inventory, QWidget *parent, const char *name, bool modal, WFlags fl )
	: QDialog ( parent, name, modal, fl )
{
	w_si = new CSelectItem ( this );
	w_si-> setOnlyWithInventory ( only_with_inventory );

	w_ok = new QPushButton ( tr( "&OK" ), this );
	w_ok-> setAutoDefault ( true );
	w_ok-> setDefault ( true );

	w_cancel = new QPushButton ( tr( "&Cancel" ), this );
	w_cancel-> setAutoDefault ( true );

	QFrame *hline = new QFrame ( this );
	hline-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	
	QBoxLayout *toplay = new QVBoxLayout ( this, 11, 6 );
	toplay-> addWidget ( w_si );
	toplay-> addWidget ( hline );

	QBoxLayout *butlay = new QHBoxLayout ( toplay );
	butlay-> addStretch ( 60 );
	butlay-> addWidget ( w_ok, 15 );
	butlay-> addWidget ( w_cancel, 15 );

	setSizeGripEnabled ( true );
	setMinimumSize ( minimumSizeHint ( ));

	connect ( w_ok, SIGNAL( clicked ( )), this, SLOT( accept ( )));
	connect ( w_cancel, SIGNAL( clicked ( )), this, SLOT( reject ( )));
	connect ( w_si, SIGNAL( itemSelected ( const BrickLink::Item *, bool )), this, SLOT( checkItem ( const BrickLink::Item *, bool )));

	w_ok-> setEnabled ( false );
}

bool CSelectItemDialog::setItemType ( const BrickLink::ItemType *itt )
{
	return w_si-> setItemType ( itt );
}

bool CSelectItemDialog::setItem ( const BrickLink::Item *item )
{
	return w_si-> setItem ( item );
}

bool CSelectItemDialog::setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter )
{
	return w_si-> setItemTypeCategoryAndFilter ( itt, cat, filter );
}

const BrickLink::Item *CSelectItemDialog::item ( ) const
{
	return w_si-> item ( );
}

void CSelectItemDialog::checkItem ( const BrickLink::Item *it, bool ok )
{
	bool b = w_si-> isOnlyWithInventory ( ) ? it-> hasInventory ( ) : true;

	w_ok-> setEnabled (( it ) && b );

	if ( it && b && ok )
		w_ok-> animateClick ( );
}

int CSelectItemDialog::exec ( const QRect &pos )
{
	if ( pos. isValid ( ))
		CUtility::setPopupPos ( this, pos );

	return QDialog::exec ( );
}

#include "cselectitem.moc"
