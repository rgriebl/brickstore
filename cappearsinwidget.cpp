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
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qlayout.h>
#include <qapplication.h>
#include <qaction.h>
#include <qtooltip.h>
#include <qheader.h>

#include "cframework.h"
#include "cresource.h"
#include "cutility.h"

#include "cappearsinwidget.h"


namespace {

class AppearsInListItem : public CListViewItem {
public:
	AppearsInListItem ( CListView *lv, int qty, const BrickLink::Item *item )
		: CListViewItem ( lv ), m_qty ( qty ), m_item ( item ), m_picture ( 0 )
	{ 
	}

	virtual ~AppearsInListItem ( )
	{
		if ( m_picture )
			m_picture-> release ( );
	}

	virtual QString text ( int col ) const
	{ 
		switch ( col ) {
			case  0: return QString::number ( m_qty );
			case  1: return m_item-> id ( );
			case  2: return m_item-> name ( ); 
			default: return QString ( );
		}
	}

	QString toolTip ( ) const
	{
		QString str = "<table><tr><td rowspan=\"2\">%1</td><td><b>%2</b></td></tr><tr><td>%3</td></tr></table>";
		QString left_cell;

		if ( !m_picture ) {
			m_picture = BrickLink::inst ( )-> picture ( m_item, m_item-> defaultColor ( ), true );

			if ( m_picture )
				m_picture-> addRef ( );
		}

		if ( m_picture && m_picture-> valid ( )) {
			QMimeSourceFactory::defaultFactory ( )-> setPixmap ( "appears_in_set_tooltip_picture", m_picture-> pixmap ( ));
			
			left_cell = "<img src=\"appears_in_set_tooltip_picture\" />";
		}
		else if ( m_picture && ( m_picture-> updateStatus ( ) == BrickLink::Updating )) {
			left_cell = "<i>" + CAppearsInWidget::tr( "[Image is loading]" ) + "</i>";
		}

		return str. arg( left_cell ). arg( m_item-> id ( )). arg( m_item-> name ( ));
	}

	virtual int compare ( QListViewItem *i, int col, bool ascending ) const
	{
		if ( col == 0 )
			return ( m_qty - static_cast <AppearsInListItem *> ( i )-> m_qty );
		else
			return CListViewItem::compare ( i, col, ascending );
	}

	const BrickLink::Item *item ( ) const
	{ return m_item; }

private:
	int                         m_qty;
	const BrickLink::Item *     m_item;
	mutable BrickLink::Picture *m_picture;
};


class AppearsInToolTip : public QToolTip {
public:
	AppearsInToolTip ( QWidget *parent, CAppearsInWidget *aiw )
		: QToolTip( parent ), m_aiw ( aiw )
	{ }
	
	virtual ~AppearsInToolTip ( )
	{ }

    void maybeTip ( const QPoint &pos )
	{
		if ( !parentWidget ( ) || !m_aiw /*|| !m_aiw-> showToolTips ( )*/ )
			return;

		AppearsInListItem *item = static_cast <AppearsInListItem *> ( m_aiw-> itemAt ( pos ));
		
		if ( item )
			tip ( m_aiw-> itemRect ( item ), item-> toolTip ( ));
	}

private:
    CAppearsInWidget *m_aiw;
};


} // namespace


class CAppearsInWidgetPrivate {
public:
	const BrickLink::Item * m_item;
	const BrickLink::Color *m_color;
	QPopupMenu *            m_popup;
	QPtrList <QAction>      m_add_actions;
	QToolTip *              m_tooltips;
};

CAppearsInWidget::CAppearsInWidget ( QWidget *parent, const char *name, WFlags /*fl*/ )
	: CListView ( parent, name )
{
	d = new CAppearsInWidgetPrivate ( );

	d-> m_item = 0;
	d-> m_color = 0;
	d-> m_popup = 0;

	setShowSortIndicator ( true );
	setAlwaysShowSelection ( true );
	header ( )-> setMovingEnabled ( false );
	header ( )-> setResizeEnabled ( false );
	addColumn ( QString ( ));
	addColumn ( QString ( ));
	addColumn ( QString ( ));
	setResizeMode ( QListView::LastColumn );

	d-> m_tooltips = new AppearsInToolTip ( viewport ( ), this );

	connect ( this, SIGNAL( contextMenuRequested ( QListViewItem *, const QPoint &, int )), this, SLOT( showContextMenu ( QListViewItem *, const QPoint & )));

	languageChange ( );
}

void CAppearsInWidget::languageChange ( )
{
	setColumnText ( 0, tr( "#" ));
	setColumnText ( 1, tr( "Set" ));
	setColumnText ( 2, tr( "Name" ));
}

CAppearsInWidget::~CAppearsInWidget ( )
{
	delete d-> m_tooltips;
	delete d;
}

void CAppearsInWidget::addActionsToContextMenu ( const QPtrList <QAction> &actions )
{
	d-> m_add_actions = actions;
	delete d-> m_popup;
	d-> m_popup = 0;
}

void CAppearsInWidget::showContextMenu ( QListViewItem *lvitem, const QPoint &pos )
{
	if ( d-> m_item && lvitem ) {
		if ( lvitem != currentItem ( ))
			setCurrentItem ( lvitem );

		if ( !d-> m_popup ) {
			d-> m_popup = new QPopupMenu ( this );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "edit_partoutitems" ), tr( "Part out Item..." ), this, SLOT( partOut ( )));

			if ( !d-> m_add_actions. isEmpty ( )) {
				d-> m_popup-> insertSeparator ( );

				for ( QPtrListIterator <QAction> it ( d-> m_add_actions ); it. current ( ); ++it )
					it. current ( )-> addTo ( d-> m_popup );
			}
		}
		d-> m_popup-> popup ( pos );
	}
}

void CAppearsInWidget::partOut ( )
{
	AppearsInListItem *item = static_cast <AppearsInListItem *> ( currentItem ( ));

	if ( item && item-> item ( ))
		CFrameWork::inst ( )-> fileImportBrickLinkInventory ( item-> item ( ));
}

QSize CAppearsInWidget::minimumSizeHint ( ) const
{
	const QFontMetrics &fm = fontMetrics ( );
	
	return QSize ( fm. width ( 'm' ) * 20, fm.height ( ) * 6 );
}

QSize CAppearsInWidget::sizeHint ( ) const
{
	return minimumSizeHint ( ) * 2;
}

void CAppearsInWidget::setItem ( const BrickLink::Item *item, const BrickLink::Color *color )
{
	clear ( );
	d-> m_item = item;
	d-> m_color = color;

	if ( item ) {
		BrickLink::Item::AppearsInMap map = item-> appearsIn ( color );

		for ( BrickLink::Item::AppearsInMap::const_iterator itc = map. begin ( ); itc != map. end ( ); ++itc ) {
			for ( BrickLink::Item::AppearsInMapVector::const_iterator itv = itc. data ( ). begin ( ); itv != itc. data ( ). end ( ); ++itv ) {
				(void) new AppearsInListItem ( this, itv-> first, itv-> second );
			}
		}
	}
}

