/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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
#include <q3popupmenu.h>
#include <Q3MimeSourceFactory>
#include <qlayout.h>
#include <qapplication.h>
#include <qaction.h>
#include <qtooltip.h>
#include <q3header.h>
#include <qcursor.h>

#include "cframework.h"
#include "cresource.h"
#include "cutility.h"
#include "cpicturewidget.h"

#include "cappearsinwidget.h"

class CAppearsInWidgetPrivate {
public:
	Q3PopupMenu *           m_popup;
    AppearsInToolTip *      m_tip;
};

CAppearsInWidget::CAppearsInWidget ( QWidget *parent, const char *name, Qt::WFlags /*fl*/ )
	: CListView ( parent, name )
{
	d = new CAppearsInWidgetPrivate ( );
	d-> m_popup = 0;

	setShowSortIndicator ( false );
	setAlwaysShowSelection ( true );
	addColumn ( QString ( ));
	addColumn ( QString ( ));
	addColumn ( QString ( ));
	setResizeMode ( Q3ListView::LastColumn );
	header ( )-> setMovingEnabled ( false );

    d->m_tip = new AppearsInToolTip(viewport(), this );
    viewport()->installEventFilter(d->m_tip);

	connect ( this, SIGNAL( contextMenuRequested ( Q3ListViewItem *, const QPoint &, int )), this, SLOT( showContextMenu ( Q3ListViewItem *, const QPoint & )));
	connect ( this, SIGNAL( returnPressed ( Q3ListViewItem * )), this, SLOT( partOut ( )));

	languageChange ( );
}

void CAppearsInWidget::languageChange ( )
{
	setColumnText ( 0, tr( "Qty." ));
	setColumnText ( 1, tr( "Set" ));
	setColumnText ( 2, tr( "Name" ));
}

CAppearsInWidget::~CAppearsInWidget ( )
{
    viewport()->removeEventFilter(d->m_tip);
	delete d;
}

void CAppearsInWidget::contentsMouseDoubleClickEvent (QMouseEvent *e) {
    partOut();
    e->accept();
}

void CAppearsInWidget::showContextMenu ( Q3ListViewItem *lvitem, const QPoint &pos )
{
	if ( /*d-> m_item &&*/ lvitem ) {
		if ( lvitem != currentItem ( ))
			setCurrentItem ( lvitem );

		if ( !d-> m_popup ) {
			d-> m_popup = new Q3PopupMenu ( this );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_partoutitems" ), tr( "Part out Item..." ), this, SLOT( partOut ( )));
			d-> m_popup-> insertSeparator ( );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "viewmagp" ), tr( "View large image..." ), this, SLOT( viewLargeImage ( )));
			d-> m_popup-> insertSeparator ( );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_catalog" ), tr( "Show BrickLink Catalog Info..." ), this, SLOT( showBLCatalogInfo ( )));
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_priceguide" ), tr( "Show BrickLink Price Guide Info..." ), this, SLOT( showBLPriceGuideInfo ( )));
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_lotsforsale" ), tr( "Show Lots for Sale on BrickLink..." ), this, SLOT( showBLLotsForSale ( )));
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

	if ( item ) {
		BrickLink::Item::AppearsInMap map = item-> appearsIn ( color );

		for ( BrickLink::Item::AppearsInMap::const_iterator itc = map. begin ( ); itc != map. end ( ); ++itc ) {
			for ( BrickLink::Item::AppearsInMapVector::const_iterator itv = itc. data ( ). begin ( ); itv != itc. data ( ). end ( ); ++itv ) {
				(void) new AppearsInListItem ( this, itv-> first, itv-> second );
			}
		}
	}
}

void CAppearsInWidget::setItem ( const BrickLink::InvItemList &list )
{
	clear ( );

	if ( !list. isEmpty ( )) {
        QMap<const BrickLink::Item *, int> unique;
        bool first_item = true;

        foreach ( const BrickLink::InvItem *it, list ) {
		    BrickLink::Item::AppearsInMap map = it-> item ( )-> appearsIn ( it-> color ( ));

		    for ( BrickLink::Item::AppearsInMap::const_iterator itc = map. begin ( ); itc != map. end ( ); ++itc ) {
			    for ( BrickLink::Item::AppearsInMapVector::const_iterator itv = itc. data ( ). begin ( ); itv != itc. data ( ). end ( ); ++itv ) {
                    QMap<const BrickLink::Item *, int>::iterator unique_it = unique. find ( itv-> second );
                    if ( unique_it != unique. end ( ))
                        ++unique_it. data ( );
                    else if ( first_item )
                        unique. insert ( itv-> second, 1 );
			    }
		    }
            first_item = false;
	    }
        for ( QMap<const BrickLink::Item *, int>::iterator unique_it = unique. begin ( ); unique_it != unique. end ( ); ++unique_it ) {
            if ( unique_it. data ( ) == list. count ( ))
        	    (void) new AppearsInListItem ( this, -1, unique_it. key ( ));
        }
    }
}

void CAppearsInWidget::viewLargeImage ( )
{
	AppearsInListItem *item = static_cast <AppearsInListItem *> ( currentItem ( ));

	if ( !item || !item-> item ( ))
		return;

	BrickLink::Picture *lpic = BrickLink::inst ( )-> largePicture ( item-> item ( ), true );

	if ( lpic ) {
		CLargePictureWidget *l = new CLargePictureWidget ( lpic, qApp-> mainWidget ( ));
		l-> show ( );
		l-> raise ( );
		l-> setActiveWindow ( );
		l-> setFocus ( );
	}
}

void CAppearsInWidget::showBLCatalogInfo ( )
{
	AppearsInListItem *item = static_cast <AppearsInListItem *> ( currentItem ( ));

	if ( item && item-> item ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo, item-> item ( )));
}

void CAppearsInWidget::showBLPriceGuideInfo ( )
{
	AppearsInListItem *item = static_cast <AppearsInListItem *> ( currentItem ( ));

	if ( item && item-> item ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, item-> item ( ), BrickLink::inst ( )-> color ( 0 )));
}

void CAppearsInWidget::showBLLotsForSale ( )
{
	AppearsInListItem *item = static_cast <AppearsInListItem *> ( currentItem ( ));

	if ( item && item-> item ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale, item-> item ( ), BrickLink::inst ( )-> color ( 0 )));
}
