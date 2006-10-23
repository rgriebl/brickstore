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
#include <qpainter.h>
#include <qstyle.h>
#include <qvaluelist.h>
#include <qpopupmenu.h>
#include <qcursor.h>
#include <qaction.h>
#include <qtooltip.h>

#include "cresource.h"
#include "cutility.h"
#include "cmoney.h"

#include "cpriceguidewidget.h"

namespace {

static const int hborder = 4;
static const int vborder = 2;

struct cell : public QRect {
	enum cell_type {
		Header,
		Quantity,
		Price,
		Update,
		Empty
	};
	
	cell ( ) : QRect ( )
	{ }
	
	cell ( cell_type t, int x, int y, int w, int h, int tfl, const QString &str, bool flag = false )
		: QRect ( x, y, w, h ), m_type ( t ), m_text_flags ( tfl ), m_text ( str ), m_flag ( flag )
	{ }

	cell_type m_type;
	int       m_text_flags;
	QString   m_text;
	bool      m_flag;

	BrickLink::PriceGuide::Price m_price;
	BrickLink::PriceGuide::Time  m_time;
	BrickLink::Condition         m_condition;
};

} // namespace

class CPriceGuideWidgetPrivate : public QToolTip {
public:
	CPriceGuideWidgetPrivate ( CPriceGuideWidget *parent )
		: QToolTip ( parent ), m_widget ( parent )
	{ }
	
	virtual ~CPriceGuideWidgetPrivate ( )
	{ }

	QValueList<cell>::const_iterator cellAtPos ( const QPoint &fpos ) 
	{
		QPoint pos = fpos - QPoint ( m_widget-> frameWidth ( ), m_widget-> frameWidth ( ));
		QValueList<cell>::const_iterator it = m_cells. begin ( );

		for ( ; it != m_cells. end ( ); ++it ) {
			if (( *it ). contains ( pos ) && (( *it ). m_type != cell::Update ))
				break;
		}
		return it;
	}

protected:
	virtual void maybeTip ( const QPoint &pos )
	{
		QValueList<cell>::const_iterator it = cellAtPos ( pos );

		if (( it != m_cells. end ( )) && (( *it ). m_type == cell::Price ))
			tip ( *it, CPriceGuideWidget::tr( "Double-click to set the price of the current item." ));
	}

private:
	friend class CPriceGuideWidget;

	CPriceGuideWidget *       m_widget;
	BrickLink::PriceGuide *   m_pg;
	CPriceGuideWidget::Layout m_layout;
	QValueList<cell>          m_cells;
	bool                      m_connected;
	QPopupMenu *              m_popup;
	bool                      m_on_price;

	QString m_str_qty;
	QString m_str_cond [BrickLink::ConditionCount];
	QString m_str_price [BrickLink::PriceGuide::PriceCount];
	QString m_str_vtime [BrickLink::PriceGuide::TimeCount];
	QString m_str_htime [BrickLink::PriceGuide::TimeCount][2];
	QString m_str_wait;
};

CPriceGuideWidget::CPriceGuideWidget ( QWidget *parent, const char *name, WFlags fl )
	: QFrame ( parent, name, fl | WNoAutoErase)
{
	d = new CPriceGuideWidgetPrivate ( this );

	d-> m_pg = 0;
	d-> m_layout = Normal;
	d-> m_on_price = false;

	setBackgroundMode ( PaletteBase );
	setMouseTracking ( true );
	setSizePolicy ( QSizePolicy::Minimum, QSizePolicy::Minimum );

	d-> m_connected = false;
	d-> m_popup = 0;

	connect ( CMoney::inst ( ), SIGNAL( monetarySettingsChanged ( )), this, SLOT( repaint ( )));

	languageChange ( );
}

void CPriceGuideWidget::languageChange ( )
{
	delete d-> m_popup;
	d-> m_popup = 0;

	d-> m_str_qty                                       = tr( "Qty." );
	d-> m_str_cond [BrickLink::New]                     = tr( "New" );
	d-> m_str_cond [BrickLink::Used]                    = tr( "Used" );
	d-> m_str_price [BrickLink::PriceGuide::Lowest]     = tr( "Min." );
	d-> m_str_price [BrickLink::PriceGuide::Average]    = tr( "Avg." );
	d-> m_str_price [BrickLink::PriceGuide::WAverage]   = tr( "Q.Avg." );
	d-> m_str_price [BrickLink::PriceGuide::Highest]    = tr( "Max." );
	d-> m_str_htime [BrickLink::PriceGuide::AllTime][0] = tr( "All Time" );
	d-> m_str_htime [BrickLink::PriceGuide::AllTime][1] = tr( "Sales" );
	d-> m_str_htime [BrickLink::PriceGuide::PastSix][0] = tr( "Last 6" );
	d-> m_str_htime [BrickLink::PriceGuide::PastSix][1] = tr( "Months Sales" );
	d-> m_str_htime [BrickLink::PriceGuide::Current][0] = tr( "Current" );
	d-> m_str_htime [BrickLink::PriceGuide::Current][1] = tr( "Inventory" );
	d-> m_str_vtime [BrickLink::PriceGuide::AllTime]    = tr( "All Time Sales" );
	d-> m_str_vtime [BrickLink::PriceGuide::PastSix]    = tr( "Last 6 Months Sales" );
	d-> m_str_vtime [BrickLink::PriceGuide::Current]    = tr( "Current Inventory" );
	d-> m_str_wait                                      = tr( "Please wait ... updating" );

	recalcLayout ( );
	update ( );
}

CPriceGuideWidget::~CPriceGuideWidget ( )
{
	if ( d-> m_pg )
		d-> m_pg-> release ( );

	delete d;
}


void CPriceGuideWidget::contextMenuEvent ( QContextMenuEvent *e )
{
	if ( d-> m_pg ) {
		if ( !d-> m_popup ) {
			d-> m_popup = new QPopupMenu ( this );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "reload" ), tr( "Update" ), this, SLOT( doUpdate ( )));
			d-> m_popup-> insertSeparator ( );
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "edit_bl_catalog" ), tr( "Show BrickLink Catalog Info..." ), this, SLOT( showBLCatalogInfo ( )));
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "edit_bl_priceguide" ), tr( "Show BrickLink Price Guide Info..." ), this, SLOT( showBLPriceGuideInfo ( )));
			d-> m_popup-> insertItem ( CResource::inst ( )-> iconSet ( "edit_bl_lotsforsale" ), tr( "Show Lots for Sale on BrickLink..." ), this, SLOT( showBLLotsForSale ( )));
		}
		d-> m_popup-> popup ( e-> globalPos ( ));
	}

	e-> accept ( );
}

void CPriceGuideWidget::showBLCatalogInfo ( )
{
	if ( d-> m_pg && d-> m_pg-> item ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_CatalogInfo, d-> m_pg-> item ( )));
}

void CPriceGuideWidget::showBLPriceGuideInfo ( )
{
	if ( d-> m_pg && d-> m_pg-> item ( ) && d-> m_pg-> color ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_PriceGuideInfo, d-> m_pg-> item ( ), d-> m_pg-> color ( )));
}

void CPriceGuideWidget::showBLLotsForSale ( )
{
	if ( d-> m_pg && d-> m_pg-> item ( ) && d-> m_pg-> color ( ))
		CUtility::openUrl ( BrickLink::inst ( )-> url ( BrickLink::URL_LotsForSale, d-> m_pg-> item ( ), d-> m_pg-> color ( )));
}

void CPriceGuideWidget::doUpdate ( )
{
	if ( d-> m_pg )
		d-> m_pg-> update ( true );
	update ( );
}


void CPriceGuideWidget::gotUpdate ( BrickLink::PriceGuide *pg )
{
	if ( pg == d-> m_pg )
		update ( );
}


void CPriceGuideWidget::setPriceGuide ( BrickLink::PriceGuide *pg )
{
	if ( pg == d-> m_pg )
		return;

	if ( d-> m_pg )
		d-> m_pg-> release ( );
	if ( pg )
		pg-> addRef ( );
	
	if ( !d-> m_connected && pg )
		d-> m_connected = connect ( BrickLink::inst ( ), SIGNAL( priceGuideUpdated ( BrickLink::PriceGuide * )), this, SLOT( gotUpdate ( BrickLink::PriceGuide * )));
		
	d-> m_pg = pg;
	//update ( ); // flickers although WNoAutoErase is set !?!
	repaint ( false );
}

BrickLink::PriceGuide *CPriceGuideWidget::priceGuide ( ) const
{
	return d-> m_pg;
}

void CPriceGuideWidget::setLayout ( Layout l )
{
	if ( l != d-> m_layout ) {
		d-> m_layout = l;
		recalcLayout ( );
		update ( );
	}
}

void CPriceGuideWidget::frameChanged ( )
{
	recalcLayout ( );
	QFrame::frameChanged ( );
}
	
void CPriceGuideWidget::resizeEvent ( QResizeEvent *e )
{
	QFrame::resizeEvent ( e );
	recalcLayout ( );
}

void CPriceGuideWidget::recalcLayout ( )
{
	d-> m_cells. clear ( );
	
	const QFontMetrics &fm = fontMetrics ( );

	QFont fb = font ( );
	fb. setBold ( true );
	QFontMetrics fmb ( fb );

	QSize s = contentsRect ( ). size ( );

	switch ( d-> m_layout ) {
		case Normal    : recalcLayoutNormal     ( s, fm, fmb ); break;
		case Horizontal: recalcLayoutHorizontal ( s, fm, fmb ); break;
		case Vertical  : recalcLayoutVertical   ( s, fm, fmb ); break;
	}
}

void CPriceGuideWidget::recalcLayoutNormal ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb )
{
	int cw [4];
	int ch = fm. height ( ) + 2 * vborder;

	int dx, dy;
	
	cw [0] = 0; // not used
	cw [1] = 0;
	for ( int i = 0; i < BrickLink::ConditionCount; i++ )
		cw [1] = QMAX( cw [1], fm. width ( d-> m_str_cond [i] ));
	cw [2] = QMAX( fm. width ( d-> m_str_qty ), fm. width ( "0000 (000000)" ));
	cw [3] = fm. width ( money_t ( 9000 ). toLocalizedString ( true ));
	for ( int i = 0; i < BrickLink::PriceGuide::PriceCount; i++ )
		cw [3] = QMAX( cw [3], fm. width ( d-> m_str_price [i] ));

	for ( int i = 1; i < 4; i++ )
		cw [i] += ( 2 * hborder );

	dx = 0;
	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ )
		dx = QMAX( dx, fmb. width ( d-> m_str_vtime [i] ));

	if (( cw [1] + cw [2] + BrickLink::PriceGuide::PriceCount * cw [3] ) < dx )
		cw [1] = dx - ( cw [2] + BrickLink::PriceGuide::PriceCount * cw [3] );

	setMinimumSize ( 2 * frameWidth ( ) + cw [1] + cw [2] + BrickLink::PriceGuide::PriceCount * cw [3],
	                 2 * frameWidth ( ) + ( 1 + ( 1 + BrickLink::ConditionCount ) * BrickLink::PriceGuide::TimeCount ) * ch );

	dx = cw [1];
	dy = 0;

	d-> m_cells << cell ( cell::Header, 0, 0, cw [1], ch, Qt::AlignCenter, "$$$", true );
	d-> m_cells << cell ( cell::Header, cw [1], 0, cw [2], ch, Qt::AlignCenter, d-> m_str_qty );

	dx = cw [1] + cw [2];

	for ( int i = 0; i < BrickLink::PriceGuide::PriceCount; i++ ) {
		d-> m_cells << cell ( cell::Header, dx, 0, cw [3], ch, Qt::AlignCenter, d-> m_str_price [i] );
		dx += cw [3];
	}

	d-> m_cells << cell ( cell::Header, dx, 0, s. width ( ) - dx, ch, 0, QString::null );

	dx = cw [1] + cw [2] + cw [3] * BrickLink::PriceGuide::PriceCount;
	dy = ch;

	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ ) {
		d-> m_cells << cell ( cell::Header, 0, dy, dx, ch, Qt::AlignCenter, d-> m_str_vtime [i], true );
		dy += ch;

		for ( int j = 0; j < BrickLink::ConditionCount; j++ )
			d-> m_cells << cell ( cell::Header, 0, dy + j * ch, cw [1], ch, Qt::AlignCenter, d-> m_str_cond [j] );
		
		d-> m_cells << cell ( cell::Update, cw [1], dy, dx - cw [1], ch * BrickLink::ConditionCount, Qt::AlignCenter | Qt::WordBreak, QString::null );
		dy += ( BrickLink::ConditionCount * ch );
	}

	d-> m_cells << cell ( cell::Header, 0, dy, cw [1], s. height ( ) - dy, 0, QString::null );
	
	dy = ch;
	bool flip = false;

	for ( BrickLink::PriceGuide::Time i = BrickLink::PriceGuide::Time( 0 ); i < BrickLink::PriceGuide::TimeCount; i = BrickLink::PriceGuide::Time( i+1 )) {
		dy += ch;
	
		for ( BrickLink::Condition j = BrickLink::Condition( 0 ); j < BrickLink::ConditionCount; j = BrickLink::Condition( j+1 )) {
			dx = cw [1];

			cell c ( cell::Quantity, dx, dy, cw [2], ch, Qt::AlignRight | Qt::AlignVCenter, QString::null, flip );
			c. m_time      = i;
			c. m_condition = j;
			d-> m_cells << c;

			dx += cw [2];

			for ( BrickLink::PriceGuide::Price k = BrickLink::PriceGuide::Price( 0 ); k < BrickLink::PriceGuide::PriceCount; k = BrickLink::PriceGuide::Price( k+1 )) {
				cell c ( cell::Price, dx, dy, cw [3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString::null, flip );
				c. m_time      = i;
				c. m_condition = j;
				c. m_price     = k;
				d-> m_cells << c;
				
				dx += cw [3];
			}
			dy += ch;
			flip = !flip;
		}
	}
}


void CPriceGuideWidget::recalcLayoutHorizontal ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb )
{
	int cw [4];
	int ch = fm. height ( ) + 2 * vborder;

	int dx, dy;

	cw [0] = 0;
	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ )
		cw [0] = QMAX( cw [0], QMAX( fmb. width ( d-> m_str_htime [i][0] ), fmb. width ( d-> m_str_htime [i][1] )));
	cw [1] = 0;
	for ( int i = 0; i < BrickLink::ConditionCount; i++ )
		cw [1] = QMAX( cw [1], fm. width ( d-> m_str_cond [i] ));
	cw [2] = QMAX( fm. width ( d-> m_str_qty ), fm. width ( "0000 (000000)" ));
	cw [3] = fm. width ( money_t ( 9000 ). toLocalizedString ( true ));
	for ( int i = 0; i < BrickLink::PriceGuide::PriceCount; i++ )
		cw [3] = QMAX( cw [3], fm. width ( d-> m_str_price [i] ));

	for ( int i = 0; i < 4; i++ )
		cw [i] += ( 2 * hborder );

	setMinimumSize ( 2 * frameWidth ( ) + cw [0] + cw [1] + cw [2] + BrickLink::PriceGuide::PriceCount * cw [3],
	                 2 * frameWidth ( ) + ( 1 + BrickLink::ConditionCount * BrickLink::PriceGuide::TimeCount ) * ch );

	dx = cw [0] + cw [1];
	dy = 0;

	d-> m_cells << cell ( cell::Header, 0,  dy, dx,     ch, Qt::AlignCenter, "$$$", true );
	d-> m_cells << cell ( cell::Header, dx, dy, cw [2], ch, Qt::AlignCenter, d-> m_str_qty );

	dx += cw [2];

	for ( int i = 0; i < BrickLink::PriceGuide::PriceCount; i++ ) {
		d-> m_cells << cell ( cell::Header, dx, dy, cw [3], ch, Qt::AlignCenter, d-> m_str_price [i] );
		dx += cw [3];
	}

	d-> m_cells << cell ( cell::Header, dx, dy, s. width ( ) - dx, ch, 0, QString::null );

	dx = 0;
	dy = ch;

	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ ) {
		d-> m_cells << cell ( cell::Header, dx, dy, cw [0], BrickLink::ConditionCount * ch, Qt::AlignLeft | Qt::AlignVCenter, d-> m_str_htime [i][0] + "\n" + d-> m_str_htime [i][1], true );

		for ( int j = 0; j < BrickLink::ConditionCount; j++ ) {
			d-> m_cells << cell ( cell::Header, dx + cw [0], dy, cw [1], ch, Qt::AlignCenter, d-> m_str_cond [j] );
			dy += ch;
		}
	}

	d-> m_cells << cell ( cell::Header, dx, dy, cw [0] + cw [1], s. height ( ) - dy, 0, QString::null );

	d-> m_cells << cell ( cell::Update, cw [0] + cw [1], ch, cw [2] + BrickLink::PriceGuide::PriceCount * cw [3], BrickLink::PriceGuide::TimeCount * BrickLink::ConditionCount * ch, Qt::AlignCenter | Qt::WordBreak, QString::null );

	dy = ch;
	bool flip = false;

	for ( BrickLink::PriceGuide::Time i = BrickLink::PriceGuide::Time( 0 ); i < BrickLink::PriceGuide::TimeCount; i = BrickLink::PriceGuide::Time( i+1 )) {
		for ( BrickLink::Condition j = BrickLink::Condition( 0 ); j < BrickLink::ConditionCount; j = BrickLink::Condition( j+1 )) {
			dx = cw [0] + cw [1];

			cell c ( cell::Quantity, dx, dy, cw [2], ch, Qt::AlignRight | Qt::AlignVCenter, QString::null, flip );
			c. m_time      = i;
			c. m_condition = j;
			d-> m_cells << c;
			dx += cw [2];

			for ( BrickLink::PriceGuide::Price k = BrickLink::PriceGuide::Price( 0 ); k < BrickLink::PriceGuide::PriceCount; k = BrickLink::PriceGuide::Price( k+1 )) {
				cell c ( cell::Price, dx, dy, cw [3], ch, Qt::AlignRight  | Qt::AlignVCenter, QString::null, flip );
				c. m_time      = i;
				c. m_condition = j;
				c. m_price     = k;
				d-> m_cells << c;
				dx += cw [3];
			}
			dy += ch;
			flip = !flip;
		}
	}
}


void CPriceGuideWidget::recalcLayoutVertical ( const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb )
{
	int cw [2];
	int ch = fm. height ( ) + 2 * vborder;

	int dx, dy;
	
	cw [0] = fm. width ( d-> m_str_qty );

	for ( int i = 0; i < BrickLink::PriceGuide::PriceCount; i++ )
		cw [0] = QMAX( cw [0], fm. width ( d-> m_str_price [i] ));
	cw [0] += 2 * hborder;

	cw [1] = QMAX( fm. width ( money_t ( 9000 ). toLocalizedString ( true )), fm. width ( "0000 (000000)" ));
	for ( int i = 0; i < BrickLink::ConditionCount; i++ )
		cw [1] = QMAX( cw [1], fm. width ( d-> m_str_cond [i] ));
	cw [1] += 2 * hborder;

	dx = 0;
	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ )
		dx = QMAX( dx, fmb. width ( d-> m_str_vtime [i] ));

	if ( dx > ( cw [0] + BrickLink::ConditionCount * cw [1] )) {
		dx -= ( cw [0] + BrickLink::ConditionCount * cw [1] );
		dx = ( dx + BrickLink::ConditionCount ) / ( BrickLink::ConditionCount + 1 );

		cw [0] += dx;
		cw [1] += dx;
	}
	setMinimumSize ( 2 * frameWidth ( ) + cw [0] + BrickLink::ConditionCount * cw [1],
	                 2 * frameWidth ( ) + ( 1 + BrickLink::PriceGuide::TimeCount * ( 2 + BrickLink::PriceGuide::PriceCount )) * ch );

	dx = 0;
	dy = 0;

	d-> m_cells << cell ( cell::Header, dx, dy, cw [0], ch, Qt::AlignCenter, "$$$", true );
	dx += cw [0];
	
	for ( int i = 0; i < BrickLink::ConditionCount; i++ ) {
		d-> m_cells << cell ( cell::Header, dx, dy, cw [1], ch, Qt::AlignCenter, d-> m_str_cond [i] );
		dx += cw [1];
	}

	d-> m_cells << cell ( cell::Header, dx, dy, s. width ( ) - dx, ch, 0, QString::null );
	dy += ch;

	d-> m_cells << cell ( cell::Empty, dx, dy, s. width ( ) - dx, s. height ( ) - dy, 0, QString::null );

	dx = 0;
	dy = ch;

	for ( int i = 0; i < BrickLink::PriceGuide::TimeCount; i++ ) {
		d-> m_cells << cell ( cell::Header, dx, dy, cw [0] + BrickLink::ConditionCount * cw [1], ch, Qt::AlignCenter, d-> m_str_vtime [i], true );
		dy += ch;

		d-> m_cells << cell ( cell::Header, dx, dy, cw [0], ch, Qt::AlignLeft | Qt::AlignVCenter, d-> m_str_qty, false );
		
		d-> m_cells << cell ( cell::Update, dx + cw [0], dy, BrickLink::ConditionCount * cw [1], ( 1 + BrickLink::PriceGuide::PriceCount ) * ch, Qt::AlignCenter | Qt::WordBreak, QString::null );
		dy += ch;

		for ( int j = 0; j < BrickLink::PriceGuide::PriceCount; j++ ) {
			d-> m_cells << cell ( cell::Header, dx, dy, cw [0], ch, Qt::AlignLeft | Qt::AlignVCenter, d-> m_str_price [j], false );
			dy += ch;
		}
	}
	d-> m_cells << cell ( cell::Header, dx, dy, cw [0], s. height ( ) - dy, 0, QString::null );
	
	d-> m_cells << cell ( cell::Empty, dx + cw [0], dy, BrickLink::ConditionCount * cw [1], s. height ( ) - dy, 0, QString::null );


	dx = cw [0];

	for ( BrickLink::Condition j = BrickLink::Condition( 0 ); j < BrickLink::ConditionCount; j = BrickLink::Condition( j+1 )) {
		dy = ch;

		for ( BrickLink::PriceGuide::Time i = BrickLink::PriceGuide::Time( 0 ); i < BrickLink::PriceGuide::TimeCount; i = BrickLink::PriceGuide::Time( i+1 )) {
			dy += ch;

			cell c ( cell::Quantity, dx, dy, cw [1], ch, Qt::AlignRight | Qt::AlignVCenter, QString::null, false );
			c. m_time = i;
			c. m_condition = j;
			d-> m_cells << c;
			dy += ch;

			bool flip = true;

			for ( BrickLink::PriceGuide::Price k = BrickLink::PriceGuide::Price( 0 ); k < BrickLink::PriceGuide::PriceCount; k = BrickLink::PriceGuide::Price( k+1 )) {
				cell c ( cell::Price, dx, dy, cw [1], ch, Qt::AlignRight | Qt::AlignVCenter, QString::null, flip );
				c. m_time = i;
				c. m_condition = j;
				c. m_price = k;
				d-> m_cells << c;
				dy += ch;
				flip = !flip;
			}
		}
		dx += cw [1];
	}
}



void CPriceGuideWidget::paintHeader ( QPainter *p, const QSize &s, const QRect &r, const QColorGroup &cg, int align, const QString &str, bool bold )
{
	//style ( ). drawPrimitive ( QStyle::PE_HeaderSection, p, r, cg );

	// vvv Official Qt3 hack to paint disabled headers (see qheader.cpp) vvv
	p-> setBrushOrigin ( r. topLeft ( ));	
	p-> save ( );
	p-> setClipRect ( r );
	
	QRect r1 ( r );
	r1. addCoords ( -2, -2, 2, 2 );

	style ( ). drawPrimitive ( QStyle::PE_HeaderSection, p, r1, cg, QStyle::Style_Horizontal | QStyle::Style_Off | QStyle::Style_Enabled | QStyle::Style_Raised, QStyleOption ( this ));
		
	p-> setPen ( cg. color ( QColorGroup::Mid ));
	if ( r. y ( ) + r. height ( ) < s. height ( ))
		p-> drawLine ( r. x ( ), r. y ( ) + r. height ( ) - 1, r. x ( ) + r. width ( ) - 1, r. y ( ) + r. height ( ) - 1 );
	if ( r. x ( ) + r. width ( ) < s. width ( ))
		p-> drawLine ( r. x ( ) + r. width ( ) - 1, r. y ( ), r. x ( ) + r. width ( ) - 1, r. y ( ) + r. height ( ) - 1 );
	p-> setPen ( cg. color ( QColorGroup::Light ));
	if ( r. y ( ) > 0 )
		p-> drawLine ( r. x ( ), r. y ( ), r. x ( ) + r. width ( ) - 1, r. y ( ));		
	if ( r. x ( ) > 0 )
		p-> drawLine ( r. x ( ), r. y ( ), r. x ( ), r. y ( ) + r. height ( ) - 1 );

	p-> restore ( );
	// ^^^ Official Qt3 hack to paint disabled headers (see qheader.cpp) ^^^
	
	if ( !str. isEmpty ( )) {
		QRect r2 ( r );
		r2. addCoords ( hborder, 0, -hborder, 0 );
		
		QFont savefont;

		if ( bold ) {
			savefont = font ( );
			QFont fb ( savefont );
			fb. setBold ( true );
			p-> setFont ( fb );
		}

		p-> setPen ( cg. text ( ));
		p-> drawText ( r2, align, str );

		if ( bold )
			p-> setFont ( savefont );
	}
}

void CPriceGuideWidget::paintCell ( QPainter *p, const QRect &r, const QColorGroup &cg, int align, const QString &str, bool alternate )
{
	p-> fillRect ( r, !alternate ? cg. base ( ) : CUtility::contrastColor ( cg. base ( )));

	QRect r2 ( r );
	r2. addCoords ( hborder, 0, -hborder, 0 );

	p-> setPen ( cg. text ( ));
	p-> drawText ( r2, align, str );
}

void CPriceGuideWidget::drawContents ( QPainter *p )
{
	const QSize s = contentsRect ( ). size ( );
	const QPoint offset = contentsRect ( ). topLeft ( );
	
	const QColorGroup &cg = colorGroup ( );
	QPixmap pix ( s );
	QPainter ppix;
//	int fw = frameWidth ( );

	ppix. begin ( &pix, this );
	ppix. fillRect ( rect ( ), colorGroup ( ). base ( ));

	bool valid = d-> m_pg && d-> m_pg-> valid ( );
	bool is_updating = d-> m_pg && ( d-> m_pg-> updateStatus ( ) == BrickLink::Updating );
	
	QString str = d-> m_pg ? "-" : "";

	for ( QValueList<cell>::const_iterator it = d-> m_cells. begin ( ); it != d-> m_cells. end ( ); ++it ) {
		const cell &c = *it;

		switch ( c. m_type ) {
			case cell::Header:
				paintHeader ( &ppix, s, c, cg, c. m_text_flags, c. m_text == "$$$" ? CMoney::inst ( )-> currencySymbol ( ) : c. m_text, c. m_flag );
				break;

			case cell::Quantity:
				if ( !is_updating ) {
					if ( valid )
						str = QString ( "%1 (%2)" ). arg ( d-> m_pg-> quantity ( c. m_time, c. m_condition )). arg ( d-> m_pg-> lots ( c. m_time, c. m_condition ));
					
					paintCell ( &ppix, c, cg, c. m_text_flags, str, c. m_flag );
				}
				break;
			
			case cell::Price:
				if ( !is_updating ) {
					if ( valid )
						str = d-> m_pg-> price ( c. m_time, c. m_condition, c. m_price ). toLocalizedString ( true );
						
					paintCell ( &ppix, c, cg, c. m_text_flags, str, c. m_flag );
				}
				break;

			case cell::Update:
				if ( is_updating )
					paintCell ( &ppix, c, cg, c. m_text_flags, d-> m_str_wait, c. m_flag );
				break;

			case cell::Empty:
				ppix. fillRect ( c, cg. base ( ));
				break;
		}
	}
	ppix. end ( );
	
	QRect srcrect = pix. rect ( );
/*
	if ( p-> hasClipping ( )) {
		QRect cliprect = p-> clipRegion ( ). boundingRect ( );
		cliprect. moveBy ( -offset. x ( ), -offset. y ( ));

		if ( cliprect. isValid ( ) && !cliprect. isEmpty ( ))
			srcrect &= cliprect;
	}*/
	bitBlt ( p-> device ( ), offset, &pix, srcrect );
}

void CPriceGuideWidget::mouseDoubleClickEvent ( QMouseEvent *me )
{
	if ( !d-> m_pg )
		return;

	QValueList<cell>::const_iterator it = d-> cellAtPos ( me-> pos ( ));

	if (( it != d-> m_cells. end ( )) && ((*it). m_type == cell::Price ))
		emit priceDoubleClicked ( d-> m_pg-> price ((*it). m_time, (*it). m_condition, (*it). m_price ));
}

void CPriceGuideWidget::mouseMoveEvent ( QMouseEvent *me )
{
	if ( !d-> m_pg )
		return;

	QValueList<cell>::const_iterator it = d-> cellAtPos ( me-> pos ( ));

	d-> m_on_price = (( it != d-> m_cells. end ( )) && ((*it). m_type == cell::Price ));

	if ( d-> m_on_price )
		setCursor ( Qt::PointingHandCursor );
	else
		unsetCursor ( );
}

void CPriceGuideWidget::leaveEvent ( QEvent * /*e*/ )
{
	if ( d-> m_on_price ) {
		d-> m_on_price = false;
		unsetCursor ( );
	}
}

