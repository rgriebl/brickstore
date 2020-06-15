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
#ifndef __CMULTIPROGRESSBAR_H__
#define __CMULTIPROGRESSBAR_H__

#include <qwidget.h>
#include <q3intdict.h>
#include <qpixmap.h>
//Added by qt3to4:
#include <QResizeEvent>

class Q3ProgressBar;
class QToolButton;

class CMultiProgressBar : public QWidget {
	Q_OBJECT
public:
	CMultiProgressBar ( QWidget *parent = 0, const char *name = 0 );
	virtual ~CMultiProgressBar ( );

	void setStopPixmap ( const QPixmap & );

	int addItem ( const QString &label, int id = -1 );
	void removeItem ( int id );

	QString itemLabel ( int id ) const;

	int itemProgress ( int id ) const;
	int itemTotalSteps ( int id ) const;

public slots:
	void setItemProgress ( int id, int progress );
	void setItemProgress ( int id, int progress, int total );
	void setItemTotalSteps ( int id, int total );

	void setItemLabel ( int id, const QString &label );

signals:
	void stop ( );
	void statusChange ( bool );

protected:
	void resizeEvent ( QResizeEvent *e );

protected slots:
	void languageChange ( );

private:
	void recalc ( );
	void recalcPixmap ( QToolButton *but, const QPixmap &pix );

private:
	int m_autoid;

	Q3ProgressBar *m_progress;
	QToolButton *m_stop;

	QPixmap m_stop_pix;

	struct ItemData {
		ItemData ( const QString &label );

		QString m_label;
		int     m_progress;
		int     m_total;
	};

	Q3IntDict <ItemData> m_items;
};

#endif
