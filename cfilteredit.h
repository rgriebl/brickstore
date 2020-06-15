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
#ifndef __CFILTEREDIT_H__
#define __CFILTEREDIT_H__

#include <q3frame.h>
//Added by qt3to4:
#include <QPaintEvent>
#include <QResizeEvent>

class CFilterEditPrivate;

class CFilterEdit : public Q3Frame {
	Q_OBJECT
public:
	CFilterEdit ( QWidget *parent, const char *name = 0, Qt::WFlags fl = 0 );
	virtual ~CFilterEdit ( );

	int addFilterType ( const QString &name, int id = -1 );

	void setFilterText ( const QString &str );
	QString filterText ( ) const;

signals:
	void filterTypeChanged ( int );
	void filterTextChanged ( const QString & );
	void returnPressed ( );

protected:
	void resizeEvent ( QResizeEvent * );
	void paintEvent ( QPaintEvent * );

private slots:
	void slotTextChanged ( const QString & );

private:
	void calcLayout ( );

private:
	CFilterEditPrivate *d;
};

#endif

