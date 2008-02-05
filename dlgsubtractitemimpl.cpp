/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#include <qlabel.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qclipboard.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlayout.h>

#include "cconfig.h"
#include "cframework.h"
#include "cwindow.h"
#include "cdocument.h"
#include "cresource.h"
#include "cutility.h"

#include "dlgsubtractitemimpl.h"


class DocListItem : public QListViewItem {
public:
	DocListItem ( QListView *lv, CWindow *w )
		: QListViewItem ( lv ), m_window ( w )
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
	: DlgSubtractItem ( parent, name, modal, fl )
{
	m_window = parent;
	//setCaption ( caption ( ). arg ( parent-> caption ( )));
	w_header-> setText ( headertext );

	connect ( w_doc_list, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( docSelected ( QListViewItem * )));
	
	QPtrList <CWindow> list = CFrameWork::inst ( )-> allWindows ( );

	for ( QPtrListIterator <CWindow> it ( list ); it. current ( ); ++it ) {
		if ( it.current ( ) != parent )
			(void) new DocListItem ( w_doc_list, it. current ( ));
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

void DlgSubtractItemImpl::docSelected ( QListViewItem *item )
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
