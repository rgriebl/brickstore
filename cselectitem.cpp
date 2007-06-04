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

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QComboBox>
#include <QButtonGroup>
#include <qlineedit.h>
#include <qlayout.h>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <qapplication.h>
#include <qcursor.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <QTableView>
#include <QListView>
#include <QStackedLayout>
#include <QMenu>

//#include "citemtypecombo.h"
//#include "cresource.h"
//#include "clistview.h"
#include "cselectitem.h"
#include "cutility.h"
#include "cmessagebox.h"


namespace {

// this hack is needed since 0 means 'no selection at all'
static const BrickLink::Category *Cat_AllParts = reinterpret_cast <const BrickLink::Category *> ( 1 );

class CategoryModel : public QAbstractListModel {
public:
	CategoryModel ( )
	{
		m_categories = BrickLink::inst ( )-> categories ( ). values ( );
	}

	virtual int rowCount ( const QModelIndex & /*parent*/ ) const
	{
		return m_categories. count ( ) + 1;
	}

	virtual QVariant data ( const QModelIndex &index, int role ) const
	{
		QVariant res;
		const BrickLink::Category *category = index. row ( ) ? m_categories [index. row ( ) - 1] : 0;
		int col = index. column ( );

		if ( col == 0 ) {
			if ( role == Qt:: DisplayRole ) {
				res = category ? category-> name ( ) : QString ( "[%1]" ). arg ( CSelectItem::tr( "All Items" ));
			}
			else if ( role == Qt::FontRole ) {
				if ( !category ) {
					QFont f = qApp-> font ( );
					f. setBold ( !f. bold ( ));
					res = f;
				}
			}
		}
		
		return res;
	}

	virtual QVariant headerData ( int section, Qt::Orientation orient, int role ) const
    {
		QVariant res;

		if ( orient == Qt::Horizontal ) {
			if ( role == Qt::DisplayRole ) {
				switch ( section ) {
					case  0: res = CSelectItem::tr( "Category" ); break;
				}
			}
		}
		return res;
    }
private:
	QList<const BrickLink::Category *> m_categories;
};

class ItemModel : public QAbstractTableModel {
public:
	ItemModel ( )
	{
		m_items = BrickLink::inst ( )-> items ( );
	}

	virtual int rowCount ( const QModelIndex & /*parent*/ ) const
	{
		return m_items. count ( );
	}

	virtual int columnCount ( const QModelIndex & /*parent*/ ) const
	{
		return 3;
	}

	virtual QVariant data ( const QModelIndex &index, int role ) const
	{
		QVariant res;
		const BrickLink::Item *item = m_items [index. row ( )];
		int col = index. column ( );

		if ( col == 0 ) {
			if ( role == Qt:: DisplayRole ) {
				res = item-> name ( );
			}
		}
		
		return res;
	}

	virtual QVariant headerData ( int section, Qt::Orientation orient, int role ) const
    {
		QVariant res;

		if ( orient == Qt::Horizontal ) {
			if ( role == Qt::DisplayRole ) {
				switch ( section ) {
					case  0: res = CSelectItem::tr( "Name" ); break;
				}
			}
		}
		return res;
    }

private:
	QVector<const BrickLink::Item *> m_items;
};


#if 0
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

	void hideTip ( )
	{
		QToolTip::hide ( );
		m_tip_item = 0; // tipHidden() doesn't get called immediatly
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
#endif

} // namespace


class CSelectItemPrivate {
public:
//	CItemTypeCombo *m_type_combo;

	QLabel *        w_item_types_label;
	QComboBox *     w_item_types;
	QListView *     w_categories;

	QStackedLayout *m_stack;
	QTableView *    w_items;
	QListView *     w_thumbs;
	
	QToolButton *   w_goto;
	QLabel *        w_filter_label;
	QToolButton *   w_filter_clear;
	QComboBox *     w_filter_expression;
	QToolButton *   w_view_list;
	QToolButton *   w_view_images;
	QToolButton *   w_view_thumbs;

	CSelectItem::ViewMode  m_viewmode;
	bool                   m_filter_active;
	bool                   m_inv_only;
	const BrickLink::Item *m_selected;
//	ItemListToolTip *      m_items_tip;
};


CSelectItem::CSelectItem ( QWidget *parent, Qt::WindowFlags f )
	: QWidget ( parent, f )
{
	d = new CSelectItemPrivate ( );

	d-> m_inv_only = false;

	QGridLayout *lay = new QGridLayout ( this );
	lay-> setColumnStretch ( 0, 25 );
	lay-> setColumnStretch ( 1, 75 );
	lay-> setRowStretch ( 0, 0 );
	lay-> setRowStretch ( 1, 100 );

	QHBoxLayout *typelay = new QHBoxLayout ( );

	d-> w_item_types_label = new QLabel ( this );
	d-> w_item_types = new QComboBox ( this );
	d-> w_item_types-> setEditable ( false );

	typelay-> addWidget ( d-> w_item_types_label, 0 );
	typelay-> addWidget ( d-> w_item_types, 1 );
	
	lay-> addLayout ( typelay, 0, 0 );

	d-> w_categories = new QListView ( this );
//	d-> w_categories-> setShowSortIndicator ( true );
//	d-> w_categories-> setAlwaysShowSelection ( true );
//	d-> w_categories-> header ( )-> setMovingEnabled ( false );
//	d-> w_categories-> header ( )-> setResizeEnabled ( false );
//	d-> w_categories-> addColumn ( QString ( ));
//	d-> w_categories-> setResizeMode ( QListView::LastColumn );

	lay-> addWidget ( d-> w_categories, 1, 0 );

	QHBoxLayout *toollay = new QHBoxLayout ( );

	d-> w_goto = new QToolButton ( this );
	d-> w_goto-> setAutoRaise ( true );
	d-> w_goto-> setIcon ( QIcon ( ":/images/edit_find" ));
	connect ( d-> w_goto, SIGNAL( clicked ( )), this, SLOT( findItem ( )));

	toollay-> addWidget ( d-> w_goto, 0 );
	toollay-> addSpacing ( 6 );

	d-> w_filter_clear = new QToolButton ( this );
	d-> w_filter_clear-> setAutoRaise ( true );
	d-> w_filter_clear-> setIcon ( QIcon ( ":/images/filter_clear" ));

	d-> w_filter_label = new QLabel ( this );

	d-> w_filter_expression = new QComboBox ( this );
	d-> w_filter_expression-> setEditable ( true );
	connect ( d-> w_filter_clear, SIGNAL( clicked ( )), d-> w_filter_expression, SLOT( clearEdit ( )));
	connect ( d-> w_filter_expression, SIGNAL( textChanged ( const QString & )), this, SLOT( applyFilter ( )));

	toollay-> addWidget ( d-> w_filter_clear, 0 );
	toollay-> addWidget ( d-> w_filter_label, 0 );
	toollay-> addWidget ( d-> w_filter_expression, 15 );
	toollay-> addSpacing ( 6 );

	QHBoxLayout *viewlay = new QHBoxLayout ( );
	viewlay-> setMargin ( 0 );
	viewlay-> setSpacing ( 0 );
	
	d-> w_view_list = new QToolButton ( this );
	d-> w_view_list-> setAutoRaise ( true );
	d-> w_view_list-> setCheckable ( true );
	d-> w_view_list-> setIcon ( QIcon ( ":/images/viewmode_list" ));
	viewlay-> addWidget ( d-> w_view_list );
	
	d-> w_view_images = new QToolButton ( this );
	d-> w_view_images-> setAutoRaise ( true );
	d-> w_view_images-> setCheckable ( true );
	d-> w_view_images-> setIcon ( QIcon ( ":/images/viewmode_list" ));
	connect ( d-> w_view_images, SIGNAL( clicked ( )), this, SLOT( setViewModeListWithImages ( )));
	viewlay-> addWidget ( d-> w_view_images );

	d-> w_view_thumbs = new QToolButton ( this );
	d-> w_view_thumbs-> setAutoRaise ( true );
	d-> w_view_thumbs-> setCheckable ( true );
	d-> w_view_thumbs-> setIcon ( QIcon ( ":/images/viewmode_list" ));
	viewlay-> addWidget ( d-> w_view_thumbs );

	QButtonGroup *bg = new QButtonGroup ( this );
	bg-> addButton ( d-> w_view_list,   ViewMode_List );
	bg-> addButton ( d-> w_view_images, ViewMode_ListWithImages );
	bg-> addButton ( d-> w_view_thumbs, ViewMode_Thumbnails );
	connect ( bg, SIGNAL( buttonClicked ( int )), this, SLOT( viewModeChanged ( int )));

	toollay-> addLayout ( viewlay );
	lay-> addLayout ( toollay, 0, 1 );

	d-> m_stack = new QStackedLayout ( );
	
	d-> w_items = new QTableView ( this );
	d-> m_stack-> addWidget ( d-> w_items );

//	d-> w_items-> setShowSortIndicator ( true );
//	d-> w_items-> setAlwaysShowSelection ( true );
//	d-> w_items-> header ( )-> setMovingEnabled ( false );
//	d-> w_items-> header ( )-> setResizeEnabled ( false );

//	d-> w_items-> addColumn ( QString ( ));
//	d-> w_items-> addColumn ( QString ( ));
//	d-> w_items-> addColumn ( QString ( ));
//	d-> w_items-> setResizeMode ( QListView::LastColumn );
//	d-> w_items-> setSortColumn ( 2 );
//	d-> w_items-> setColumnWidthMode ( 0, QListView::Manual );
//	d-> w_items-> setColumnWidth ( 0, 0 );
//	d-> m_items_tip = new ItemListToolTip ( d-> w_items );

	d-> w_thumbs = new QListView ( this );
	d-> m_stack-> addWidget ( d-> w_thumbs );
//	d-> w_thumbs-> setResizeMode ( QIconView::Adjust );
//	d-> w_thumbs-> setItemsMovable ( false );
//	d-> w_thumbs-> setShowToolTips ( false );
//	(void) new ItemIconToolTip ( d-> w_thumbs );

	lay-> addLayout ( d-> m_stack, 1, 1 );

//	d-> m_type_combo = new CItemTypeCombo ( d-> w_item_types, d-> m_inv_only );
//	w_item_types-> setModel ( new ItemTypeModel ( d-> m_inv_only ));


//	connect ( d-> m_type_combo, SIGNAL( itemTypeActivated ( const BrickLink::ItemType * )), this, SLOT( itemTypeChanged ( )));
	connect ( d-> w_item_types, SIGNAL( currentIndexChanged ( )), this, SLOT( itemTypeChanged ( )));
	connect ( d-> w_categories, SIGNAL( selectionChanged ( )), this, SLOT( categoryChanged ( )));
	connect ( d-> w_items, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedList ( )));
	connect ( d-> w_items, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_items, SIGNAL( returnPressed ( QListViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_items, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( itemContextList ( QListViewItem *, const QPoint & )));
	connect ( d-> w_thumbs, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedIcon ( )));
	connect ( d-> w_thumbs, SIGNAL( doubleClicked ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_thumbs, SIGNAL( returnPressed ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_thumbs, SIGNAL( contextMenuRequested ( QIconViewItem *, const QPoint & )), this, SLOT( itemContextIcon ( QIconViewItem *, const QPoint & )));

	d-> m_filter_active = false;

	d-> m_stack-> setCurrentWidget ( d-> w_items );
	d-> m_viewmode = ViewMode_List;

	d-> m_selected = 0;

	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));

	languageChange ( );

//	activating the [All Items] category takes too long
//	d-> w_categories-> setSelected ( d-> w_categories-> firstChild ( ), true );

	setFocusProxy ( d-> w_item_types );
}

void CSelectItem::languageChange ( )
{
	d-> w_item_types_label-> setText ( tr( "Item type:" ));
	d-> w_filter_label-> setText ( tr( "Filter:" )); 

	d-> w_goto-> setShortcut ( tr( "Ctrl+F", "Find Item" ));
	d-> w_goto-> setToolTip ( tr( "Find Item..." ) + " (" + QString( d-> w_goto-> shortcut ( )) + ")");

	d-> w_filter_expression-> setToolTip ( tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));
	d-> w_filter_clear-> setToolTip ( tr( "Reset an active filter" ));

//	d-> w_categories-> setColumnText ( 0, tr( "Category" ));
//	d-> w_categories-> firstChild ( )-> repaint ( );
//	d-> w_categories-> lastItem ( )-> repaint ( );

//	d-> w_items-> setColumnText ( 1, tr( "Part #" ));
//	d-> w_items-> setColumnText ( 2, tr( "Description" ));

	d-> w_view_list-> setToolTip ( tr( "View as plain list" ));
	d-> w_view_images-> setToolTip ( tr( "View as list with images" ));
	d-> w_view_thumbs-> setToolTip ( tr( "View as thumbnails" ));
}

void CSelectItem::setOnlyWithInventory ( bool b )
{
	if ( b != d-> m_inv_only ) {
//		d-> m_type_combo-> reset ( b );
		d-> w_item_types-> setModel ( new ItemTypeModel ( b ));
		d-> m_inv_only = b;
	}
}

bool CSelectItem::isOnlyWithInventory ( ) const
{
	return d-> m_inv_only;
}

void CSelectItem::pictureUpdated ( BrickLink::Picture *pic )
{
	if ( !pic )
		return;

	for ( QListViewItemIterator it ( d-> w_items ); it. current ( ); ++it ) {
		ItemListItem *ii = static_cast <ItemListItem *> ( it. current ( ));

		if (( pic-> item ( ) == ii-> item ( )) && ( pic-> color ( ) == ii-> item ( )-> defaultColor ( )))
			ii-> pictureChanged ( );
	}
	for ( QIconViewItem *it = d-> w_thumbs-> firstItem ( ); it; it = it-> nextItem ( )) {
		ItemIconItem *ii = static_cast <ItemIconItem *> ( it );

		if (( pic-> item ( ) == ii-> item ( )) && ( pic-> color ( ) == ii-> item ( )-> defaultColor ( )))
			ii-> pictureChanged ( );
	}
}


void CSelectItem::viewModeChanged ( int ivm )
{
	if ( ivm == d-> m_viewmode )
		return;

	const BrickLink::ItemType *itt = d-> m_type_combo-> currentItemType ( );
	CatListItem *cli = static_cast <CatListItem *> ( d-> w_categories-> selectedItem ( ));
	const BrickLink::Category *cat = cli ? cli-> category ( ) : 0;

	if ( itt && cat )
		setViewMode ( checkViewMode ((ViewMode) ivm, itt, cat ), itt, cat );
}

void CSelectItem::findItem ( )
{
	const BrickLink::ItemType *itt = d-> m_type_combo-> currentItemType ( );

	if ( !itt )
		return;

	QString str;

	if ( CMessageBox::getString ( this, tr( "Please enter the complete item number:" ), str )) {
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

	d-> m_type_combo-> setCurrentItemType ( itt ? itt : BrickLink::inst ( )-> itemType ( 'P' ));
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

		d-> w_filter_expression-> setEditText ( filter );
		applyFilter ( );
		return true;
	}
	return false;
}

bool CSelectItem::setItemType ( const BrickLink::ItemType *itt )
{
	if ( !itt )
		return false;

	d-> m_type_combo-> setCurrentItemType ( itt ? itt : BrickLink::inst ( )-> itemType ( 'P' ));
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
	fillCategoryView ( d-> m_type_combo-> currentItemType ( ));
}

void CSelectItem::categoryChanged ( )
{
	const BrickLink::ItemType *itt = d-> m_type_combo-> currentItemType ( );
	CatListItem *cli = static_cast <CatListItem *> ( d-> w_categories-> selectedItem ( ));
	const BrickLink::Category *cat = cli ? cli-> category ( ) : 0;

	fillItemView ( itt, cat );
}

bool CSelectItem::fillCategoryView ( const BrickLink::ItemType *itype, const BrickLink::Category *select )
{
	d-> w_categories-> clearSelection ( );
	d-> w_categories-> clear ( );

	if ( !itype ) {
		// SPERREN
		return false;
	}
	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CatListItem *cat;
	bool found = false;

	cat = new CatListItem ( d-> w_categories, Cat_AllParts );
	if ( select == Cat_AllParts )
		cat-> setSelected ( found = true );

	for ( const BrickLink::Category **catp = itype-> categories ( ); *catp; catp++ ) {
		cat = new CatListItem ( d-> w_categories, *catp );
		
		if ( *catp == select )
			d-> w_categories-> setSelected ( cat, found = true );
	}

	d-> w_categories-> sort ( );

	emit hasColors ( itype-> hasColors ( ));
	
	QApplication::restoreOverrideCursor ( );

	return select ? found : true;
}

bool CSelectItem::fillItemView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	d-> m_items_tip-> hideTip ( );

	d-> w_thumbs-> clearSelection ( );
	d-> w_items-> clearSelection ( );
	d-> w_thumbs-> clear ( );
	d-> w_items-> clear ( );
	d-> w_items-> setColumnWidthMode ( 1, QListView::Manual );
	d-> w_items-> setColumnWidth ( 1, 0 );
	d-> w_items-> setColumnWidthMode ( 1, QListView::Maximum );

	bool found = false;

	if ( itt && cat )
		found = setViewMode ( checkViewMode ( d-> m_viewmode, itt, cat ), itt, cat, select );

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
	bool same_vm = ( ivm == d-> m_viewmode );
	ViewMode oldvm = d-> m_viewmode;

	d-> m_viewmode = ivm;

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	switch ( ivm ) {
		case ViewMode_Thumbnails:
			if ( d-> w_thumbs-> count ( ) == 0 )
				found = fillItemIconView ( itt, cat, select );

			if ( !same_vm )
				d-> w_stack-> raiseWidget ( d-> w_thumbs );
			break;

		case ViewMode_ListWithImages:
		case ViewMode_List:
			d-> w_items-> setColumnWidth ( 0, ( ivm == ViewMode_ListWithImages ) ? 40 + 2 * d-> w_items-> itemMargin ( ) : 0 );
			d-> w_items-> header ( )-> adjustHeaderSize ( );

			if ( d-> w_items-> childCount ( ) == 0 )
				found = fillItemListView ( itt, cat, select );

			if ( !same_vm ) {
				if ( oldvm != ViewMode_Thumbnails ) {
					for ( QListViewItemIterator it ( d-> w_items ); it. current ( ); ++it )
						it. current ( )-> setup ( );
				}
				d-> w_stack-> raiseWidget ( d-> w_items );
			}
			break;
	}
	d-> w_filter_expression-> setEnabled ( ivm != ViewMode_Thumbnails );
	d-> w_filter_clear-> setEnabled ( ivm != ViewMode_Thumbnails );

	applyFilter ( );

	QApplication::restoreOverrideCursor ( );

	if ( !same_vm ) {
		d-> w_viewpopup-> setItemChecked ( ViewMode_List, false );
		d-> w_viewpopup-> setItemChecked ( ViewMode_ListWithImages, false );
		d-> w_viewpopup-> setItemChecked ( ViewMode_Thumbnails, false );
		d-> w_viewpopup-> setItemChecked ( ivm, true );
	}
	return select ? ( found != 0 ) : true;
}

void CSelectItem::showEvent ( QShowEvent * )
{
	ensureSelectionVisible ( );
}

void CSelectItem::ensureSelectionVisible ( )
{
	d-> w_categories-> centerItem ( d-> w_categories-> selectedItem ( ));

	if ( d-> m_viewmode == ViewMode_Thumbnails ) {
		QIconViewItem *ivi = QIconView__selectedItem ( d-> w_thumbs );

		if ( ivi ) {
			QPoint p = ivi-> rect ( ). center ( );
			d-> w_thumbs-> center ( p. x ( ), p. y ( ), 1.f, 1.f );
		}
	}
	else
		d-> w_items-> centerItem ( d-> w_items-> selectedItem ( ));
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
			ItemIconItem *iii = new ItemIconItem ( d-> w_thumbs, item, d-> m_viewmode, d-> m_inv_only );
			if ( item == select ) {
				found = item;
				d-> w_thumbs-> setSelected ( iii, true );
			}
		}
	}
	d-> w_thumbs-> arrangeItemsInGrid ( );

	return found;
}

const BrickLink::Item *CSelectItem::fillItemListView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select )
{
	const BrickLink::Item *found = 0;
	const BrickLink::Item * const *itemp = BrickLink::inst ( )-> items ( ). data ( );
	uint count = BrickLink::inst ( )-> items ( ). count ( );

	CDisableUpdates disupd ( d-> w_items );

	for ( uint i = 0; i < count; i++, itemp++ ) {
		const BrickLink::Item *item = *itemp;

		if (( item-> itemType ( ) == itt ) && (( cat == Cat_AllParts ) || ( item-> hasCategory ( cat )))) {
			ItemListItem *ili = new ItemListItem ( d-> w_items, item, d-> m_viewmode, d-> m_inv_only );
			if ( item == select ) {
				found = item;
				d-> w_items-> setSelected ( ili, true );
			}
		}
	}
	disupd. reenable ( );
	static_cast <HackListView *> ( d-> w_items )-> hackUpdateContents ( );

	return found;
}


void CSelectItem::applyFilter ( )
{
	if ( d-> m_viewmode == ViewMode_Thumbnails )
		return;

	QRegExp regexp ( d-> w_filter_expression-> lineEdit ( )-> text ( ), false, true );

	bool regexp_ok = !regexp. isEmpty ( ) && regexp. isValid ( );

	if ( !regexp_ok && !d-> m_filter_active ) {
		//qDebug ( "ignoring filter...." );
		return;
	}

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CDisableUpdates disupd ( d-> w_items );

	for ( QListViewItemIterator it ( d-> w_items ); it. current ( ); ++it ) {
		QListViewItem *ivi = it. current ( );

		bool b = !regexp_ok || ( regexp. search ( ivi-> text ( 1 )) >= 0 ) || ( regexp. search ( ivi-> text ( 2 )) >= 0 );

		if ( !b && ivi-> isSelected ( ))
			d-> w_items-> setSelected ( ivi, false );

		ivi-> setVisible ( b );
	}
	d-> m_filter_active = regexp_ok;

	QApplication::restoreOverrideCursor ( );
}

const BrickLink::Item *CSelectItem::item ( ) const
{
	return d-> m_selected;
}

void CSelectItem::itemChangedList ( )
{
	QListViewItem *lvi = d-> w_items-> selectedItem ( );
	const BrickLink::Item *newitem = lvi ? static_cast <ItemListItem *> ( lvi )-> item ( ) : 0;

	if ( lvi )
		d-> w_items-> ensureItemVisible ( lvi );

	if ( newitem != d-> m_selected ) {
		d-> m_selected = newitem;
		emit itemSelected ( d-> m_selected, false );
	}
}

void CSelectItem::itemChangedIcon ( )
{
	QIconViewItem *ivi = QIconView__selectedItem ( d-> w_thumbs );
	const BrickLink::Item *newitem = ivi ? static_cast <ItemIconItem *> ( ivi )-> item ( ) : 0;

	if ( ivi )
		d-> w_thumbs-> ensureItemVisible ( ivi );

	if ( newitem != d-> m_selected ) {
		d-> m_selected = newitem;
		emit itemSelected ( d-> m_selected, false );
	}
}

void CSelectItem::itemConfirmed ( )
{
	if ( d-> m_selected )
		emit itemSelected ( d-> m_selected, true );
}

QSize CSelectItem::sizeHint ( ) const
{
	QFontMetrics fm = fontMetrics ( );

	return QSize ( 120 * fm. width ( 'x' ), 20 * fm. height ( ));
}

void CSelectItem::itemContextList ( QListViewItem *lvi, const QPoint &pos )
{
	const BrickLink::Item *item = lvi ? static_cast <ItemListItem *> ( lvi )-> item ( ) : 0;

	itemContext ( item, pos );
}

void CSelectItem::itemContextIcon ( QIconViewItem *ivi, const QPoint &pos )
{
	const BrickLink::Item *item = ivi ? static_cast <ItemIconItem *> ( ivi )-> item ( ) : 0;

	itemContext ( item, pos );
}

void CSelectItem::itemContext ( const BrickLink::Item *item, const QPoint &pos )
{
	CatListItem *cli = static_cast <CatListItem *> ( d-> w_categories-> selectedItem ( ));
	const BrickLink::Category *cat = cli ? cli-> category ( ) : 0;

	if ( !item-> category ( ) || ( item-> category ( ) == cat ))
		return;

	QPopupMenu pop ( this );
	pop.insertItem ( tr( "View item's category" ), 0 );

	if ( pop. exec ( pos ) == 0 ) {
		setItem ( item );
		ensureSelectionVisible ( );
	}
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
