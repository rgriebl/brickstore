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
#ifndef __CREPORT_H__
#define __CREPORT_H__

#include <qobject.h>
#include <qmap.h>
#include <qdict.h>
#include <qstring.h>
#include <qptrlist.h>
#include <qprinter.h>

#include "bricklink.h"

class CReportFunction {
public:
	virtual QString name ( ) const = 0;
	virtual int argc ( ) const = 0;

	virtual QString operator ( ) ( const QStringList &argv ) = 0;
	
	virtual ~CReportFunction ( ) { }
};

typedef QMap <QString, QString>  CReportVariables;
typedef QDict <CReportFunction>  CReportFunctions;

class CReportPrivate;
class QPainter;

class CReport : public QObject {
	Q_OBJECT
public:
	CReport ( );
	virtual ~CReport ( );

	bool load ( const QString &file );

	QString name ( ) const;
	
	QPrinter::PageSize pageSize ( ) const;
	QSize pageSizePt ( ) const;

	uint pageCount ( uint itemcount ) const;
	void render ( const QPtrList <BrickLink::InvItem> &items, const CReportVariables &add_vars, int from, int to, QPainter *p ) const;

	bool hasUI ( ) const;
	QWidget *createUI ( CReportVariables &vars ) const;
	CReportVariables getValuesFromUI ( QWidget *ui ) const;

private:
	CReportPrivate *d;
};

class CReportManager {
private:
	CReportManager ( );
	static CReportManager *s_inst;

public:
	~CReportManager ( );
	static CReportManager *inst ( );

	bool reload ( );

	QPrinter *printer ( ) const;
	
	const QPtrList <CReport> &reports ( ) const;

	const CReportFunctions &functions ( ) const;
	
private:
	QPtrList <CReport> m_reports;
	CReportFunctions m_functions;

	mutable QPrinter *m_printer; // mutable for delayed initialization
};

#endif

