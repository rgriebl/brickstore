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
#ifndef __CREBUILDDATABASE_H__
#define __CREBUILDDATABASE_H__

#include <qobject.h>
#include <qmap.h>

#include "bricklink.h"
#include "ctransfer.h"

class CRebuildDatabase : public QObject {
	Q_OBJECT

public:
	CRebuildDatabase ( const QString &output );

	void exec ( );

signals:
	void finished ( int );

private slots:
	void downloadJobFinished ( CTransfer::Job *job );
	void inventoryUpdated ( BrickLink::Inventory *inv );

private:
	void error ( );

	bool parse ( );
	bool parseInv ( );
	bool download ( );
	bool downloadInv ( );
	bool writeDB ( );

private:
	QString m_output;
	QString m_error;
	int m_downloads_in_progress;
	int m_downloads_failed;
	int m_processed;
	int m_ptotal;

	QMap<const BrickLink::Item *, BrickLink::Item::AppearsInMap> m_map;
};

#endif
