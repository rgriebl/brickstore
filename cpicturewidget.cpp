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
#include <qlayout.h>
#include <qapplication.h>
#include <qaction.h>
#include <qtooltip.h>
//Added by qt3to4:
#include <QContextMenuEvent>
#include <Q3BoxLayout>
#include <Q3Frame>
#include <QPixmap>
#include <QMouseEvent>
#include <QKeyEvent>
#include <Q3VBoxLayout>

#include "cresource.h"
#include "cutility.h"

#include "cpicturewidget.h"

class CPictureWidgetPrivate {
public:
	BrickLink::Picture *m_pic;
	QLabel *            m_plabel;
	QLabel *            m_tlabel;
	Q3PopupMenu *        m_popup;
	bool                m_connected;
};

class CLargePictureWidgetPrivate {
public:
	BrickLink::Picture *m_pic;
	Q3PopupMenu *        m_popup;
};

CPictureWidget::CPictureWidget ( QWidget *parent, const char *name, Qt::WFlags fl )
	: Q3Frame ( parent, name, fl )
{
	d = new CPictureWidgetPrivate ( );

	d-> m_pic = 0;
	d-> m_connected = false;
	d-> m_popup = 0;

	setBackgroundMode ( Qt::PaletteBase );
	setSizePolicy ( QSizePolicy::Minimum, QSizePolicy::Minimum );

	int fw = frameWidth ( ) * 2;

	d-> m_plabel = new QLabel ( this );
	d-> m_plabel-> setFrameStyle ( Q3Frame::NoFrame );
    d-> m_plabel-> setAlignment ( Qt::AlignCenter );
	d-> m_plabel-> setBackgroundMode ( Qt::PaletteBase );
	d-> m_plabel-> setFixedSize ( 80, 80 );

	d-> m_tlabel = new QLabel ( "Ay<br />Ay<br />Ay<br />Ay<br />Ay", this );
    d-> m_tlabel-> setWordWrap( true );
    d-> m_tlabel-> setAlignment ( Qt::AlignCenter );
	d-> m_tlabel-> setBackgroundMode ( Qt::PaletteBase );
	d-> m_tlabel-> setFixedSize ( 2 * d-> m_plabel-> width ( ), d-> m_tlabel-> sizeHint ( ). height ( ));
	d-> m_tlabel-> setText ( "" );

	Q3BoxLayout *lay = new Q3VBoxLayout ( this, fw + 4, 4 );
	lay-> addWidget ( d-> m_plabel, 0, Qt::AlignCenter /*, AlignTop | AlignHCenter*/ );

	lay-> addWidget ( d-> m_tlabel, 1, Qt::AlignCenter /*, Qt::AlignBottom | Qt::AlignHCenter*/ );
	
	redraw ( );
}

void CPictureWidget::languageChange ( )
{
	delete d-> m_popup;
	d-> m_popup = 0;
}

CPictureWidget::~CPictureWidget ( )
{
	if ( d-> m_pic )
		d-> m_pic-> release ( );

	delete d;
}

void CPictureWidget::mouseDoubleClickEvent ( QMouseEvent * )
{
	viewLargeImage ( );
}

void CPictureWidget::contextMenuEvent ( QContextMenuEvent *e )
{
	if ( d-> m_pic ) {
		if ( !d-> m_popup ) {
			d-> m_popup = new Q3PopupMenu ( this );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "reload" ), tr( "Update" ), this, SLOT( doUpdate ( )));
			d-> m_popup-> insertSeparator ( );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "viewmagp" ), tr( "View large image..." ), this, SLOT( viewLargeImage ( )));
			d-> m_popup-> insertSeparator ( );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_catalog" ), tr( "Show BrickLink Catalog Info..." ), this, SLOT( showBLCatalogInfo ( )));
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_priceguide" ), tr( "Show BrickLink Price Guide Info..." ), this, SLOT( showBLPriceGuideInfo ( )));
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "edit_bl_lotsforsale" ), tr( "Show Lots for Sale on BrickLink..." ), this, SLOT( showBLLotsForSale ( )));
		}
		d-> m_popup-> popup ( e-> globalPos ( ));
	}

	e-> accept ( );
}

void CPictureWidget::doUpdate ( )
{
	if ( d-> m_pic ) {
		d-> m_pic-> update ( true );
		redraw ( );
	}
}

void CPictureWidget::viewLargeImage ( )
{
	if ( !d-> m_pic )
		return;

	BrickLink::Picture *lpic = BrickLink::inst ( )-> largePicture ( d-> m_pic-> item ( ), true );

	if ( lpic ) {
		CLargePictureWidget *l = new CLargePictureWidget ( lpic, qApp-> mainWidget ( ));
		l-> show ( );
		l-> raise ( );
		l-> setActiveWindow ( );
		l-> setFocus ( );
	}
}

void CPictureWidget::showBLCatalogInfo ( )
{
	if ( d-> m_pic && d-> m_pic-> item ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo, d-> m_pic-> item ( )));
}

void CPictureWidget::showBLPriceGuideInfo ( )
{
	if ( d-> m_pic && d-> m_pic-> item ( ) && d-> m_pic-> color ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, d-> m_pic-> item ( ), d-> m_pic-> color ( )));
}

void CPictureWidget::showBLLotsForSale ( )
{
	if ( d-> m_pic && d-> m_pic-> item ( ) && d-> m_pic-> color ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale, d-> m_pic-> item ( ), d-> m_pic-> color ( )));
}

void CPictureWidget::setPicture ( BrickLink::Picture *pic )
{
	if ( pic == d-> m_pic )
		return;

	if ( d-> m_pic )
		d-> m_pic-> release ( );
	d-> m_pic = pic;
	if ( d-> m_pic )
		d-> m_pic-> addRef ( );

	if ( !d-> m_connected && pic )
		d-> m_connected = connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( gotUpdate ( BrickLink::Picture * )));

	if ( pic )
		QToolTip::add ( this, tr( "Double-click to view the large image." ));
	else
		QToolTip::remove ( this );

	redraw ( );
}

BrickLink::Picture *CPictureWidget::picture ( ) const
{
	return d-> m_pic;
}

void CPictureWidget::gotUpdate ( BrickLink::Picture *pic )
{
	if ( pic == d-> m_pic )
		redraw ( );
}

void CPictureWidget::redraw ( )
{
	if ( d-> m_pic && ( d-> m_pic-> updateStatus ( ) == BrickLink::Updating )) {
		d-> m_tlabel-> setText ( tr( "Please wait ... updating" ));
		d-> m_plabel-> setPixmap ( QPixmap ( ));
	}
	else if ( d-> m_pic && d-> m_pic-> valid ( )) {
		d-> m_tlabel-> setText ( QString( "<b>" ) + d-> m_pic-> item ( )-> id ( ) + "</b>&nbsp; " + d-> m_pic-> item ( )-> name ( ));
		d-> m_plabel-> setPixmap ( d-> m_pic-> pixmap ( ));
	}
	else {
		d-> m_tlabel-> setText ( QString::null );
		d-> m_plabel-> setPixmap ( QPixmap ( ));
	}
}


// -------------------------------------------------------------------------


CLargePictureWidget::CLargePictureWidget ( BrickLink::Picture *lpic, QWidget *parent )
	: QLabel ( parent, "LargeImage", Qt::WType_Dialog | Qt::WShowModal | Qt::WStyle_Customize | Qt::WType_Dialog | Qt::WStyle_SysMenu | Qt::WStyle_Title | Qt::WStyle_Tool | Qt::WStyle_StaysOnTop | Qt::WDestructiveClose )
{
	d = new CLargePictureWidgetPrivate ( );

	setBackgroundMode ( Qt::PaletteBase );
	setFrameStyle ( Q3Frame::NoFrame );
	setAlignment ( Qt::AlignCenter );
//	setScaledContents ( true );
	setFixedSize ( 640, 480 );

	d-> m_pic = lpic;
	if ( d-> m_pic )
		d-> m_pic-> addRef ( );

	d-> m_popup = 0;

	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( gotUpdate ( BrickLink::Picture * )));

	QToolTip::add ( this, tr( "Double-click to close this window." ));
	redraw ( );
}

void CLargePictureWidget::languageChange ( )
{
	delete d-> m_popup;
	d-> m_popup = 0;
}

CLargePictureWidget::~CLargePictureWidget ( )
{
	if ( d-> m_pic )
		d-> m_pic-> release ( );
	delete d;
}

void CLargePictureWidget::gotUpdate ( BrickLink::Picture *pic )
{
	if ( pic == d-> m_pic )
		redraw ( );
}

void CLargePictureWidget::redraw ( )
{
	if ( d-> m_pic ) {
		setCaption ( QString ( d-> m_pic-> item ( )-> id ( )) + " " + d-> m_pic-> item ( )-> name ( ));

		if ( d-> m_pic-> updateStatus ( ) == BrickLink::Updating )
			setText ( tr( "Please wait ... updating" ));
		else if ( d-> m_pic-> valid ( ))
			setPixmap ( d-> m_pic-> pixmap ( ));
		else
			setText ( QString::null );
	}
}

void CLargePictureWidget::mouseDoubleClickEvent ( QMouseEvent *e )
{
	e-> accept ( );
	close ( );
}

void CLargePictureWidget::keyPressEvent ( QKeyEvent *e )
{
	if ( e-> key ( ) == Qt::Key_Escape || e-> key ( ) == Qt::Key_Return ) {
		e-> accept ( );
		close ( );
	}
}

void CLargePictureWidget::contextMenuEvent ( QContextMenuEvent *e )
{
	if ( d-> m_pic ) {
		if ( !d-> m_popup ) {
			d-> m_popup = new Q3PopupMenu ( this );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "reload" ), tr( "Update" ), this, SLOT( doUpdate ( )));
			d-> m_popup-> insertSeparator ( );
            d-> m_popup-> insertItem ( CResource::inst ( )-> icon ( "file_close" ), tr( "Close" ), this, SLOT( close ( )));
		}
		d-> m_popup-> popup ( e-> globalPos ( ));
	}
	e-> accept ( );
}

void CLargePictureWidget::doUpdate ( )
{
	if ( d-> m_pic )
		d-> m_pic-> update ( true );
	redraw ( );
}


