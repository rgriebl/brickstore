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
#ifndef __CPROGRESSDIALOG_H__
#define __CPROGRESSDIALOG_H__

#include <qdialog.h>

#include "ctransfer.h"

class QLabel;
class CUrlLabel;
class QProgressBar;
class QWidget;
class QPushButton;


class CProgressDialog : public QDialog {
	Q_OBJECT

public:
	CProgressDialog ( QWidget *parent, const char *name = 0 );
	virtual ~CProgressDialog ( );

	void setAutoClose ( bool ac );
	void setHeaderText ( const QString &str );
	void setMessageText ( const QString &str );
	void setErrorText ( const QString &str );
	void setFinished ( bool ok );
	void setProgress ( int steps, int total );
	void setProgressVisible ( bool b );

	bool post ( const QString &url, const CKeyValueList &query = CKeyValueList ( ), QFile *file = 0 );
	bool get ( const QString &url, const CKeyValueList &query = CKeyValueList ( ), const QDateTime &ifnewer = QDateTime ( ), QFile *file = 0 );

	CTransfer::Job *job ( ) const;
	bool hasErrors ( ) const;

	void layout ( );

signals:
	void transferFinished ( );

protected slots:
	virtual void done ( int r );

private slots:
	void transferProgress ( CTransfer::Job *job, int, int );
	void transferDone ( CTransfer::Job *job );

private:
	bool initTransfer ( );
	void syncRepaint ( QWidget *w );

private:
	QLabel *m_header;
	CUrlLabel *m_message;
	QProgressBar *m_progress;
	QWidget *m_progress_container;
	QPushButton *m_ok;
	QPushButton *m_cancel;

	QString m_message_text;
	bool m_message_progress : 1;
	bool m_has_errors       : 1;
	bool m_autoclose        : 1;
	bool m_override         : 1;

	CTransfer *m_trans;
	CTransfer::Job *m_job;
};

#endif
