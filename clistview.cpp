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

#include "cutility.h"
#include "clistview.h"


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

	m_header_popup = new QPopupMenu ( this );
	m_header_popup-> setCheckable ( true );
	connect ( m_header_popup, SIGNAL( activated ( int )), this, SLOT( toggleColumn ( int )));

	m_use_header_popup = false;

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

void CListView::saveSettings ( QSettings *set, const QString &grp ) const
{
	set-> beginGroup ( grp );

	QHeader *head = header ( );
	QStringList wl, hl, ol;

	for ( int i = 0; i < columns ( ); i++ ) {
		wl << QString::number ( columnWidth ( i ));
		hl << QString::number ( m_col_width [i] );
		ol << QString::number ( head-> mapToIndex ( i ));
	}

	set-> writeEntry ( "/ColumnWidths",  wl );
	set-> writeEntry ( "/ColumnWidthsHidden", hl );
	set-> writeEntry ( "/ColumnOrder",   ol );
	set-> writeEntry ( "/SortColumn",    sortColumn ( ));
	set-> writeEntry ( "/SortAscending", ( sortOrder ( ) == Ascending ));

	set-> endGroup ( );
}

void CListView::loadSettings ( QSettings *set, const QString &grp )
{
	set-> beginGroup ( grp );

	QHeader *head = header ( );
	bool ok = false;

	QStringList sl = set-> readListEntry ( "ColumnWidths", &ok );

	if ( ok ) {
		int i = 0;
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it, i++ )
			setColumnWidth ( i, ( *it ). toInt ( ));
	}
	
	sl = set-> readListEntry ( "ColumnWidthsHidden", &ok );

	if ( ok ) {
		int i = 0;
		for ( QStringList::Iterator it = sl. begin ( ); it != sl. end ( ); ++it, i++ )
			m_col_width [i] = ( *it ). toInt ( );
	}

	sl = set-> readListEntry ( "ColumnOrder", &ok );

	if ( ok ) {
		for ( int i = 0; i < columns ( ); i++ ) {
			int j = sl. findIndex ( QString::number ( i ));

			if ( j >= 0 )
				head-> moveSection ( j, i );
		}
	}

	int sortcol = set-> readNumEntry ( "SortColumn", -1, &ok );

	if ( ok )
		setSorting ( sortcol, set-> readBoolEntry ( "SortAscending", true ));

	set-> endGroup ( );
}

void CListView::setColumnsHideable ( bool b )
{
	m_use_header_popup = b;
}

bool CListView::columnsHideable ( ) const
{
	return m_use_header_popup;
}

bool CListView::isColumnVisible ( int i ) const
{
	return ( i < columns ( )) && ( columnWidth ( i ) > 0 );
}

void CListView::hideColumn ( int i )
{
	hideColumn ( i, false );
}

void CListView::hideColumn ( int i, bool completly )
{
	if ( i < columns ( )) {
		if ( columnWidth ( i ) != 0 )
			toggleColumn ( i );

		if ( completly )
			m_completly_hidden [i] = true;
	}
}

void CListView::showColumn ( int i )
{
	if ( i < columns ( )) {
		m_completly_hidden. remove ( i );

		if ( columnWidth ( i ) == 0 )
			toggleColumn ( i );
	}
}

void CListView::toggleColumn ( int i )
{
	//qDebug ( "TC: %d (w: %d)", i, columnWidth ( i ));

	if ( i < columns ( )) {
		if ( columnWidth ( i ) == 0 ) {
			int w = m_col_width [i];
			setColumnWidth ( i, w == 0 ? 30 : w );
		}
		else {
			m_col_width [i] = columnWidth ( i );
			setColumnWidth ( i, 0 );
		}
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

bool CListView::eventFilter ( QObject *o, QEvent *e )
{
	bool res = false;

	if ( o == header ( ) && m_use_header_popup ) {
		if (( e-> type ( ) == QEvent::MouseButtonPress ) && ( static_cast <QMouseEvent *> ( e )-> button ( ) == RightButton )) {
			m_header_popup-> clear ( );

			for ( int i = 0; i < columns ( ); i++ ) {
				if ( !m_completly_hidden. contains ( i )) {
					m_header_popup-> insertItem ( columnText ( i ), i );
					m_header_popup-> setItemChecked ( i, columnWidth ( i ) != 0 );
				}
			}

			m_header_popup-> popup ( static_cast<QMouseEvent *> ( e )-> globalPos ( ));
			res = true;
		}
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
