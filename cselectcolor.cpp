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
#include <QLayout>
#include <QList>
#include <QPushButton>
#include <QHash>
#include <QApplication>
#include <QHeaderView>
#include <QTableView>
#include <QItemDelegate>

#include "cutility.h"
//#include "clistview.h"
#include "cselectcolor.h"


namespace {

class ColorModel : public QAbstractTableModel {
public:
	enum Role { BrickLinkColorRole = Qt::UserRole + 1 };

	ColorModel ( )
	{ m_colors = BrickLink::inst ( )-> colors ( ). values ( ); }

	const BrickLink::Color *color ( const QModelIndex &index ) const
	{ return m_colors [index. row ( )]; }

	QModelIndex index ( const BrickLink::Color *color ) const
	{ return color ? createIndex ( m_colors. indexOf ( color ), 0, 0 ) : QModelIndex ( ); }

	virtual int rowCount ( const QModelIndex & /*parent*/ ) const
	{ return m_colors. count ( ); }

	virtual int columnCount ( const QModelIndex & /*parent*/ ) const
	{ return 1;	}

	virtual QVariant data ( const QModelIndex &index, int role ) const
	{
		QVariant res;
		const BrickLink::Color *color = this-> color ( index );
		int col = index. column ( );

		if ( col == 0 ) {
			if ( role == Qt:: DisplayRole ) {
				res = color-> name ( );
			}
			else if ( role == Qt::DecorationRole ) {
				QFontMetrics fm = QApplication::fontMetrics ( );
				QPixmap pix = QPixmap::fromImage ( BrickLink::inst ( )-> colorImage ( color, fm. height ( ), fm. height ( )));
				res = pix;
			}
			else if ( role == Qt::ToolTipRole ) {
				res = QString( "<img src=\"#/select_color_tooltip_picture\"><br />%1: %2" ). arg ( CSelectColor::tr( "RGB" ), color-> color ( ). name ( ));
			}
			else if ( role == BrickLinkColorRole ) {
				res = color;
			}
		}
		
		return res;
	}

	virtual QVariant headerData ( int section, Qt::Orientation orient, int role ) const
    {
		if (( orient == Qt::Horizontal ) && ( role == Qt::DisplayRole ) && ( section == 0 ))
			return CSelectColor::tr( "Color" );
		return QVariant ( );
    }

	virtual void sort ( int column, Qt::SortOrder so )
	{
		if ( column == 0 ) {
		    emit layoutAboutToBeChanged ( );
			qStableSort ( m_colors. begin ( ), m_colors. end ( ), so == Qt::DescendingOrder ? colorNameCompare : colorHsvCompare );
			emit layoutChanged ( );
		}
	}

	static bool colorNameCompare ( const BrickLink::Color *c1, const BrickLink::Color *c2 )
	{
		return qstrcmp ( c1-> name ( ), c2-> name ( )) < 0;
	}

	static bool colorHsvCompare ( const BrickLink::Color *c1, const BrickLink::Color *c2 )
	{
		int lh, rh, ls, rs, lv, rv, d;

		c1-> color ( ). getHsv ( &lh, &ls, &lv );
		c2-> color ( ). getHsv ( &rh, &rs, &rv );

		if ( lh != rh )
			d = lh - rh;
		else if ( ls != rs )
			d = ls - rs;
		else
			d = lv - rv;

		return d < 0;
	}

private:
	QList<const BrickLink::Color *> m_colors;
};


class ColorDelegate : public QItemDelegate {
public:
    ColorDelegate ( QObject *parent = 0 )
		: QItemDelegate ( parent )
	{ }

	virtual void drawDecoration ( QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QPixmap &pixmap) const
	{
		QStyleOptionViewItem myoption ( option );
		myoption. state &= ~QStyle::State_Selected;

		QItemDelegate::drawDecoration ( painter, myoption, rect, pixmap );
	}
};


}

CSelectColor::CSelectColor ( QWidget *parent, Qt::WindowFlags f )
	: QWidget ( parent, f )
{
	w_colors = new QTableView ( this );
	w_colors-> setModel ( new ColorModel ( ));
	w_colors-> setItemDelegate ( new ColorDelegate ( ));
	w_colors-> horizontalHeader ( )-> setResizeMode ( QHeaderView::Fixed );
	w_colors-> horizontalHeader ( )-> setStretchLastSection ( true );
	w_colors-> horizontalHeader ( )-> setMovable ( false );
	w_colors-> verticalHeader ( )-> setResizeMode ( QHeaderView::ResizeToContents );
	w_colors-> verticalHeader ( )-> hide ( );
	w_colors-> setShowGrid ( false );
	w_colors-> setAlternatingRowColors ( true );
	w_colors-> setSortingEnabled ( true );
	w_colors-> setFixedWidth ( w_colors-> sizeHint ( ). width ( ) + w_colors-> style ( )-> pixelMetric ( QStyle::PM_ScrollBarExtent ));
	w_colors-> sortByColumn ( 0 );

	setFocusProxy ( w_colors );

	connect ( w_colors-> selectionModel ( ), SIGNAL( selectionChanged ( const QItemSelection &, const QItemSelection & )), this, SLOT( colorChanged ( )));
	connect ( w_colors, SIGNAL( activated ( const QModelIndex & )), this, SLOT( colorConfirmed ( )));

	QBoxLayout *lay = new QVBoxLayout ( this );
	lay-> setMargin ( 0 );
	lay-> addWidget ( w_colors );
}

const BrickLink::Color *CSelectColor::color ( ) const
{
	ColorModel *model = static_cast<ColorModel *>( w_colors-> model ( ));

	if ( !w_colors-> selectionModel ( )-> selectedIndexes ( ). isEmpty ( ))
		return model-> color ( w_colors-> selectionModel ( )-> selectedIndexes ( ). front ( ));
	else
		return 0;
}

void CSelectColor::enabledChange ( bool old )
{
	if ( !isEnabled ( ))
		setColor ( BrickLink::inst ( )-> color ( 0 ));

	QWidget::enabledChange ( old );
}

void CSelectColor::setColor ( const BrickLink::Color *color )
{
	ColorModel *model = static_cast<ColorModel *>( w_colors-> model ( ));

	w_colors-> selectionModel ( )-> select ( model-> index ( color ), QItemSelectionModel::SelectCurrent );
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
	if ( !w_colors-> selectionModel ( )-> selectedIndexes ( ). isEmpty ( ))
		w_colors-> scrollTo ( w_colors-> selectionModel ( )-> selectedIndexes ( ). front ( ), QAbstractItemView::PositionAtCenter );
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


DSelectColor::DSelectColor ( QWidget *parent, Qt::WindowFlags f )
	: QDialog ( parent, f )
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
	
	QBoxLayout *toplay = new QVBoxLayout ( this );
	toplay-> addWidget ( w_sc );
	toplay-> addWidget ( hline );

	QBoxLayout *butlay = new QHBoxLayout ( );
	toplay-> addLayout ( butlay );
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

void DSelectColor::setColor ( const BrickLink::Color *col )
{
	w_sc-> setColor ( col );
}

const BrickLink::Color *DSelectColor::color ( ) const
{
	return w_sc-> color ( );
}

void DSelectColor::checkColor ( const BrickLink::Color *col, bool ok )
{
	w_ok-> setEnabled (( col ));

	if ( col && ok )
		w_ok-> animateClick ( );
}

int DSelectColor::exec ( const QRect &pos )
{
	if ( pos. isValid ( ))
		CUtility::setPopupPos ( this, pos );

	return QDialog::exec ( );
}

