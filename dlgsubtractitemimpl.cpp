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
#include <qlabel.h>
#include <q3buttongroup.h>
#include <qradiobutton.h>
#include <qclipboard.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <q3header.h>
#include <q3listview.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QPixmap>

#include "cconfig.h"
#include "cframework.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cresource.h"
#include "cutility.h"

#include "dlgsubtractitemimpl.h"


class DocListItem : public Q3ListViewItem {
public:
	DocListItem ( Q3ListView *lv, CWindow *w )
		: Q3ListViewItem ( lv ), m_window ( w )
	{ }

	virtual QString text ( int /*col*/ ) const
	{
		return m_window-> caption ( );
	}

	virtual const QPixmap *pixmap ( int /*col*/ ) const
	{
		return m_window-> icon ( );
	}

	CWindow *window ( ) const
	{
		return m_window;
	}

private:
	CWindow *m_window;
};

DlgSubtractItemImpl::DlgSubtractItemImpl ( const QString &headertext, CWindow *parent, const char *name, bool modal, int fl )
    : QDialog ( parent, name, modal, (Qt::WindowType)fl )
{
    setupUi ( this );

	m_window = parent;
	//setCaption ( caption ( ). arg ( parent-> caption ( )));
	w_header-> setText ( headertext );

	connect ( w_doc_list, SIGNAL( doubleClicked ( Q3ListViewItem *, const QPoint &, int )), this, SLOT( docSelected ( Q3ListViewItem * )));
	
    QList <CWindow *> list = CFrameWork::inst ( )-> allWindows ( );
    QListIterator<CWindow *> i(list);

    while (i.hasNext()) {
        CWindow *w = i.next();
        if (w != parent) {
            (void) new DocListItem ( w_doc_list, w);
        }
    }

	w_doc_list-> header ( )-> hide ( );

	bool clipok = false;
	bool listok = false;

	if ( w_doc_list-> firstChild ( )) {
		w_doc_list-> setSelected ( w_doc_list-> firstChild ( ), true );
		listok = true;
	}

	if ( BrickLink::InvItemDrag::canDecode ( QApplication::clipboard ( )-> data ( )))
		clipok = true;

	w_doc_list-> setEnabled ( listok && !clipok );
	w_document-> setEnabled ( listok );
	w_clipboard-> setEnabled ( clipok );

	w_type-> setButton ( clipok || !listok ? 0 : 1 );

	if ( !listok && !clipok )
		w_ok-> setEnabled ( false );
}

void DlgSubtractItemImpl::docSelected ( Q3ListViewItem *item )
{
	if ( item )
		w_ok-> animateClick ( );
}

BrickLink::InvItemList DlgSubtractItemImpl::items ( ) const
{
	BrickLink::InvItemList list;

	switch ( w_type-> selectedId ( )) {
		case 0: // clipboard
			BrickLink::InvItemDrag::decode ( QApplication::clipboard ( )-> data ( ), list );
			break;

		case 1: // document
			if ( w_doc_list-> selectedItem ( )) {
				foreach ( CDocument::Item *item, static_cast <DocListItem *> ( w_doc_list-> selectedItem ( ))-> window ( )-> document ( )-> items ( ))
					list. append ( new BrickLink::InvItem ( *item ));
			}
			break;
	}
	return list;
}
