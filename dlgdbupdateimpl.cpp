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
#include <qpushbutton.h>
#include <qlabel.h>
#include <qprogressbar.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qfile.h>
#include <qdir.h>


#include "cutility.h"
#include "cconfig.h"
#include "bricklink.h"
#include "dlgdbupdateimpl.h"

class DlgDBUpdateImplPrivate {
public:
	QFile *         m_file;
	CTransfer *     m_trans;
	CTransfer::Job *m_job;
	bool            m_has_errors;
	bool            m_override;
};

#define DATABASE_URL   "http://softforge.de/binary.cache.gz"


DlgDBUpdateImpl::DlgDBUpdateImpl ( QWidget *parent, const char *name, bool modal, int fl )
	: DlgDBUpdate ( parent, name, modal, fl )
{
	d = new DlgDBUpdateImplPrivate ( );

	d-> m_has_errors = false;
	d-> m_file = 0;
	d-> m_job = 0;
	d-> m_trans = new CTransfer ( );
	connect ( d-> m_trans, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( transferJobFinished ( CTransfer::Job * )));
	connect ( d-> m_trans, SIGNAL( progress ( CTransfer::Job *, int, int )), this, SLOT( transferJobProgress ( CTransfer::Job *, int, int )));

	if ( d-> m_trans-> init ( )) {
		d-> m_trans-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));

		d-> m_file = new QFile ( BrickLink::inst ( )-> dataPath ( ) + "binary.cache.new" );

		if ( d-> m_file-> open ( IO_WriteOnly )) {
			QString url = DATABASE_URL;
			d-> m_job = d-> m_trans-> get ( url. latin1 ( ), CKeyValueList ( ), d-> m_file );
		}
	}

	if ( d-> m_trans && d-> m_job ) {
		w_ok-> hide ( );
		transferJobProgress ( d-> m_job, 0, 0 );
		QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));
		d-> m_override = true;
	}
	else {
		message ( true, tr( "Transfer job could not be started." ));
		w_cancel-> hide ( );
		d-> m_override = false;
	}

	setFixedSize ( sizeHint ( ));
}

DlgDBUpdateImpl::~DlgDBUpdateImpl ( )
{
	delete d-> m_trans;
	delete d;
}

void DlgDBUpdateImpl::done ( int r )
{
	d-> m_trans-> cancelAllJobs ( );
	DlgDBUpdate::done ( r );

	if ( d-> m_override )
		QApplication::restoreOverrideCursor ( );
}

bool DlgDBUpdateImpl::errors ( ) const
{
	return d-> m_has_errors;
}

void DlgDBUpdateImpl::transferJobProgress ( CTransfer::Job *job, int progress, int total )
{
	if ( job == d-> m_job ) {
		w_progress-> setProgress ( progress, total );
		message ( false, tr( "Downloading: %1/%2 KB" ). arg( progress / 1024 ). arg( total / 1024 ));
	}
}

void DlgDBUpdateImpl::transferJobFinished ( CTransfer::Job *job )
{
	if ( job == d-> m_job ) {
		w_ok-> show ( );
		w_cancel-> hide ( );

		if ( d-> m_file-> isOpen ( ))
			d-> m_file-> close ( );

		if ( !job-> failed ( ) && d-> m_file-> size ( )) {
			QString basepath = d-> m_file-> name ( );
			basepath. truncate ( basepath. length ( ) - 4 );

			QString error = CUtility::safeRename ( basepath );

			if ( error. isNull ( )) {
				if ( BrickLink::inst ( )-> readDatabase ( ))
					message ( false, tr( "Finished." ));
				else
					message ( true, tr( "Could not load the new database." ));
			}
			else
				message ( true, error );
		}
		else
			message ( true, tr( "Transfer job failed: %1" ). arg ( !job-> responseCode ( ) ? job-> errorString ( ) :  tr( "Version information is not available." )));

		w_progress-> setProgress ( -1, -1 );
		QApplication::restoreOverrideCursor ( );
		d-> m_override = false;

		setFixedSize ( sizeHint ( ));
	}
}

void DlgDBUpdateImpl::message ( bool error, const QString &msg )
{
	w_message-> setText ( error ? QString( "<b>%1:</b> %2" ). arg( tr( "Error" )). arg( msg ) : msg );
	d-> m_has_errors |= error;
}

