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
#include <qpushbutton.h>
#include <qlabel.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qfile.h>

#include "capplication.h"
#include "curllabel.h"
#include "cconfig.h"

#include "cprogressdialog.h"

CProgressDialog::CProgressDialog ( QWidget *parent, const char *name )
	: QDialog ( parent, name, true )
{
	m_has_errors = false;
	m_autoclose = true;
	m_job = 0;
	m_trans = 0;

	m_message_progress = false;

	setCaption ( cApp-> appName ( ));

	int minwidth = fontMetrics ( ). width ( 'm' ) * 40;

	QVBoxLayout *lay = new QVBoxLayout ( this, 11, 6 );

	m_header = new QLabel ( this );
	m_header-> setAlignment ( Qt::AlignLeft | Qt::AlignVCenter );
	m_header->setMinimumWidth ( minwidth );
	lay-> addWidget ( m_header );

	QFrame *frame = new QFrame ( this );
	frame-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	lay-> addWidget ( frame );

	m_message = new CUrlLabel ( this );
	m_message-> setAlignment ( Qt::AlignCenter );
	lay-> addWidget ( m_message );

	m_progress_container = new QWidget ( this );
	lay-> addWidget ( m_progress_container );
	QHBoxLayout *play = new QHBoxLayout ( m_progress_container, 0, 6 );

	m_progress = new QProgressBar ( m_progress_container );
	m_progress-> setPercentageVisible ( false );
	play-> addSpacing ( 20 );
	play-> addWidget ( m_progress );
	play-> addSpacing ( 20 );

	frame = new QFrame ( this );
	frame-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	lay-> addWidget ( frame );

	QHBoxLayout *blay = new QHBoxLayout ( lay );
	blay-> addSpacing ( 11 );
	blay-> addStretch ( 10 );
	m_ok = new QPushButton ( tr( "&OK" ), this );
	blay-> addWidget ( m_ok );
	m_ok-> hide ( );
	m_cancel = new QPushButton ( tr( "&Cancel" ), this );
	blay-> addWidget ( m_cancel );
	blay-> addStretch ( 10 );
	blay-> addSpacing ( 11 );
	connect ( m_cancel, SIGNAL( clicked ( )), this, SLOT( reject ( )));

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));
	m_override = true;
}

CProgressDialog::~CProgressDialog ( )
{
	delete m_trans;
}

void CProgressDialog::done ( int r )
{
	if ( m_trans )
		m_trans-> cancelAllJobs ( );

	QDialog::done ( r );

	if ( m_override )
		QApplication::restoreOverrideCursor ( );
}

void CProgressDialog::setHeaderText ( const QString &str )
{
	m_header-> setText ( QString( "<b>%1</b>" ). arg( str ));
	
	layout ( );
	m_header-> repaint ( );
}

void CProgressDialog::setMessageText ( const QString &str )
{
	m_message_text = str;
	m_message_progress = ( str. contains ( "%1" ) && str. contains ( "%2" ));

	if ( m_message_progress )
		setProgress ( 0, 0 );
	else
		m_message-> setText ( str );
	
	layout ( );
	m_message-> repaint ( );
}

void CProgressDialog::setErrorText ( const QString &str )
{
	m_message-> setText ( QString( "<b>%1</b>: %2" ). arg( tr( "Error" )). arg( str ));
	setFinished ( false );

	layout ( );
	m_message-> repaint ( );
}

void CProgressDialog::setFinished ( bool ok )
{
	QApplication::restoreOverrideCursor ( );
	m_override = false;

	m_has_errors = !ok;

	if ( m_autoclose && ok ) {
		accept ( );
	}
	else {
		m_cancel-> hide ( );
		m_ok-> show ( );
		connect ( m_ok, SIGNAL( clicked ( )), this, ok ? SLOT( accept ( )) : SLOT( reject ( )));
		layout ( );
	}
}

void CProgressDialog::setProgress ( int s, int t )
{
	m_progress-> setProgress ( s, t );

	if ( m_message_progress )
		m_message-> setText ( m_message_text. arg( s ). arg( t ));
}

void CProgressDialog::setProgressVisible ( bool b )
{
	if ( b )
		m_progress_container-> show ( );
	else
		m_progress_container-> hide ( );
}

CTransfer::Job *CProgressDialog::job ( ) const
{
	return m_job;
}

void CProgressDialog::transferProgress ( CTransfer::Job *j, int s, int t )
{
	if ( j && ( j == m_job ))
		setProgress ( s / 1024, t / 1024 );
}

void CProgressDialog::transferDone ( CTransfer::Job *j )
{
	if ( !j || ( j != m_job ))
		return;

	if ( j-> file ( ) && j-> file ( )-> isOpen ( ))
		j-> file ( )-> close ( );

	if ( j-> failed ( ))
		setErrorText ( tr( "Download failed: %1" ). arg( j-> errorString ( )));
	else
		emit transferFinished ( );
}

bool CProgressDialog::initTransfer ( )
{
	if ( m_trans )
		return true;

	m_trans = new CTransfer ( );

	if ( m_trans-> init ( )) {
		connect ( m_trans, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( transferDone ( CTransfer::Job * )));
		connect ( m_trans, SIGNAL( progress ( CTransfer::Job *, int, int )), this, SLOT( transferProgress ( CTransfer::Job *, int, int )));

		m_trans-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));

		return true;
	}
	return false;
}

bool CProgressDialog::hasErrors ( ) const
{
	return m_has_errors;
}

bool CProgressDialog::post ( const QString &url, const CKeyValueList &query, QFile *file )
{
	if ( initTransfer ( )) {
		m_job = m_trans-> post ( url. latin1 ( ), query, file );
		return ( m_job != 0 );
	}
	return false;
}

bool CProgressDialog::get ( const QString &url, const CKeyValueList &query, const QDateTime &ifnewer, QFile *file )
{
	if ( initTransfer ( )) {
		m_job = m_trans-> getIfNewer ( url. latin1 ( ), query, ifnewer, file );
		return ( m_job != 0 );
	}
	return false;
}


void CProgressDialog::layout ( )
{
#if 0
	QSize now = size ( );
	QSize then = sizeHint ( );

	setFixedSize ( QSize ( QMAX( now. width ( ), then. height ( )), QMAX( now. height ( ), then. height ( ))));
#endif
	setFixedSize ( sizeHint ( ));
}

void CProgressDialog::setAutoClose ( bool ac )
{
	m_autoclose = ac;
}

