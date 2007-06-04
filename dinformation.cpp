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
#include <QPushButton>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>

#include "dinformation.h"

DInformation::DInformation ( const QString &title, const QMap<QString, QString> &pages, bool delayok, QWidget *parent, Qt::WindowFlags f )
	: QDialog ( parent, f ), m_pages ( pages )
{
	setupUi ( this );
	setWindowTitle ( title );

	w_browser-> setMaximumWidth ( 500 );

	gotoPage ( "index" );
	connect ( w_browser, SIGNAL( linkActivated ( const QString & )), this, SLOT( gotoPage ( const QString & )));

	if ( delayok ) {
		w_buttons-> button ( QDialogButtonBox::Ok )-> setEnabled ( false );
		QTimer::singleShot ( 3 * 1000, this, SLOT( enableOk ( )));
	}

	setFixedSize ( sizeHint ( ));
}

void DInformation::changeEvent ( QEvent *e )
{
	if ( e-> type ( ) == QEvent::LanguageChange )
		retranslateUi ( this );
	else
		QDialog::changeEvent ( e );
}

void DInformation::enableOk ( )
{
	w_buttons-> button ( QDialogButtonBox::Ok )-> setEnabled ( true );
}

void DInformation::reject ( )
{
	if ( w_buttons-> button ( QDialogButtonBox::Ok )-> isEnabled ( ))
		QDialog::reject ( );
}

void DInformation::closeEvent ( QCloseEvent *ce )
{
	ce-> setAccepted ( w_buttons-> button ( QDialogButtonBox::Ok )-> isEnabled ( ));
}

void DInformation::gotoPage ( const QString &url )
{
	qWarning ( "URL: %s", qPrintable ( url ));

	if ( m_pages. contains ( url )) {
		w_browser-> setText ( m_pages [url] );
	//	w_browser->document()->setTextWidth(500);
		//w_browser->document()->adjustSize();
//		w_browser->setFixedSize(w_browser->document()->size().toSize());
		layout()->activate();
	}
	else {
		QDesktopServices::openUrl ( url );
	}
}
