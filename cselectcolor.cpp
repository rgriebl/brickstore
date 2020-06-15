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
#include <qlayout.h>
#include <q3header.h>
#include <qpushbutton.h>
#include <qstyle.h>
#include <qpainter.h>
#include <qtooltip.h>
//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3BoxLayout>
#include <QShowEvent>
#include <Q3Frame>
#include <QPixmap>
#include <Q3VBoxLayout>
#include <Q3MimeSourceFactory>

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
		m_pix = BrickLink::inst ( )-> colorImage ( col, fm. height ( ) + 2, fm. height ( ) + 2 );
	}

	virtual const QPixmap *pixmap ( int /*col*/ ) const
	{ return m_pix; }

	virtual QString text ( int /*col*/ ) const
	{ return m_col-> name ( ); }
	
	virtual QString toolTip ( ) const
	{ 
		if ( color ( )-> color ( ). isValid ( )) {
			QFontMetrics fm = listView ( )-> fontMetrics ( );
			QPixmap bigpix ( *BrickLink::inst ( )-> colorImage ( color ( ), fm. height ( ) * 8, fm. height ( ) * 4 ));
		
			Q3MimeSourceFactory::defaultFactory ( )-> setPixmap ( "select_color_tooltip_picture", bigpix );

			return QString( "<img src=\"select_color_tooltip_picture\"><br />%1: %2" ). arg ( CSelectColor::tr( "RGB" ), color ( )-> color ( ). name ( ));
		}
		return QString ( );
	}
	
	virtual int compare ( Q3ListViewItem *i, int /*col*/, bool ascending ) const
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
	const QPixmap *m_pix;
};

class ColToolTip : public QObject {
public:
	ColToolTip ( QWidget *parent, CListView *clv )
        : QObject( parent ), m_clv ( clv )
	{ }
	
	virtual ~ColToolTip ( )
	{ }

	void maybeTip ( const QPoint &pos )
	{
        if ( !((QWidget*)QObject::parent()) || !m_clv /*|| !m_clv-> showToolTips ( )*/ )
			return;

		ColListItem *item = static_cast <ColListItem *> ( m_clv-> itemAt ( pos ));
		
		if ( item )
            QToolTip::add(((QWidget*)QObject::parent()), m_clv-> itemRect ( item ), item-> toolTip ( ));
	}

private:
    CListView *m_clv;
};


} // namespace


CSelectColor::CSelectColor ( QWidget *parent, const char *name, Qt::WFlags fl )
	: QWidget ( parent, name, fl )
{
	w_colors = new CListView ( this );
	setFocusProxy ( w_colors );
	w_colors-> setAlwaysShowSelection ( true );
	w_colors-> addColumn ( QString ( ));
	w_colors-> header ( )-> setMovingEnabled ( false );
	w_colors-> header ( )-> setResizeEnabled ( false );
    new ColToolTip ( w_colors-> viewport ( ), w_colors );

    const QHash<int, BrickLink::Color *> &coldict = BrickLink::inst ( )-> colors ( );

    for ( QHashIterator<int, BrickLink::Color *> it ( coldict ); it. hasNext ( ); )
        (void) new ColListItem ( w_colors, it. next ( ). value ( ));

	w_colors-> sort ( );

	connect ( w_colors, SIGNAL( selectionChanged ( )), this, SLOT( colorChanged ( )));
	connect ( w_colors, SIGNAL( doubleClicked ( Q3ListViewItem *, const QPoint &, int )), this, SLOT( colorConfirmed ( )));
	connect ( w_colors, SIGNAL( returnPressed ( Q3ListViewItem * )), this, SLOT( colorConfirmed ( ))); 

    w_colors-> setFixedWidth ( w_colors-> sizeHint ( ). width ( ) + w_colors-> style ( )-> pixelMetric ( QStyle::PM_ScrollBarExtent ));
	w_colors-> setResizeMode ( Q3ListView::LastColumn );

	Q3BoxLayout *lay = new Q3VBoxLayout ( this, 0, 0 );
	lay-> addWidget ( w_colors );

	languageChange ( );
}

void CSelectColor::languageChange ( )
{
	w_colors-> setColumnText ( 0, tr( "Color" ));
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
	for ( Q3ListViewItemIterator it ( w_colors ); *it; ++it ) {
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


CSelectColorDialog::CSelectColorDialog ( QWidget *parent, const char *name, bool modal, Qt::WFlags fl )
	: QDialog ( parent, name, modal, fl )
{
	w_sc = new CSelectColor ( this );
	w_sc-> w_colors-> setMaximumSize ( 32767, 32767 ); // fixed width -> minimum width in this case

	w_ok = new QPushButton ( tr( "&OK" ), this );
	w_ok-> setAutoDefault ( true );
	w_ok-> setDefault ( true );

	w_cancel = new QPushButton ( tr( "&Cancel" ), this );
	w_cancel-> setAutoDefault ( true );

	Q3Frame *hline = new Q3Frame ( this );
	hline-> setFrameStyle ( Q3Frame::HLine | Q3Frame::Sunken );
	
	Q3BoxLayout *toplay = new Q3VBoxLayout ( this, 11, 6 );
	toplay-> addWidget ( w_sc );
	toplay-> addWidget ( hline );

	Q3BoxLayout *butlay = new Q3HBoxLayout ( toplay );
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

