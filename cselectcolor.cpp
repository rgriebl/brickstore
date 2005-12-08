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
#include <qheader.h>
#include <qpushbutton.h>
#include <qstyle.h>
#include <qpainter.h>

#include "cutility.h"
#include "clistview.h"
#include "cselectcolor.h"


namespace {

class ColListItem : public CListViewItem {
public:
	ColListItem ( CListView *lv, const BrickLink::Color *col )
		: CListViewItem ( lv ), m_col ( col )
	{
		QFontMetrics fm = lv-> fontMetrics ( );
		m_pix. convertFromImage ( BrickLink::inst ( )-> colorImage ( col, fm. height ( ) + 2, fm. height ( ) + 2 ));
	}

	virtual const QPixmap *pixmap ( int /*col*/ ) const
	{ return &m_pix; }

	virtual QString text ( int /*col*/ ) const
	{ return m_col-> name ( ); }

	virtual int compare ( QListViewItem *i, int /*col*/, bool ascending ) const
	{
		ColListItem *cli = static_cast <ColListItem *> ( i );

		if ( ascending )
			return text ( 0 ). compare ( cli-> text ( 0 ));
		else
			return -CUtility::compareColors ( m_col-> color ( ), cli-> m_col-> color ( ));
	}

	const BrickLink::Color *color ( ) const
	{ return m_col; }

private:
	const BrickLink::Color *m_col;
	QPixmap m_pix;
};

} // namespace


CSelectColor::CSelectColor ( QWidget *parent, const char *name, WFlags fl )
	: QWidget ( parent, name, fl )
{
	w_colors = new CListView ( this );
	setFocusProxy ( w_colors );
	w_colors-> setAlwaysShowSelection ( true );
	w_colors-> addColumn ( tr( "Color" ));
	w_colors-> header ( )-> setMovingEnabled ( false );
	w_colors-> header ( )-> setResizeEnabled ( false );

	const QIntDict<BrickLink::Color> &coldict = BrickLink::inst ( )-> colors ( );

	for ( QIntDictIterator<BrickLink::Color> it ( coldict ); it. current ( ); ++it )
		(void) new ColListItem ( w_colors, it. current ( ));

	w_colors-> sort ( );

	connect ( w_colors, SIGNAL( selectionChanged ( )), this, SLOT( colorChanged ( )));
	connect ( w_colors, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( colorConfirmed ( )));
	connect ( w_colors, SIGNAL( returnPressed ( QListViewItem * )), this, SLOT( colorConfirmed ( ))); 

	w_colors-> setFixedWidth ( w_colors-> sizeHint ( ). width ( ) + w_colors-> style ( ). pixelMetric ( QStyle::PM_ScrollBarExtent ));
	w_colors-> setResizeMode ( QListView::LastColumn );

	QBoxLayout *lay = new QVBoxLayout ( this, 0, 0 );
	lay-> addWidget ( w_colors ); 
}

const BrickLink::Color *CSelectColor::color ( ) const
{
	ColListItem *cli = static_cast <ColListItem *> ( w_colors-> selectedItem ( ));
	return cli ? cli-> color ( ) : 0;
}

void CSelectColor::enabledChange ( bool old )
{
	if ( !isEnabled ( ))
		setColor ( BrickLink::inst ( )-> color ( 0 ));

	QWidget::enabledChange ( old );
}

void CSelectColor::setColor ( const BrickLink::Color *col )
{
	for ( QListViewItemIterator it ( w_colors ); *it; ++it ) {
		if ( static_cast <ColListItem *> ( *it )-> color ( ) == col ) {
			w_colors-> setSelected ( *it, true );
			break;
		}
	}
}

void CSelectColor::colorChanged ( )
{
	emit colorSelected ( color ( ), false );
}

void CSelectColor::colorConfirmed ( )
{
	emit colorSelected ( color ( ), true );
}

void CSelectColor::showEvent ( QShowEvent * )
{
	w_colors-> centerItem ( w_colors-> selectedItem ( ));
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


CSelectColorDialog::CSelectColorDialog ( QWidget *parent, const char *name, bool modal, WFlags fl )
	: QDialog ( parent, name, modal, fl )
{
	w_sc = new CSelectColor ( this );
	w_sc-> w_colors-> setMaximumSize ( 32767, 32767 ); // fixed width -> minimum width in this case

	w_ok = new QPushButton ( tr( "&OK" ), this );
	w_ok-> setAutoDefault ( true );
	w_ok-> setDefault ( true );

	w_cancel = new QPushButton ( tr( "&Cancel" ), this );
	w_cancel-> setAutoDefault ( true );

	QFrame *hline = new QFrame ( this );
	hline-> setFrameStyle ( QFrame::HLine | QFrame::Sunken );
	
	QBoxLayout *toplay = new QVBoxLayout ( this, 11, 6 );
	toplay-> addWidget ( w_sc );
	toplay-> addWidget ( hline );

	QBoxLayout *butlay = new QHBoxLayout ( toplay );
	butlay-> addStretch ( 60 );
	butlay-> addWidget ( w_ok, 15 );
	butlay-> addWidget ( w_cancel, 15 );

	setSizeGripEnabled ( true );
	setMinimumSize ( minimumSizeHint ( ));

	connect ( w_ok, SIGNAL( clicked ( )), this, SLOT( accept ( )));
	connect ( w_cancel, SIGNAL( clicked ( )), this, SLOT( reject ( )));
	connect ( w_sc, SIGNAL( colorSelected ( const BrickLink::Color *, bool )), this, SLOT( checkColor ( const BrickLink::Color *, bool )));

	w_ok-> setEnabled ( false );
	w_sc-> setFocus ( );
}

void CSelectColorDialog::setColor ( const BrickLink::Color *col )
{
	w_sc-> setColor ( col );
}

const BrickLink::Color *CSelectColorDialog::color ( ) const
{
	return w_sc-> color ( );
}

void CSelectColorDialog::checkColor ( const BrickLink::Color *col, bool ok )
{
	w_ok-> setEnabled (( col ));

	if ( col && ok )
		w_ok-> animateClick ( );
}

int CSelectColorDialog::exec ( const QRect &pos )
{
	if ( pos. isValid ( ))
		CUtility::setPopupPos ( this, pos );

	return QDialog::exec ( );
}

