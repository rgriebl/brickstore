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
#include <qapplication.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qheader.h>
#include <qpopupmenu.h>
#include <qsettings.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qvaluevector.h>

#include "cutility.h"
#include "clistview.h"
#include "clistview_p.h"

CDisableUpdates::CDisableUpdates ( QWidget *w )
	: m_reenabled ( false )
{
	QScrollView *sv = ::qt_cast <QScrollView *> ( w );

	if ( sv )
		m_w = sv-> viewport ( );
	else
		m_w = w;

	m_upd_enabled = m_w-> isUpdatesEnabled ( );
	m_w-> setUpdatesEnabled ( false );
}

CDisableUpdates::~CDisableUpdates ( )
{
	reenable ( );
}

void CDisableUpdates::reenable ( )
{
	if ( !m_reenabled ) {
		m_w-> setUpdatesEnabled ( m_upd_enabled );
		m_reenabled = true;
	}
}


CListView::CListView ( QWidget *parent, const char *name )
	: QListView ( parent, name )
{
	m_paint_above = 0;
	m_paint_current = 0;
	m_paint_below = 0;
	m_painting = false;
	m_current_column = 0;
	m_grid_mode = false;

	m_always_show_selection = false;

	setAllColumnsShowFocus ( true );

	recalc_alternate_background ( );

	m_header_popup = 0;

	header ( )-> installEventFilter ( this );

	connect ( this, SIGNAL( mouseButtonPressed ( int, QListViewItem *, const QPoint &, int )), this, SLOT( checkCurrentColumn ( int, QListViewItem *, const QPoint &, int )));
}

void CListView::recalc_alternate_background ( )
{
	m_alternate_background = CUtility::contrastColor ( qApp-> palette ( ). active ( ). base ( ));
}

void CListView::recalc_highlight_palette ( )
{
	if ( m_always_show_selection ) {
		QPalette p = qApp-> palette ( );
		QColor hc = CUtility::gradientColor ( p. color ( QPalette::Active, QColorGroup::Highlight ), p. color ( QPalette::Inactive, QColorGroup::Highlight ), 0.35f );
		QColor htc = p. color ( QPalette::Active, QColorGroup::HighlightedText );

		p. setColor ( QPalette::Inactive, QColorGroup::Highlight, hc );
		p. setColor ( QPalette::Inactive, QColorGroup::HighlightedText, htc );
		setPalette ( p );
	}
	else {
		if ( ownPalette ( ))
			unsetPalette ( );
	}
}

bool CListView::gridMode ( ) const
{
	return m_grid_mode;
}

void CListView::setGridMode ( bool b )
{
	m_grid_mode = b;
	m_current_column = 0;
	repaint ( );
}

int CListView::currentColumn ( ) const
{
	return m_grid_mode ? m_current_column : -1;
}

void CListView::setCurrentColumn ( int col )
{
	if ( m_grid_mode && ( col >= 0 ) && ( col < columns ( ) ) && ( col != m_current_column )) {
		int oldx = header ( )-> sectionPos ( m_current_column );
		int newx = header ( )-> sectionPos ( col );
		int oldw = columnWidth ( m_current_column );
		int neww = columnWidth ( col );

		m_current_column = col;

		QRect repaint_rect;

		for ( QListViewItemIterator it ( this ); it. current ( ); ++it ) {
			QListViewItem *item = it. current ( );
			int y = itemPos ( item );

			if ( item-> isSelected ( )) {
				repaint_rect |= QRect ( oldx, y, oldw, item-> height ( ));
				repaint_rect |= QRect ( newx, y, neww, item-> height ( ));
			}
		}
		if ( !repaint_rect. isEmpty ( ))
			repaintContents ( repaint_rect, false );

		int movedx = 0;

		if ( newx < contentsX ( ))
			movedx = newx - contentsX ( );
		else if (( newx + neww ) > ( contentsX ( ) + visibleWidth ( )))
			movedx = ( newx + neww ) - ( contentsX ( ) + visibleWidth ( ));
			
		scrollBy ( movedx, 0 );
	}
}

void CListView::checkCurrentColumn ( int /*button*/, QListViewItem *item, const QPoint & /*pos*/, int column )
{
	if ( gridMode ( ) && item && item-> isSelected ( ))
		setCurrentColumn ( column );
}

void CListView::keyPressEvent( QKeyEvent *e )
{
	if ( !m_grid_mode ) {
		QListView::keyPressEvent ( e );
		return;
	}

	if ( !firstChild ( )) {
		e-> ignore ( );
		return; // subclass bug
	}

	if ( !currentItem ( )) {
		setCurrentItem ( firstChild ( ));
		if ( selectionMode ( ) == Single )
			setSelected ( firstChild ( ), true );
		return;
	}

	switch ( e-> key ( )) {
		case Qt::Key_Left : 
		case Qt::Key_Right: {
			e-> accept ( );
			int d = ( e-> key ( ) == Qt::Key_Left ) ? -1 : +1;

			if ( e-> state ( ) & Qt::ShiftButton ) {
				scrollBy ( d * 10, 0 );
			}
			else {
				QHeader *head = header ( );
				int index = head-> mapToIndex ( currentColumn ( ));
				bool found = false;

				while ((( index + d ) >= 0 ) && (( index + d ) < columns ( ))) {
					index += d;
					if ( head-> sectionSize ( head-> mapToSection ( index ))) {
						found = true;
						break;
					}
				}
				if ( found )
					setCurrentColumn ( head-> mapToSection ( index ));
			}
			break;
		}
		case Qt::Key_Enter :
		case Qt::Key_Return: {
			e-> accept ( );

			emit doubleClicked ( currentItem ( ), QPoint ( ), currentColumn ( )); 
			break;
		}
		default:
			QListView::keyPressEvent ( e );
			break;
	}
}

QMap <QString, QString> CListView::saveSettings ( ) const
{
	QMap <QString, QString> map;

	QHeader *head = header ( );
	QStringList wl, hl, ol;

	for ( int i = 0; i < columns ( ); i++ ) {
		const colinfo &ci = m_columns [i];
		
		wl << QString::number ( !ci. m_available && ci. m_visible ? ci. m_width : columnWidth ( i ));
		hl << QString::number ( ci. m_width );
		ol << QString::number ( head-> mapToIndex ( i ));
	}

	map ["ColumnWidths"]       = wl. join ( "," );
	map ["ColumnWidthsHidden"] = hl. join ( "," );
	map ["ColumnOrder"]        = ol. join ( "," );
	map ["SortColumn"]         = QString::number ( sortColumn ( ));
	map ["SortDirection"]      = ( sortOrder ( ) == Ascending ) ? "A" : "D";

	return map;
}

void CListView::loadSettings ( const QMap <QString, QString> &map )
{
	QHeader *head = header ( );
	QMap <QString, QString>::const_iterator it;
	QStringList sl;

	it = map. find ( "ColumnWidths" );

	if ( it != map. end ( )) {
		sl = QStringList::split ( ',', *it, true );

		int i = 0;
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it, i++ ) {
			int w = ( *it ). toInt ( );
			setColumnWidth ( i, w );
			m_columns [i]. m_visible = ( w != 0 );
		}
	}
	
	it = map. find ( "ColumnWidthsHidden" );

	if ( it != map. end ( )) {
		sl = QStringList::split ( ',', *it, true );

		int i = 0;
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it, i++ )
			m_columns [i]. m_width = ( *it ). toInt ( );
	}

	it = map. find ( "ColumnOrder" );

	if ( it != map. end ( )) {
		sl = QStringList::split ( ',', *it, true );

		for ( int i = 0; i < columns ( ); i++ ) {
			int j = sl. findIndex ( QString::number ( i ));

			if ( j >= 0 )
				head-> moveSection ( j, i );
		}
	}

	it = map. find ( "SortColumn" );

	if ( it != map. end ( ))
		setSorting (( *it ). toInt ( ), ( map ["SortDirection"] != "D" ));
}

void CListView::setColumnsHideable ( bool b )
{
	if ( b && !m_header_popup ) {
		m_header_popup = new QPopupMenu ( this );
	}
	else if ( !b && m_header_popup ) {
		delete m_header_popup;
		m_header_popup = 0;
	}
}

bool CListView::columnsHideable ( ) const
{
	return ( m_header_popup );
}

bool CListView::isColumnVisible ( int i ) const
{
	return ( i < columns ( )) && ( columnWidth ( i ) > 0 );
}

void CListView::hideColumn ( int i )
{
	setColumnVisible ( i, false );
}

void CListView::showColumn ( int i )
{
	setColumnVisible ( i, true );
}

void CListView::toggleColumn ( int i )
{
	setColumnVisible ( i, !isColumnVisible ( i ));
}
	
void CListView::setColumnVisible ( int i, bool v )
{
	if ( i < columns ( )) {
		if ( v != m_columns [i]. m_visible )
			update_column ( i, true, false );
	}
}

void CListView::setColumnAvailable ( int i, bool avail )
{
	if ( i < columns ( )) {
		if ( avail != m_columns [i]. m_available )
			update_column ( i, false, true );
	}
}

void CListView::update_column ( int i, bool toggle_visible, bool toggle_available )
{
	if ( !toggle_visible && !toggle_available ) {
		return;
	}
	else if ( toggle_visible && toggle_available ) {
		update_column ( i, true, false );
		update_column ( i, false, true );
	}
	else if ( i < columns ( )) {
		colinfo &ci = m_columns [i];
		int status = 0;

		if ( toggle_available ) {
			ci. m_available = !ci. m_available;

			if ( ci. m_visible )
				status = ci. m_available ? 1 : -1;
		}
		else if ( toggle_visible ) {
			ci. m_visible = !ci. m_visible;

			if ( ci. m_available )
				status = ci. m_visible ? 1 : -1;
		}

		if (( status < 0 ) && columnWidth ( i )) { // hide
			ci. m_width = columnWidth ( i );
			setColumnWidth ( i, 0 );
		}
		else if (( status > 0 ) && !columnWidth ( i )) { // show
			int w = ci. m_width;
			ci. m_width = 0;
			setColumnWidth ( i, w == 0 ? 30 : w );
		}

		if ( status )
			triggerUpdate ( );
	}
}

void CListView::setColumnWidth ( int column, int w )
{
	QListView::setColumnWidth ( column, w );

	QHeader *head = header ( );

	head-> setClickEnabled (( w ), column );
	head-> setResizeEnabled (( w ), column );
}


void CListView::configureColumns ( )
{
	CListViewColumnsDialog cc ( this );
	cc. exec ( );
}

bool CListView::eventFilter ( QObject *o, QEvent *e )
{
	bool res = false;

	if ( o == header ( ) && ( e-> type ( ) == QEvent::ContextMenu ) && m_header_popup ) {
		m_header_popup-> clear ( );
		m_header_popup-> setCheckable ( true );

		m_header_popup-> insertItem ( tr( "Configure..." ), this, SLOT( configureColumns ( )));
		m_header_popup-> insertSeparator ( );

		for ( int i = 0; i < columns ( ); i++ ) {
			if ( m_columns [i]. m_available ) {
				m_header_popup-> insertItem ( columnText ( i ), this, SLOT( toggleColumn ( int )), 0, i );
				m_header_popup-> setItemParameter ( i, i );
				m_header_popup-> setItemChecked ( i, m_columns [i]. m_visible );
			}
		}

		m_header_popup-> popup ( static_cast<QContextMenuEvent *> ( e )-> globalPos ( ));
		res = true;
	}
	return res ? true : QListView::eventFilter ( o, e );
}

bool CListView::event ( QEvent *e )
{
	if ( e-> type ( ) == QEvent::ApplicationPaletteChange ) {
		recalc_alternate_background ( );
		recalc_highlight_palette ( );
	}
	return QListView::event ( e );
}

void CListView::setAlwaysShowSelection ( bool b )
{
	if ( b != m_always_show_selection ) {
		m_always_show_selection = b;

		recalc_highlight_palette ( );
	}
}

bool CListView::hasAlwaysShowSelection ( ) const
{
	return m_always_show_selection;
}

void CListView::setSorting ( int column, bool ascending )
{
	QListView::setSorting ( column, ascending );
	QListViewItem *item = firstChild ( );

	while ( item ) {
		static_cast<CListViewItem *> ( item )-> m_known = false;
		item = item-> itemBelow ( );
	}
	centerItem ( currentItem ( ));
}

void CListView::centerItem ( const QListViewItem *item )
{
	if ( item )
		center ( 0, itemPos ( item ) + item-> height ( ) / 2, 1.f, 1.f );
}

void CListView::viewportPaintEvent ( QPaintEvent *e )
{
	m_paint_above = 0;
	m_paint_current = 0;
	m_paint_below = 0;
	m_painting = true;

	QListView::viewportPaintEvent ( e );

	m_painting = false;
}

CListViewItem::CListViewItem ( QListView *parent )
	: QListViewItem ( parent )
{
	init ( );
}

CListViewItem::CListViewItem ( QListViewItem *parent )
	: QListViewItem ( parent )
{
	init ( );
}

CListViewItem::CListViewItem ( QListView *parent, QListViewItem *after )
	: QListViewItem ( parent, after )
{
	init ( );
}

CListViewItem::CListViewItem ( QListViewItem *parent, QListViewItem *after)
	: QListViewItem ( parent, after )
{
	init ( );
}

CListViewItem::CListViewItem ( QListView *parent,
	QString label1, QString label2, QString label3, QString label4,
	QString label5, QString label6, QString label7, QString label8 )
	: QListViewItem ( parent, label1, label2, label3, label4, label5, label6, label7, label8 )
{
	init ( );
}

CListViewItem::CListViewItem ( QListViewItem *parent,
	QString label1, QString label2, QString label3, QString label4,
	QString label5, QString label6, QString label7, QString label8 )
	: QListViewItem ( parent, label1, label2, label3, label4, label5, label6, label7, label8 )
{
	init ( );
}

CListViewItem::CListViewItem ( QListView *parent, QListViewItem *after,
	QString label1, QString label2, QString label3, QString label4,
	QString label5, QString label6, QString label7, QString label8 )
	: QListViewItem ( parent, after, label1, label2, label3, label4, label5, label6, label7, label8 )
{
	init ( );
}

CListViewItem::CListViewItem ( QListViewItem *parent, QListViewItem *after,
	QString label1, QString label2, QString label3, QString label4,
	QString label5, QString label6, QString label7, QString label8 )
	: QListViewItem ( parent, after, label1, label2, label3, label4, label5, label6, label7, label8 )
{
	init ( );
}

void CListViewItem::init ( )
{
	m_odd = m_known = false;
}


void CListViewItem::paintCell ( QPainter *p, const QColorGroup &cg, int column, int width, int alignment )
{
	QColorGroup _cg = cg;
	QListView *lv = listView ( );

	const QPixmap *pm = lv-> viewport ( )-> backgroundPixmap ( );

	if ( pm && !pm-> isNull ( )) {
		_cg. setBrush ( QColorGroup::Base, QBrush( backgroundColor ( ), *pm ));
		QPoint o = p-> brushOrigin ( );
		p-> setBrushOrigin ( o. x ( ) - lv-> contentsX ( ), o. y ( ) - lv-> contentsY ( ));
	}
	else {
		if ( lv-> viewport ( )-> backgroundMode ( ) == Qt::FixedColor )
			_cg. setColor ( QColorGroup::Background, backgroundColor ( ));
		else
			_cg. setColor ( QColorGroup::Base, backgroundColor ( ));
	}
	if ( !lv-> isEnabled ( ))
		_cg. setColor ( QColorGroup::Text, lv-> palette ( ). color ( QPalette::Disabled, QColorGroup::Text ));

	QListViewItem::paintCell ( p, _cg, column, width, alignment );
}


bool CListViewItem::isAlternate()
{
	CListView *lv = static_cast<CListView *> ( listView ( ));

	if ( lv && lv-> m_alternate_background. isValid ( )) {
		CListViewItem *above;

		// Ok, there's some weirdness here that requires explanation as this is a
		// speed hack.	itemAbove() is a O(n) operation (though this isn't
		// immediately clear) so we want to call it as infrequently as possible --
		// especially in the case of painting a cell.
		//
		// So, in the case that we *are* painting a cell:	(1) we're assuming that
		// said painting is happening top to bottem -- this assumption is present
		// elsewhere in the implementation of this class, (2) itemBelow() is fast --
		// roughly constant time.
		//
		// Given these assumptions we can do a mixture of caching and telling the
		// next item that the when that item is the current item that the now
		// current item will be the item above it.
		//
		// Ideally this will make checking to see if the item above the current item
		// is the alternate color a constant time operation rather than 0(n).

		if ( lv-> m_painting ) {
			if ( lv-> m_paint_current != this ) {
				lv-> m_paint_above = lv-> m_paint_below == this ? lv-> m_paint_current : itemAbove ( );
				lv-> m_paint_current = this;
				lv-> m_paint_below = itemBelow ( );
			}

			above = static_cast<CListViewItem *> ( lv-> m_paint_above );
		}
		else {
			above = static_cast<CListViewItem *> ( itemAbove ( ));
		}

		m_known = above ? above-> m_known : true;
		if ( m_known ) {
			 m_odd = above ? !above->m_odd : false;
		}
		else {
			 CListViewItem *item;
			 bool previous = true;

			 if ( parent ( )) {
				item = static_cast<CListViewItem *> ( parent ( ));
				if ( item )
					 previous = item-> m_odd;
				item = static_cast<CListViewItem *> ( parent ( )-> firstChild ( ));
			 }
			 else {
				item = static_cast<CListViewItem *> ( lv-> firstChild ( ));
			 }

			 while ( item ) {
				item-> m_odd = previous = !previous;
				item-> m_known = true;
				item = static_cast<CListViewItem *> ( item-> nextSibling ( ));
			 }
		}
		return m_odd;
	}
	return false;
}


const QColor &CListViewItem::backgroundColor ( )
{
	if ( isAlternate ( ))
		return static_cast<CListView *> ( listView ( ))-> m_alternate_background;
	return listView ( )-> viewport ( )-> colorGroup ( ). base ( );
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------

namespace {

class ColListItem : public QCheckListItem {
public:
	ColListItem ( CListView *lv, int col, const QString &text, bool onoff )
		: QCheckListItem ( lv, s_last, text, CheckBox ), m_col ( col )
	{
		setOn ( onoff );
		s_last = this;
	}
	~ColListItem ( )
	{
		s_last = 0;
	}

	int column ( ) const
	{
		return m_col;
	}

private:
	int m_col;
	static ColListItem *s_last;
};

ColListItem *ColListItem::s_last = 0;

} // namespace

CListViewColumnsDialog::CListViewColumnsDialog ( CListView *parent )
	: QDialog ( parent, "listview_configure_columns", true ), m_parent ( parent )
{
	setCaption ( tr( "Configure columns..." ));

	QPushButton *p;

	QBoxLayout *toplay = new QVBoxLayout ( this, 11, 6 );
	w_list = new CListView ( this );
	w_list-> addColumn ( QString ( ));
	w_list-> header ( )-> hide ( );
	w_list-> setSorting ( -1 );
	w_list-> setResizeMode ( QListView::LastColumn );


	QValueVector <int> indices ( m_parent-> columns ( ));

	for ( int i = 0; i < m_parent-> columns ( ); i++ )
		indices [m_parent-> header ( )-> mapToIndex ( i )] = i;

	for ( int i = 0; i < m_parent-> columns ( ); i++ ) {
		int col = indices [i];

		if ( m_parent-> m_columns [col]. m_available )
			(void) new ColListItem ( w_list, col, m_parent-> columnText ( col ), ( m_parent-> columnWidth ( col ) != 0 ));
	}
	QBoxLayout *horlay = new QHBoxLayout ( toplay );
	horlay-> addWidget ( w_list, 10 );

	QBoxLayout *rgtlay = new QVBoxLayout ( horlay );

	w_up = new QPushButton ( tr( "Move &up" ), this );
	connect ( w_up, SIGNAL( clicked ( )), this, SLOT( upCol ( )));
	rgtlay-> addWidget ( w_up );
	w_down = new QPushButton ( tr( "Move &down" ), this );
	connect ( w_down, SIGNAL( clicked ( )), this, SLOT( downCol ( )));
	rgtlay-> addWidget ( w_down );
	w_show = new QPushButton ( tr( "&Show" ), this );
	connect ( w_show, SIGNAL( clicked ( )), this, SLOT( showCol ( )));
	rgtlay-> addWidget ( w_show );
	w_hide = new QPushButton ( tr( "&Hide" ), this );
	connect ( w_hide, SIGNAL( clicked ( )), this, SLOT( hideCol ( )));
	rgtlay-> addWidget ( w_hide );

	rgtlay-> addStretch ( 10 );

	QFrame *line = new QFrame ( this );
	line-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	toplay-> addWidget ( line );

	QBoxLayout *botlay = new QHBoxLayout ( toplay );
	botlay-> addSpacing ( QFontMetrics ( font ( )). width ( "Aa0" ) * 6 );
	botlay-> addStretch ( 10 );
	p = new QPushButton ( tr( "&OK" ), this );
	p-> setDefault ( true );
	p-> setAutoDefault ( true );
	connect ( p, SIGNAL( clicked ( )), this, SLOT( accept ( )));
	botlay-> addWidget ( p );
	p = new QPushButton ( tr( "&Cancel" ), this );
	p-> setAutoDefault ( true );
	connect ( p, SIGNAL( clicked ( )), this, SLOT( reject ( )));
	botlay-> addWidget ( p );

	connect ( w_list, SIGNAL( currentChanged ( QListViewItem * )), this, SLOT( colSelected ( QListViewItem * )));
	w_list-> setCurrentItem ( w_list-> firstChild ( ));
	w_list-> setSelected ( w_list-> firstChild ( ), true );
	colSelected ( w_list-> currentItem ( ));

	setSizeGripEnabled ( true );
	setMinimumSize ( minimumSizeHint ( ));
}


void CListViewColumnsDialog::accept ( )
{
	int index = 0;
	QHeader *head = m_parent-> header ( );
	QValueList <int> collist;

	for ( QListViewItem *lvi = w_list-> firstChild ( ); lvi; lvi = lvi-> nextSibling ( ), index++ ) {
		ColListItem *cli = static_cast <ColListItem *> ( lvi );
		int col = cli-> column ( );

		if ( cli-> isOn ( ) != m_parent-> isColumnVisible ( col ))
			m_parent-> toggleColumn ( col );

		collist << col;
	}

	for ( uint i = 0; i < collist. count ( ); i++ )
		head-> moveSection ( collist [i], i );

	m_parent-> triggerUpdate ( );
	QDialog::accept ( );
}

void CListViewColumnsDialog::colSelected ( QListViewItem *lvi )
{
	ColListItem *cli = static_cast <ColListItem *> ( lvi );

	w_show-> setEnabled ( cli && !cli-> isOn ( ));
	w_hide-> setEnabled ( cli && cli-> isOn ( ));

	w_up-> setEnabled ( cli && cli-> itemAbove ( ));
	w_down-> setEnabled ( cli && cli-> itemBelow ( ));
}

void CListViewColumnsDialog::hideCol ( )
{
	ColListItem *cli = static_cast <ColListItem *> ( w_list->currentItem ( ));
	cli-> setOn ( false );
	colSelected ( cli );
}

void CListViewColumnsDialog::showCol ( )
{
	ColListItem *cli = static_cast <ColListItem *> ( w_list->currentItem ( ));
	cli-> setOn ( true );
	colSelected ( cli );
}

void CListViewColumnsDialog::upCol ( )
{
	ColListItem *cli = static_cast <ColListItem *> ( w_list->currentItem ( ));
	
	QListViewItem *above = cli-> itemAbove ( );
	if ( above-> itemAbove ( )) {
		cli-> moveItem ( above-> itemAbove ( ));
	}
	else { // moveItem ( 0 ); doesn't work !
		w_list-> takeItem ( cli );
		w_list-> insertItem ( cli );
		w_list-> setCurrentItem ( cli );
	}
	colSelected ( cli );
	w_list-> ensureItemVisible ( cli );
}

void CListViewColumnsDialog::downCol ( )
{
	ColListItem *cli = static_cast <ColListItem *> ( w_list->currentItem ( ));
	cli-> moveItem ( cli-> itemBelow ( ));
	colSelected ( cli );
	w_list-> ensureItemVisible ( cli );
}
