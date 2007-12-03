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
#include <QHeaderView>
#include <QStackedLayout>
#include <QMenu>

//#include "citemtypecombo.h"
//#include "cresource.h"
//#include "clistview.h"
#include "cselectitem.h"
#include "cutility.h"
#include "cmessagebox.h"








class CSelectItemPrivate {
public:
	QLabel *      w_item_types_label;
	QComboBox *   w_item_types;
	QTableView *  w_categories;
	QStackedLayout *m_stack;
	QTableView *  w_items;
	QListView *   w_thumbs;
	QToolButton * w_goto;
	QLabel *      w_filter_label;
	QToolButton * w_filter_clear;
	QComboBox *   w_filter_expression;
	QToolButton * w_viewbutton;
	QAction *     m_viewaction[3];
	QMenu *       w_viewmenu;
	CSelectItem::ViewMode m_viewmode;

	bool                   m_filter_active;
	bool                   m_inv_only;
	const BrickLink::Item *m_selected;
//	ItemListToolTip *      m_items_tip;
};


CSelectItem::CSelectItem(bool inv_only, QWidget *parent)
	: QWidget(parent)
{
	d = new CSelectItemPrivate ( );

	d-> m_inv_only = inv_only;

	d->w_item_types_label = new QLabel(this);
	d->w_item_types = new QComboBox(this);
	d->w_item_types->setEditable(false);

	d->w_categories = new QTableView(this);
	d->w_categories->setSortingEnabled(true);
	d->w_categories->setAlternatingRowColors(true);
	d->w_categories->setSelectionBehavior(QAbstractItemView::SelectRows);
	d->w_categories->setSelectionMode(QAbstractItemView::SingleSelection);
	d->w_categories->setTextElideMode(Qt::ElideRight);
	d->w_categories->horizontalHeader()->setResizeMode(QHeaderView::Fixed);
	d->w_categories->horizontalHeader()->setStretchLastSection(true);
	d->w_categories->horizontalHeader()->setMovable(false);
	d->w_categories->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	d->w_categories->verticalHeader()->hide();
	d->w_categories->setShowGrid(false);
	d->w_categories->setWordWrap(false);

	d->w_goto = new QToolButton(this);
	d->w_goto->setAutoRaise(true);
	d->w_goto->setIcon(QIcon(":/images/22x22/edit_find"));

	d->w_filter_label = new QLabel(this);
	d->w_filter_clear = new QToolButton(this);
	d->w_filter_clear->setAutoRaise(true);
	d->w_filter_clear->setIcon(QIcon(":/images/22x22/filter_clear"));

	d->w_filter_expression = new QComboBox(this);
	d->w_filter_expression->setEditable(true);

	d->w_viewmenu = new QMenu(this);
	QActionGroup *ag = new QActionGroup(this);
	ag->setExclusive(true);

	d->m_viewaction[ListMode] = new QAction(ag);
	d->m_viewaction[ListMode]->setIcon(QIcon(":/images/22x22/viewmode_list"));
	d->m_viewaction[ListMode]->setCheckable(true);
	connect(d->m_viewaction[ListMode], SIGNAL(triggered()), this, SLOT(showAsList()));
	d->m_viewaction[TableMode] = new QAction(ag);
	d->m_viewaction[TableMode]->setIcon(QIcon(":/images/22x22/viewmode_images"));
	d->m_viewaction[TableMode]->setCheckable(true);
	connect(d->m_viewaction[TableMode], SIGNAL(triggered()), this, SLOT(showAsTable()));
	d->m_viewaction[ThumbsMode] = new QAction(ag);
	d->m_viewaction[ThumbsMode]->setIcon(QIcon(":/images/22x22/viewmode_thumbs"));
	d->m_viewaction[ThumbsMode]->setCheckable(true);
	connect(d->m_viewaction[ThumbsMode], SIGNAL(triggered()), this, SLOT(showAsThumbs()));

	d->w_viewmenu->addActions(ag->actions());

	d->w_viewbutton = new QToolButton(this);
	d->w_viewbutton->setAutoRaise(true);
	d->w_viewbutton->setPopupMode(QToolButton::MenuButtonPopup);
	d->w_viewbutton->setMenu(d->w_viewmenu);
	d->w_viewbutton->setIcon(QIcon(":/images/22x22/viewmode"));

	d->w_items = new QTableView(this);
	d->w_items->setSortingEnabled(true);
	d->w_items->setAlternatingRowColors(true);
	d->w_items->setSelectionBehavior(QAbstractItemView::SelectRows);
	d->w_items->setSelectionMode(QAbstractItemView::SingleSelection);
	d->w_items->setTextElideMode(Qt::ElideRight);
	d->w_items->horizontalHeader()-> setResizeMode(QHeaderView::Fixed);
	d->w_items->horizontalHeader()-> setStretchLastSection(true);
	d->w_items->horizontalHeader()-> setMovable(false);
	d->w_items->verticalHeader()-> setResizeMode(QHeaderView::ResizeToContents);
	d->w_items->verticalHeader()-> hide();
	d->w_items->setShowGrid(false);

//	d-> w_items-> setSortColumn ( 2 );
//	d-> w_items-> setColumnWidthMode ( 0, QListView::Manual );
//	d-> w_items-> setColumnWidth ( 0, 0 );
//	d-> m_items_tip = new ItemListToolTip ( d-> w_items );

	d->w_thumbs = new QListView(this);
	d->w_thumbs->setMovement(QListView::Static);
	d->w_thumbs->setViewMode(QListView::IconMode);
	d->w_thumbs->setSelectionMode(QAbstractItemView::SingleSelection);
	d->w_thumbs->setTextElideMode(Qt::ElideRight);

//	d-> w_thumbs-> setShowToolTips ( false );
//	(void) new ItemIconToolTip ( d-> w_thumbs );

	d->w_item_types->setModel(BrickLink::core()->itemTypeModel(d->m_inv_only ? BrickLink::ItemTypeModel::ExcludeWithoutInventory : BrickLink::ItemTypeModel::Default));
	d->w_categories->setModel(BrickLink::core()->categoryModel(BrickLink::CategoryModel::IncludeAllCategoriesItem));

	connect(d->w_goto, SIGNAL(clicked()), this, SLOT(findItem()));
	connect(d->w_filter_clear, SIGNAL(clicked()), d->w_filter_expression, SLOT(clearEdit()));
	connect(d->w_filter_expression, SIGNAL(textChanged(const QString &)), this, SLOT(applyFilter()));

	connect(d->w_item_types, SIGNAL(currentIndexChanged(int)), this, SLOT(itemTypeChanged()));
	connect(d->w_categories->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(categoryChanged()));

//	connect ( d-> w_item_types, SIGNAL( itemTypeActivated ( const BrickLink::ItemType * )), this, SLOT( itemTypeChanged ( )));
//	connect ( d-> w_categories, SIGNAL( selectionChanged ( )), this, SLOT( categoryChanged ( )));
	connect ( d-> w_items, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedList ( )));
	connect ( d-> w_items, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_items, SIGNAL( returnPressed ( QListViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_items, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( itemContextList ( QListViewItem *, const QPoint & )));
	connect ( d-> w_thumbs, SIGNAL( selectionChanged ( )), this, SLOT( itemChangedIcon ( )));
	connect ( d-> w_thumbs, SIGNAL( doubleClicked ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_thumbs, SIGNAL( returnPressed ( QIconViewItem * )), this, SLOT( itemConfirmed ( )));
	connect ( d-> w_thumbs, SIGNAL( contextMenuRequested ( QIconViewItem *, const QPoint & )), this, SLOT( itemContextIcon ( QIconViewItem *, const QPoint & )));
//	connect ( d-> w_viewpopup, SIGNAL( activated ( int )), this, SLOT( viewModeChanged ( int )));

	QGridLayout *toplay = new QGridLayout(this);
	toplay->setContentsMargins(0, 0, 0, 0);
	toplay->setColumnStretch(0, 25);
	toplay->setColumnStretch(1, 75);
	toplay->setRowStretch(0, 0);
	toplay->setRowStretch(1, 100);
	
	QBoxLayout *lay = new QHBoxLayout();
	lay->addWidget(d->w_item_types_label, 0);
	lay->addWidget(d->w_item_types, 1);

	toplay->addLayout(lay, 0, 0);
	toplay->addWidget(d->w_categories, 1, 0);

	lay = new QHBoxLayout();
	lay->addWidget(d-> w_goto, 0);
	lay->addSpacing(6);
	lay->addWidget(d->w_filter_clear, 0);
	lay->addWidget(d->w_filter_label, 0);
	lay->addWidget(d->w_filter_expression, 15);
	lay->addSpacing(6);
	lay->addWidget(d->w_viewbutton);

	toplay->addLayout(lay, 0, 1);
	d->m_stack = new QStackedLayout();
	d->m_stack->addWidget(d->w_items);
	d->m_stack->addWidget(d->w_thumbs);
	toplay->addLayout(d->m_stack, 1, 1);
	
	d-> m_filter_active = false;

	d->m_stack->setCurrentWidget(d->w_items);
	d->m_viewmode = ListMode;

	d->m_selected = 0;

	languageChange();

//	activating the [All Items] category takes too long
//	d-> w_categories-> setSelected ( d-> w_categories-> firstChild ( ), true );
	setFocusProxy(d->w_item_types);
}

void CSelectItem::languageChange()
{
	d->w_item_types_label->setText(tr("Item type:"));
	d->w_filter_label->setText(tr("Filter:")); 

	d->w_goto->setShortcut(tr( "Ctrl+F", "Find Item" ));
	d->w_goto->setToolTip(tr( "Find Item..." ) + " (" + QString(d->w_goto->shortcut()) + ")");

	d->w_filter_expression->setToolTip(tr( "Filter the list using this pattern (wildcards allowed: * ? [])" ));
	d->w_filter_clear->setToolTip(tr( "Reset an active filter" ));
	d->w_viewbutton->setToolTip(tr( "View" ));

//	d-> w_categories-> setColumnText ( 0, tr( "Category" ));
//	d-> w_categories-> firstChild ( )-> repaint ( );
//	d-> w_categories-> lastItem ( )-> repaint ( );

//	d-> w_items-> setColumnText ( 1, tr( "Part #" ));
//	d-> w_items-> setColumnText ( 2, tr( "Description" ));

	d->m_viewaction[ListMode]->setText(tr("List"));
	d->m_viewaction[TableMode]->setText(tr("List with images"));
	d->m_viewaction[ThumbsMode]->setText(tr("Thumbnails"));
}

bool CSelectItem::isOnlyWithInventory ( ) const
{
	return d-> m_inv_only;
}

void CSelectItem::categoryChanged()
{
}

const BrickLink::Category *CSelectItem::currentCategory() const
{
	BrickLink::CategoryModel *model = qobject_cast<BrickLink::CategoryModel *>(d->w_categories->model());

	if (model && d->w_categories->selectionModel()->hasSelection()) {
		QModelIndex idx = d->w_categories->selectionModel()->selectedIndexes().front();
		return model->category(idx);
	}
	else
		return 0;
}

void CSelectItem::setCurrentCategory(const BrickLink::Category *cat)
{
	BrickLink::CategoryModel *model = qobject_cast<BrickLink::CategoryModel *>(d->w_categories->model());

	if (model)
		d->w_categories->selectionModel()->select(model->index(cat), QItemSelectionModel::SelectCurrent);
}

void CSelectItem::itemTypeChanged()
{
	const BrickLink::Category *oldcat = currentCategory();

	BrickLink::CategoryModel *model = qobject_cast<BrickLink::CategoryModel *>(d->w_categories->model());
	model->setItemTypeFilter(currentItemType());
	setCurrentCategory(oldcat);
}

const BrickLink::ItemType *CSelectItem::currentItemType() const
{
	BrickLink::ItemTypeModel *model = qobject_cast<BrickLink::ItemTypeModel *>(d->w_item_types->model());

	if (model && d->w_item_types->currentIndex() >= 0) {
		QModelIndex idx = model->index(d->w_item_types->currentIndex(), 0, QModelIndex());
		return model->itemType(idx);
	}
	else
		return 0;
}

void CSelectItem::setCurrentItemType(const BrickLink::ItemType *it)
{
	BrickLink::ItemTypeModel *model = qobject_cast<BrickLink::ItemTypeModel *>(d->w_item_types->model());

	if (model)
		d->w_item_types->setCurrentIndex(model->index(it).row());
}


bool CSelectItem::checkViewMode(CSelectItem::ViewMode vm)
{
	if (vm == d->m_viewmode)
		return false;

	if ((vm != ListMode) && (currentCategory() == BrickLink::CategoryModel::AllCategories)) {
		if (CMessageBox::question(this, tr("Viewing all items with images is a bandwidth- and memory-hungry operation.<br />Are you sure you want to continue?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return false;
	}
	return true;
}

void CSelectItem::showAsList()
{
	if (checkViewMode(ListMode)) {
		if (d->m_viewmode == ThumbsMode)
			d->m_stack->setCurrentWidget(d->w_items);

		d->w_items->hideColumn(1);
		d->m_viewmode = ListMode;
	}
}

void CSelectItem::showAsTable()
{
	if (checkViewMode(TableMode)) {
		if (d->m_viewmode == ThumbsMode)
			d->m_stack->setCurrentWidget(d->w_items);

		d->w_items->showColumn(1);
		d->m_viewmode = TableMode;
	}
}

void CSelectItem::showAsThumbs()
{
	if (checkViewMode(ThumbsMode)) {
		if (currentCategory() == BrickLink::CategoryModel::AllCategories) {
			if (CMessageBox::question(this, tr("Viewing all items with images is a bandwidth- and memory-hungry operation.<br />Are you sure you want to continue?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return;
		}
		d->m_stack->setCurrentWidget(d->w_thumbs);
		d->m_viewmode = ThumbsMode;
	}
}


void CSelectItem::findItem()
{
	const BrickLink::ItemType *itt = currentItemType();

	if (!itt)
		return;

	QString str;

	if (CMessageBox::getString(this, tr("Please enter the complete item number:"), str)) {
		const BrickLink::Item *item = BrickLink::core()->item(itt->id(), str.toAscii().constData());

		if (item) {
			setCurrentItem(item);
			ensureSelectionVisible();
		}
		else
			QApplication::beep ( );
	}
}

const BrickLink::Item *CSelectItem::currentItem() const
{
	return 0;
}

bool CSelectItem::setCurrentItem(const BrickLink::Item *item)
{
	if (!item)
		return false;

	bool found = false;
	const BrickLink::ItemType *itt = item ? item->itemType() : 0;
	const BrickLink::Category *cat = item ? item->category() : 0;

	setCurrentItemType(itt ? itt : BrickLink::core()->itemType('P'));
	setCurrentCategory(cat ? cat : BrickLink::CategoryModel::AllCategories);

	//TODO: set current item, if filter permits -> return true/false (found)
	return found;
}



void CSelectItem::showEvent(QShowEvent *)
{
	ensureSelectionVisible();
}

void CSelectItem::ensureSelectionVisible ( )
{
	const BrickLink::Category *cat = currentCategory();
	const BrickLink::Item *item = currentItem();

	if (cat) {
		BrickLink::CategoryModel *model = qobject_cast<BrickLink::CategoryModel *>(d->w_categories->model());

		d->w_categories->scrollTo(model->index(cat));
	}

	if (item) {
/*		BrickLink::ItemModel *model = qobject_cast<BrickLink::ItemModel *>(d->w_items->model());

		d->w_items->scrollTo(model->index(item));
		d->w_thumbs->scrollTo(model->index(item));*/
	}
}


void CSelectItem::applyFilter()
{
	QRegExp regexp(d->w_filter_expression->lineEdit()->text(), Qt::CaseInsensitive, QRegExp::Wildcard);

	bool regexp_ok = !regexp.isEmpty() && regexp.isValid();

	if (!regexp_ok && !d->m_filter_active) {
		//qDebug ( "ignoring filter...." );
		return;
	}
/*
	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

//	CDisableUpdates disupd ( d-> w_items );

	for ( QListViewItemIterator it ( d-> w_items ); it. current ( ); ++it ) {
		QListViewItem *ivi = it. current ( );

		bool b = !regexp_ok || ( regexp. search ( ivi-> text ( 1 )) >= 0 ) || ( regexp. search ( ivi-> text ( 2 )) >= 0 );

		if ( !b && ivi-> isSelected ( ))
			d-> w_items-> setSelected ( ivi, false );

		ivi-> setVisible ( b );
	}

	QApplication::restoreOverrideCursor ( );
*/
	d->m_filter_active = regexp_ok;
}


QSize CSelectItem::sizeHint ( ) const
{
	QFontMetrics fm = fontMetrics ( );

	return QSize ( 120 * fm. width ( 'x' ), 20 * fm. height ( ));
}

/*
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

	if ( !item || !item-> category ( ) || ( item-> category ( ) == cat ))
		return;

	QPopupMenu pop ( this );
	pop.insertItem ( tr( "View item's category" ), 0 );

	if ( pop. exec ( pos ) == 0 ) {
		setItem ( item );
		ensureSelectionVisible ( );
	}
}
*/
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

DSelectItem::DSelectItem(bool only_with_inventory, QWidget *parent)
	: QDialog(parent)
{
	w_si = new CSelectItem(only_with_inventory, this);

	w_ok = new QPushButton ( tr( "&OK" ), this );
	w_ok-> setAutoDefault ( true );
	w_ok-> setDefault ( true );

	w_cancel = new QPushButton ( tr( "&Cancel" ), this );
	w_cancel-> setAutoDefault ( true );

	QFrame *hline = new QFrame ( this );
	hline-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	
	QBoxLayout *toplay = new QVBoxLayout(this);
	toplay-> addWidget ( w_si );
	toplay-> addWidget ( hline );

	QBoxLayout *butlay = new QHBoxLayout();
	butlay-> addStretch ( 60 );
	butlay-> addWidget ( w_ok, 15 );
	butlay-> addWidget ( w_cancel, 15 );
	toplay->addLayout(butlay);

	setSizeGripEnabled ( true );
	setMinimumSize ( minimumSizeHint ( ));

	connect ( w_ok, SIGNAL( clicked ( )), this, SLOT( accept ( )));
	connect ( w_cancel, SIGNAL( clicked ( )), this, SLOT( reject ( )));
	connect ( w_si, SIGNAL( itemSelected ( const BrickLink::Item *, bool )), this, SLOT( checkItem ( const BrickLink::Item *, bool )));

	w_ok-> setEnabled ( false );
}

void DSelectItem::setItemType ( const BrickLink::ItemType *itt )
{
	w_si-> setCurrentItemType ( itt );
}

void DSelectItem::setItem ( const BrickLink::Item *item )
{
	w_si-> setCurrentItem ( item );
}
/*
bool DSelectItem::setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter )
{
	return w_si-> setItemTypeCategoryAndFilter ( itt, cat, filter );
}
*/
const BrickLink::Item *DSelectItem::item ( ) const
{
	return w_si->currentItem ( );
}

void DSelectItem::checkItem ( const BrickLink::Item *it, bool ok )
{
	bool b = w_si-> isOnlyWithInventory ( ) ? it-> hasInventory ( ) : true;

	w_ok-> setEnabled (( it ) && b );

	if ( it && b && ok )
		 w_ok-> animateClick ( );
}

int DSelectItem::exec ( const QRect &pos )
{
	if ( pos. isValid ( ))
		CUtility::setPopupPos ( this, pos );

	return QDialog::exec ( );
}
























#if 0


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



