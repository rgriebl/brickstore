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

#include "cresource.h"
#include "cutility.h"

#include "cpicturewidget.h"

class CPictureWidgetPrivate {
public:
	BrickLink::Picture *m_pic;
	QLabel *            m_plabel;
	QLabel *            m_tlabel;
	QPopupMenu *        m_popup;
	QPtrList <QAction>  m_add_actions;
	bool                m_connected;
};

class CLargePictureWidgetPrivate {
public:
	BrickLink::Picture *m_pic;
	QPopupMenu *        m_popup;
};

CPictureWidget::CPictureWidget ( QWidget *parent, const char *name, WFlags fl )
	: QFrame ( parent, name, fl )
{
	d = new CPictureWidgetPrivate ( );

	d-> m_pic = 0;
	d-> m_connected = false;
	d-> m_popup = 0;

	setBackgroundMode ( Qt::PaletteBase );
	setSizePolicy ( QSizePolicy::Minimum, QSizePolicy::Minimum );

	int fw = frameWidth ( ) * 2;

	d-> m_plabel = new QLabel ( this );
	d-> m_plabel-> setFrameStyle ( QFrame::NoFrame );
	d-> m_plabel-> setAlignment ( Qt::AlignCenter );
	d-> m_plabel-> setBackgroundMode ( Qt::PaletteBase );
	d-> m_plabel-> setFixedSize ( 80, 80 );

	d-> m_tlabel = new QLabel ( "Ay<br />Ay<br />Ay<br />Ay<br />Ay", this );
	d-> m_tlabel-> setAlignment ( Qt::AlignCenter | Qt::WordBreak );
	d-> m_tlabel-> setBackgroundMode ( Qt::PaletteBase );
	d-> m_tlabel-> setFixedSize ( 2 * d-> m_plabel-> width ( ), d-> m_tlabel-> sizeHint ( ). height ( ));
	d-> m_tlabel-> setText ( "" );

	QBoxLayout *lay = new QVBoxLayout ( this, fw + 4, 4 );
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

void CPictureWidget::addActionsToContextMenu ( const QPtrList <QAction> &actions )
{
	d-> m_add_actions = actions;
	delete d-> m_popup;
	d-> m_popup = 0;
}

void CPictureWidget::mouseDoubleClickEvent ( QMouseEvent * )
{
	viewLargeImage ( );
}

void CPictureWidget::contextMenuEvent ( QContextMenuEvent *e )
{
	if ( d-> m_pic ) {
		if ( !d-> m_popup ) {
			d-> m_popup = new QPopupMenu ( this );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "reload" ), tr( "Update" ), this, SLOT( doUpdate ( )));
			d-> m_popup-> insertSeparator ( );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "viewmagp" ), tr( "View large image..." ), this, SLOT( viewLargeImage ( )));

			if ( !d-> m_add_actions. isEmpty ( )) {
				d-> m_popup-> insertSeparator ( );

				for ( QPtrListIterator <QAction> it ( d-> m_add_actions ); it. current ( ); ++it )
					it. current ( )-> addTo ( d-> m_popup );
			}
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
	: QLabel ( parent, "LargeImage", WType_Dialog | WShowModal | WStyle_Customize | WStyle_Dialog | WStyle_SysMenu | WStyle_Title | WStyle_Tool | WStyle_StaysOnTop | WDestructiveClose )
{
	d = new CLargePictureWidgetPrivate ( );

	setBackgroundMode ( Qt::PaletteBase );
	setFrameStyle ( QFrame::NoFrame );
	setAlignment ( Qt::AlignCenter );
//	setScaledContents ( true );
	setFixedSize ( 640, 480 );

	d-> m_pic = lpic;
	if ( d-> m_pic )
		d-> m_pic-> addRef ( );

	d-> m_popup = 0;

	connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( gotUpdate ( BrickLink::Picture * )));

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

void CLargePictureWidget::mouseDoubleClickEvent ( QMouseEvent * )
{
	close ( );
}

void CLargePictureWidget::contextMenuEvent ( QContextMenuEvent *e )
{
	if ( d-> m_pic ) {
		if ( !d-> m_popup ) {
			d-> m_popup = new QPopupMenu ( this );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "reload" ), tr( "Update" ), this, SLOT( doUpdate ( )));
			d-> m_popup-> insertSeparator ( );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "file_close" ), tr( "Close" ), this, SLOT( close ( )));
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


