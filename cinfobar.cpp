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
#include <qlayout.h>
#include <qiconset.h>
#include <qdockarea.h>
#include <qmainwindow.h>
#include <qlabel.h>
#include <qwidgetstack.h>

#include "cpriceguidewidget.h"
#include "cpicturewidget.h"
#include "ctaskbar.h"
#include "cresource.h"
#include "cutility.h"
#include "curllabel.h"

#include "cinfobar.h"


class CInfoBarPrivate {
public:
	CPriceGuideWidget *m_pg_info;
	CPictureWidget *   m_pic_info;
	CUrlLabel *        m_link_info;
	QLabel *           m_text_info;
	QWidgetStack *     m_info_stack;
	CTaskBar *         m_taskbar;
	CInfoBar::Look     m_look;
	bool               m_look_set;
};


CInfoBar::CInfoBar ( const QString &title, QWidget *parent, const char *name )
	: QDockWindow ( parent, name )
{
	d = new CInfoBarPrivate;

	setCaption ( title );
	setResizeEnabled ( false );

	d-> m_info_stack = new QWidgetStack ( this );
	d-> m_pic_info = new CPictureWidget ( d-> m_info_stack );
	d-> m_info_stack-> addWidget ( d-> m_pic_info );
	d-> m_text_info = new QLabel ( d-> m_info_stack );
	d-> m_info_stack-> addWidget ( d-> m_text_info );
	d-> m_text_info-> setAlignment ( Qt::AlignLeft | Qt::AlignTop );
	d-> m_text_info-> setIndent ( 8 );
	
	paletteChange ( palette ( ));

	d-> m_pg_info = new CPriceGuideWidget ( this );
	d-> m_pg_info-> setLayout ( CPriceGuideWidget::Horizontal );
	connect ( d-> m_pg_info, SIGNAL( priceDoubleClicked ( money_t )), this, SIGNAL ( priceDoubleClicked ( money_t )));

	d-> m_taskbar = new CTaskBar ( this, "taskbar" );

	d-> m_link_info = new CUrlLabel ( this );
	d-> m_link_info-> unsetPalette ( );
	d-> m_link_info-> setText ( "<b>B</b><br />1<br />2<br />3<br />4<br /><br/><b>P</b><br />1<br />" );
	d-> m_link_info-> setMinimumHeight ( d-> m_link_info-> sizeHint ( ). height ( ));
	d-> m_link_info-> setText ( "" );

	d-> m_look_set = false;
	d-> m_look = Classic;
	
	setLook ( Classic );
}

void CInfoBar::paletteChange ( const QPalette &oldpal )
{
	QPixmap pix;
	pix. convertFromImage ( CUtility::shadeImage ( CResource::inst ( )-> pixmap( "bg_infotext" ). convertToImage ( ), 
		                                            palette ( ). active ( ). base ( )));

	d-> m_text_info-> setBackgroundPixmap ( pix );

	QDockWindow::paletteChange ( oldpal );
}


void CInfoBar::setLook ( int il )
{
	Look l = Look( il );

	if ( !d-> m_look_set || ( d-> m_look != l )) {
		if ( l == Classic ) {
			boxLayout ( )-> remove ( d-> m_taskbar );
			d-> m_taskbar-> hide ( );

			setMovingEnabled ( true );
			setCloseMode ( QDockWindow::Always );
			setVerticallyStretchable ( false );

			d-> m_taskbar-> removeItem ( 2, false );
			d-> m_taskbar-> removeItem ( 1, false );
			d-> m_taskbar-> removeItem ( 0, false );

			d-> m_info_stack-> reparent ( this, 0, QPoint ( 0, 0 ), true );
			d-> m_pg_info-> reparent ( this, 0, QPoint ( 0, 0 ), true );
			d-> m_link_info-> reparent ( this, 0, QPoint ( 0, 0 ), true );
			
			d-> m_info_stack-> setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
			d-> m_info_stack-> setLineWidth ( 2 );
			d-> m_pg_info-> setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
			d-> m_pg_info-> setLineWidth ( 2 );
			d-> m_link_info-> setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
			d-> m_link_info-> setLineWidth ( 2 );

			boxLayout ( )-> setMargin ( 1 );
			boxLayout ( )-> setSpacing ( 3 );
			boxLayout ( )-> addWidget ( d-> m_info_stack );
			boxLayout ( )-> addWidget ( d-> m_pg_info );
			boxLayout ( )-> addWidget ( d-> m_link_info );
		}
		else {
			QMainWindow *mw = ::qt_cast<QMainWindow *> ( area ( ) ? area ( )-> parentWidget ( ) : parentWidget ( ));
			if ( mw )
				mw-> moveDockWindow ( this, l == ModernRight ? QMainWindow::Right : QMainWindow::Left );

			if ( d-> m_look == Classic ) {
				setMovingEnabled ( false );
				setCloseMode ( QDockWindow::Never );
				setVerticallyStretchable ( true );

				d-> m_info_stack-> setFrameStyle( QFrame::NoFrame );
				d-> m_pg_info-> setFrameStyle( QFrame::NoFrame );
				d-> m_link_info-> setFrameStyle( QFrame::NoFrame );

				boxLayout ( )-> setMargin ( 0 );
				boxLayout ( )-> setSpacing ( 0 );
				boxLayout ( )-> remove ( d-> m_info_stack );
				boxLayout ( )-> remove ( d-> m_pg_info );
				boxLayout ( )-> remove ( d-> m_link_info );
				boxLayout ( )-> addWidget ( d-> m_taskbar );

				d-> m_taskbar-> addItem ( d-> m_info_stack, CResource::inst ( )-> pixmap ( "22x22/edit_bl_catalog" ), tr( "Info" ), true, false );
				d-> m_taskbar-> addItem ( d-> m_pg_info, CResource::inst ( )-> pixmap ( "22x22/edit_bl_priceguide" ), tr( "Price Guide" ), true, false );
				d-> m_taskbar-> addItem ( d-> m_link_info, CResource::inst ( )-> pixmap ( "22x22/browser" ), tr( "Links" ), true, false );

				d-> m_taskbar-> show ( );
			}
		}

		d-> m_look_set = true;
		d-> m_look = l;
	}
}

CInfoBar::~CInfoBar ( )
{
	delete d;
}

void CInfoBar::setOrientation ( Orientation o )
{
	QDockWindow::setOrientation ( o );

	d-> m_pg_info-> setLayout ( o == Qt::Horizontal ? CPriceGuideWidget::Horizontal : CPriceGuideWidget::Vertical );
}

void CInfoBar::setPriceGuide ( BrickLink::PriceGuide *pg )
{
	if ( d-> m_pg_info-> priceGuide ( ) != pg )
		d-> m_pg_info-> setPriceGuide ( pg );
}

void CInfoBar::setPicture ( BrickLink::Picture *pic )
{
	if ( d-> m_pic_info-> picture ( ) != pic )
		d-> m_pic_info-> setPicture ( pic );
	d-> m_info_stack-> raiseWidget ( d-> m_pic_info );

	QString str;

	if ( pic && pic-> item ( )) {
		const BrickLink::Item *item = pic-> item ( );
		const BrickLink::Color *color = pic-> color ( );

		QString fmt1 = "&nbsp;&nbsp;<b>%1</b><br />";
		QString fmt2 = "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"%2\">%1...</a><br />";
		
		str += fmt1. arg( tr( "BrickLink" ));
		str += fmt2. arg( tr( "Catalog" )).         arg( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo,    item, color ));
		str += fmt2. arg( tr( "Price Guide" )).     arg( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, item, color ));
		str += fmt2. arg( tr( "Lots for Sale" )).   arg( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale,    item, color ));
		str += fmt2. arg( tr( "Appears in Sets" )). arg( BrickLink::inst ( )-> url ( BrickLink::URL_AppearsInSets,  item, color ));
		str += "<br />";

		str += fmt1. arg( tr( "Peeron" ));
		str += fmt2. arg( tr( "Infomation" )). arg( BrickLink::inst ( )-> url ( BrickLink::URL_PeeronInfo, item, color ));
	}
	d-> m_link_info-> setText ( str );
}

void CInfoBar::setInfoText ( const QString &text )
{
	if ( d-> m_pic_info-> picture ( ))
		d-> m_pic_info-> setPicture ( 0 );
	d-> m_text_info-> setText ( text );
	d-> m_info_stack-> raiseWidget ( d-> m_text_info );

	d-> m_link_info-> setText ( QString::null );
}

