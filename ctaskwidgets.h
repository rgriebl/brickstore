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
#ifndef __CTASKWIDGETS_H__
#define __CTASKWIDGETS_H__

#include <QStackedWidget>
#include <QTextBrowser>

#include "cdocument.h"
#include "cappearsinwidget.h"
#include "cpriceguidewidget.h"
#include "cpicturewidget.h"

class QLabel;
class QDockWidget;


class CTaskLinksWidget : public QTextBrowser {
	Q_OBJECT

public:
	CTaskLinksWidget ( QWidget *parent );

protected slots:
	void documentUpdate ( CDocument *doc );
	void selectionUpdate ( const CDocument::ItemList &list );

	void languageChange ( );

private:
	CDocument * m_doc;

};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class CTaskPriceGuideWidget : public CPriceGuideWidget {
	Q_OBJECT

public:
	CTaskPriceGuideWidget ( QWidget *parent );

protected slots:
	void documentUpdate ( CDocument *doc );
	void selectionUpdate ( const CDocument::ItemList &list );

	void setPrice ( money_t p );

protected:
	virtual bool event ( QEvent *e );
	void fixParentDockWindow ( );
	virtual bool eventFilter ( QObject *o, QEvent *e );

private:
	CDocument *m_doc;
	QDockWidget *m_dock;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class CTaskInfoWidget : public QStackedWidget {
	Q_OBJECT

public:
	CTaskInfoWidget ( QWidget *parent );

protected slots:
	void documentUpdate ( CDocument *doc );
	void selectionUpdate ( const CDocument::ItemList &list );

	void languageChange ( );
	void refresh ( );

private:
	QLabel *        m_text;
	CPictureWidget *m_pic;
	CDocument *     m_doc;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class CTaskAppearsInWidget : public CAppearsInWidget {
	Q_OBJECT

public:
	CTaskAppearsInWidget ( QWidget *parent );

	virtual QSize minimumSizeHint ( ) const;
	virtual QSize sizeHint ( ) const;

protected slots:
	void documentUpdate ( CDocument *doc );
	void selectionUpdate ( const CDocument::ItemList &list );

private:
	CDocument * m_doc;
};

#endif
