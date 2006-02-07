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
#include <qvalidator.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qheader.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qlistbox.h>
#include <qcursor.h>
#include <qtooltip.h>
#include <qstyle.h>
#include <qintdict.h>

#include "cmessagebox.h"
#include "cutility.h"
#include "cresource.h"
#include "cmoney.h"
#include "cselectitem.h"
#include "cselectcolor.h"
#include "cconfig.h"

#include "citemview.h"

class CItemViewToolTip;


class CItemViewPrivate {
public:
	QColor    m_shades_table [13];
	QPixmap   m_pixmap_nopicture;
	QPixmap   m_pixmap_status [BrickLink::InvItem::Unknown + 1];

	QIntDict <QPixmap> m_pixmap_color;

	QWidget *          m_active_editor;
	CItemViewItem *    m_active_item;
	int                m_active_column;
	QValidator *       m_active_validator;

	QLineEdit *        m_lineedit;
	CItemViewToolTip  *m_tooltips;

	CDocument *        m_doc;
	bool               m_diff_mode;
	bool               m_simple_mode;


	CItemViewPrivate ( CItemView *view, CDocument *doc )
	{
		m_active_editor = 0;
		m_active_item = 0;
		m_active_column = 0;
		m_active_validator = 0;

		m_lineedit = 0;
		m_tooltips = 0;

		m_doc = doc;
		m_diff_mode = false;
		m_simple_mode = false;

		for ( int i = 0; i < 13; i++ )
			m_shades_table [i] = QColor ( i == 0 ? -1 : ( i - 1 ) * 30, 255, 255, QColor::Hsv );

		m_pixmap_status [BrickLink::InvItem::Include] = QPixmap ( CResource::inst ( )-> pixmap ( "status_include" ));
		m_pixmap_status [BrickLink::InvItem::Exclude] = QPixmap ( CResource::inst ( )-> pixmap ( "status_exclude" ));
		m_pixmap_status [BrickLink::InvItem::Extra]   = QPixmap ( CResource::inst ( )-> pixmap ( "status_extra" ));
		m_pixmap_status [BrickLink::InvItem::Unknown] = QPixmap ( CResource::inst ( )-> pixmap ( "status_unkown" ));

		m_pixmap_nopicture = QPixmap ( 80, 60 );
		m_pixmap_nopicture. fill ( Qt::white );

		QBitmap mask ( m_pixmap_nopicture. size ( ));
		mask. fill ( Qt::color0 );

		m_pixmap_nopicture. setMask ( mask );

		const QIntDict<BrickLink::Color> &coldict = BrickLink::inst ( )-> colors ( );
		int h = QMAX( 30, view-> fontMetrics ( ). height ( ));
		int w = QMAX( 15, h / 2);

		for ( QIntDictIterator<BrickLink::Color> it ( coldict ); it. current ( ); ++it ) {
			const BrickLink::Color *color = it. current ( );

			QPixmap *pix = new QPixmap ( );
			pix-> convertFromImage ( BrickLink::inst ( )-> colorImage ( color, w, h ));
			
			m_pixmap_color. insert ( color-> id ( ), pix );
		}
		m_pixmap_color. setAutoDelete ( true );
	}
};

class CItemViewToolTip : public QToolTip {
public:
	CItemViewToolTip ( QWidget *parent, CItemView *iv )
		: QToolTip( parent ), m_iv ( iv )
	{ }
	
	virtual ~CItemViewToolTip ( )
	{ }

    void maybeTip ( const QPoint &pos )
	{
		if ( !parentWidget ( ) || !m_iv /*|| !m_iv-> showToolTips ( )*/ )
			return;

		CItemViewItem *item = static_cast <CItemViewItem *> ( m_iv-> itemAt ( pos ));
		QPoint contentpos = m_iv-> viewportToContents ( pos );
		
		if ( !item )
			return;

		int col = m_iv-> header ( )-> sectionAt ( contentpos. x ( ));
		QString text = item-> toolTip ( col );

		if ( text. isEmpty ( ))
			return;

		QRect r = m_iv-> itemRect ( item );
		int headerleft = m_iv-> header ( )-> sectionPos ( col ) - m_iv-> contentsX ( );
		r. setLeft ( headerleft );
		r. setRight ( headerleft + m_iv-> header ( )-> sectionSize ( col ));
		tip ( r, text );
	}

private:
    CItemView *m_iv;
};


CItemView::CItemView ( CDocument *doc, QWidget *parent, const char *name )
	: CListView ( parent, name )
{
	d = new CItemViewPrivate ( this, doc );

	setGridMode ( true );

	d-> m_lineedit = new QLineEdit ( viewport ( ));
    d-> m_lineedit-> setFrameStyle ( QFrame::Box | QFrame::Plain );
    d-> m_lineedit-> setLineWidth ( 1 );
	d-> m_lineedit-> hide ( );
	d-> m_lineedit-> installEventFilter ( this );

	setSelectionMode ( QListView::Extended );
	setAlwaysShowSelection ( true );
	setShowToolTips ( false );
	d-> m_tooltips = new CItemViewToolTip ( viewport ( ), this );

	for ( int i = 0; i < FieldCount; i++ ) {
		int align = AlignLeft;
		QString t;
		int width = 0;
		int hidden = 0;

		switch ( i ) {
			case Status      : align = AlignCenter; width = -16; break;
			case Picture     : align = AlignCenter; width = -40; break;
			case PartNo      : width = 10; break;
			case Description : width = 28; break;
			case Comments    : width = 8; break;
			case Remarks     : width = 8; break;
			case QuantityOrig: width = 5; hidden = 2; break;
			case QuantityDiff: width = 5; hidden = 2; break;
			case Quantity    : width = 5; break;
			case Bulk        : width = 5; break;
			case PriceOrig   : width = 8; align = AlignRight; hidden = 2; break;
			case PriceDiff   : width = 8; align = AlignRight; hidden = 2; break;
			case Price       : width = 8; align = AlignRight; break;
			case Total       : width = 8; align = AlignRight; break;
			case Sale        : width = 5; align = AlignRight; break;
			case Condition   : width = 5; align = AlignHCenter; break;
			case Color       : width = 15; break;
			case Category    : width = 12; break;
			case ItemType    : width = 12; break;
			case TierQ1      : width = 5; break;
			case TierP1      : width = 8; align = AlignRight; break;
			case TierQ2      : width = 5; break;
			case TierP2      : width = 8; align = AlignRight; break;
			case TierQ3      : width = 5; break;
			case TierP3      : width = 8; align = AlignRight; break;
			case LotId       : align = AlignLeft; width = 8; hidden = 1; break;
			case Retain      : align = AlignCenter; width = 8; hidden = 1; break;
			case Stockroom   : align = AlignCenter; width = 8; hidden = 1; break;
			case Reserved    : width = 8; hidden = 1; break;
			case Weight      : align = AlignRight; width = 6; hidden = 1; break;
			case YearReleased: width = 5; hidden = 1; break;
		}
		int cid = addColumn ( t );
		setColumnAlignment ( cid, align );
		setColumnWidthMode ( cid, QListView::Manual );
		setColumnWidth ( cid, 2 * itemMargin ( ) + 3 + ( width <= 0 ? -width : width * fontMetrics ( ). width ( "0" )));

		if ( hidden )
			hideColumn ( cid, ( hidden == 2 ));
	}

	connect ( this, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( listItemDoubleClicked ( QListViewItem *, const QPoint &, int )));

	connect ( this, SIGNAL( contentsMoving ( int, int )), this, SLOT( cancelEdit ( )));
	connect ( this, SIGNAL( horizontalSliderPressed ( )), this, SLOT( cancelEdit ( )));
	connect ( this, SIGNAL( verticalSliderPressed ( )),   this, SLOT( cancelEdit ( )));

	languageChange ( );
}

void CItemView::languageChange ( )
{
	for ( int i = 0; i < FieldCount; i++ ) {
		QString t;

		switch ( i ) {
			case Status      : t = tr( "Status" ); break;
			case Picture     : t = tr( "Image" ); break;
			case PartNo      : t = tr( "PartNo" ); break;
			case Description : t = tr( "Description" ); break;
			case Comments    : t = tr( "Comments" ); break;
			case Remarks     : t = tr( "Remarks" ); break;
			case QuantityOrig: t = tr( "Qty. Orig" ); break;
			case QuantityDiff: t = tr( "Qty. Diff" ); break;
			case Quantity    : t = tr( "Qty." ); break;
			case Bulk        : t = tr( "Bulk" ); break;
			case PriceOrig   : t = tr( "Pr. Orig" ); break;
			case PriceDiff   : t = tr( "Pr. Diff" ); break;
			case Price       : t = tr( "Price" ); break;
			case Total       : t = tr( "Total" ); break;
			case Sale        : t = tr( "Sale" ); break;
			case Condition   : t = tr( "Cond." ); break;
			case Color       : t = tr( "Color" ); break;
			case Category    : t = tr( "Category" ); break;
			case ItemType    : t = tr( "Item Type" ); break;
			case TierQ1      : t = tr( "Tier Q1" ); break;
			case TierP1      : t = tr( "Tier P1" ); break;
			case TierQ2      : t = tr( "Tier Q2" ); break;
			case TierP2      : t = tr( "Tier P2" ); break;
			case TierQ3      : t = tr( "Tier Q3" ); break;
			case TierP3      : t = tr( "Tier P3" ); break;
			case LotId       : t = tr( "Lot Id" ); break;
			case Retain      : t = tr( "Retain" ); break;
			case Stockroom   : t = tr( "Stockroom" ); break;
			case Reserved    : t = tr( "Reserved" ); break;
			case Weight      : t = tr( "Weight" ); break;
			case YearReleased: t = tr( "Year" ); break;
		}
		setColumnText ( i, t );
	}
	triggerUpdate ( );
}

CItemView::~CItemView ( )
{
	delete d-> m_tooltips;
	delete d;
}

QString CItemView::statusLabel ( BrickLink::InvItem::Status status )
{
	QString str;

	switch ( status ) {
		case BrickLink::InvItem::Exclude: str = CItemView::tr( "Exclude" ); break;
		case BrickLink::InvItem::Extra  : str = CItemView::tr( "Extra" ); break;
		case BrickLink::InvItem::Include: str = CItemView::tr( "Include" ); break;
		default                         : break;
	}
	return str;
}

QString CItemView::conditionLabel ( BrickLink::Condition cond )
{
	QString str;

	switch ( cond ) {
		case BrickLink::New : str = CItemView::tr( "New" ); break;
		case BrickLink::Used: str = CItemView::tr( "Used" ); break;
		default             : break;
	}
	return str;
}

bool CItemView::isSimpleMode ( ) const
{
	return d-> m_simple_mode;
}

void CItemView::setSimpleMode ( bool b )
{
	d-> m_simple_mode = b;

	if ( b ) {
		hideColumn ( Bulk, true );
		hideColumn ( Sale, true );
		hideColumn ( TierQ1, true );
		hideColumn ( TierQ2, true );
		hideColumn ( TierQ3, true );
		hideColumn ( TierP1, true );
		hideColumn ( TierP2, true );
		hideColumn ( TierP3, true );
		hideColumn ( Reserved, true );
		hideColumn ( Stockroom, true );
		hideColumn ( Retain, true );
		hideColumn ( LotId, true );
		hideColumn ( Remarks, true );
	}
	else {
		showColumn ( Bulk );
		showColumn ( Sale );
		showColumn ( TierQ1 );
		showColumn ( TierQ2 );
		showColumn ( TierQ3 );
		showColumn ( TierP1 );
		showColumn ( TierP2 );
		showColumn ( TierP3 );
		showColumn ( Reserved );
		showColumn ( Stockroom );
		showColumn ( Retain );
		showColumn ( LotId );
		showColumn ( Remarks );
	}
}

bool CItemView::isDifferenceMode ( ) const
{
	return d-> m_diff_mode;
}

void CItemView::setDifferenceMode ( bool b )
{
	d-> m_diff_mode = b;

	if ( b ) {
		showColumn ( QuantityDiff );
		showColumn ( QuantityOrig );

		if ( isColumnVisible ( Quantity )) {
			header ( )-> moveSection ( QuantityDiff, header ( )-> mapToIndex ( Quantity ));
			header ( )-> moveSection ( QuantityOrig, header ( )-> mapToIndex ( QuantityDiff ));
		}

		showColumn ( PriceDiff );
		showColumn ( PriceOrig );
		
		if ( isColumnVisible ( Price )) {
			header ( )-> moveSection ( PriceDiff, header ( )-> mapToIndex ( Price ));
			header ( )-> moveSection ( PriceOrig, header ( )-> mapToIndex ( PriceDiff ));
		}
	}
	else {
		hideColumn ( PriceOrig, true );
		hideColumn ( PriceDiff, true );
		hideColumn ( QuantityOrig, true );
		hideColumn ( QuantityDiff, true );
	}
}

void CItemView::applyFilter ( const QString &filter, int field, bool is_regex )
{
	QRegExp regexp;

	if ( is_regex )
		regexp = QRegExp ( filter, false, true );

	field = ( field < FilterCountSpecial ) ? -field - 1 : field - FilterCountSpecial;

	QApplication::setOverrideCursor ( QCursor( Qt::WaitCursor ));

	CDisableUpdates disupd ( this );

	for ( QListViewItemIterator it ( this ); it. current ( ); ++it ) {
		QListViewItem *ivi = it. current ( );

		bool b = regexp. isEmpty ( ) || !regexp. isValid ( );

		if ( !b && ( field >= 0 ) && ( field < CItemView::FieldCount )) { // normal field
			b = ( regexp. search ( ivi-> text ( field )) >= 0 );
		}
		else if ( !b && ( field < 0 )) { // special field
			int fieldindex = -field - 1;

			static int matchfields [FilterCountSpecial][CItemView::FieldCount + 1] = {
				{ /*All       */ CItemView::PartNo, CItemView::Description, CItemView::Comments, CItemView::Remarks, CItemView::Quantity, CItemView::Price, CItemView::Color, CItemView::Category, CItemView::ItemType, CItemView::TierQ1, CItemView::TierP1, CItemView::TierQ2, CItemView::TierP2, CItemView::TierQ3, CItemView::TierP3, CItemView::Weight, CItemView::YearReleased, -1 },
				{ /*Prices    */ CItemView::Price, CItemView::TierP1, CItemView::TierP2, CItemView::TierP3, CItemView::Total, -1 },
				{ /*Texts     */ CItemView::Description, CItemView::Comments, CItemView::Remarks, -1 },
				{ /*Quantities*/ CItemView::Quantity, CItemView::TierQ1, CItemView::TierQ2, CItemView::TierQ3, -1 },
			};

			for ( int i = 0; matchfields [fieldindex][i] != -1; i++ ) {
				b |= ( regexp. search ( QString ( ivi-> text ( matchfields [fieldindex][i] ))) >= 0 );
			}
		}

		if ( !b && ivi-> isSelected ( ))
			setSelected ( ivi, false );

		ivi-> setVisible ( b );
	}
	QApplication::restoreOverrideCursor ( );
}

void CItemView::editWithLineEdit ( CItemViewItem *ivi, int col, const QString &text, const QString &mask, QValidator *valid )
{
	if ( !ivi || ( ivi-> listView ( ) != this ) || ( col < 0 ) || ( col >= columns ( )))
		return;

	d-> m_active_validator = valid;

	d-> m_lineedit-> setAlignment ( columnAlignment ( col ));
	d-> m_lineedit-> setValidator ( valid );
	d-> m_lineedit-> setInputMask ( mask );
	d-> m_lineedit-> setText ( text );
	d-> m_lineedit-> selectAll ( );

	edit ( ivi, col, d-> m_lineedit );
}

void CItemView::cancelEdit ( )
{
	terminateEdit ( false );
}

void CItemView::terminateEdit ( bool commit )
{
	if ( d-> m_active_editor ) {
		QWidget *w = d-> m_active_editor;
		d-> m_active_editor = 0;

		//setFocusProxy ( 0 );
		setFocus ( );

		w-> hide ( ); // will call FocusOut which will call us again!

		if ( commit ) {
			if ( w == d-> m_lineedit ) {
				emit editDone ( d-> m_active_item, d-> m_active_column, d-> m_lineedit-> text ( ), d-> m_lineedit-> hasAcceptableInput ( ));

				d-> m_active_item-> editDone ( d-> m_active_column, d-> m_lineedit-> text ( ), d-> m_lineedit-> hasAcceptableInput ( ));
			}
		}
		else {
			emit editCanceled ( d-> m_active_item, d-> m_active_column );
		}
		d-> m_active_item = 0;
		d-> m_active_column = 0;

		d-> m_lineedit-> setValidator ( 0 );
		d-> m_lineedit-> setInputMask ( QString::null );
		delete d-> m_active_validator;
		d-> m_active_validator = 0;
	}
}

void CItemView::edit ( CItemViewItem *ivi, int col, QWidget *w )
{
	if ( d-> m_active_editor )
		terminateEdit ( false );

	d-> m_active_item = ivi;
	d-> m_active_column = col;
	d-> m_active_editor = w;

	QRect rect = itemRect ( ivi );
	int offset = header ( )-> sectionPos ( col );

	if ( col == 0 )
		offset += treeStepSize ( ) * ( ivi-> depth ( ) + ( rootIsDecorated ( ) ? 1 : 0 ));

	if ( ivi-> pixmap ( col ))
		offset += ivi-> pixmap ( col )-> width ( );

	rect. rLeft ( ) += offset;
	rect. rRight ( ) = rect. rLeft ( ) + columnWidth ( col ) - 1;
	rect. addCoords ( -1, -1, 2, 2 );

	w-> setGeometry ( rect );

	//setFocusProxy ( w );
	w-> show ( );
	w-> setFocus ( );
}


void CItemView::listItemDoubleClicked ( QListViewItem *item, const QPoint &p, int col )
{
	if ( !item || ( col < 0 ) || ( col >= columns ( )))
		return;

	if ( item-> rtti ( ) == CItemViewItem::RTTI )
		static_cast <CItemViewItem *> ( item )-> doubleClicked ( p, col );
}


bool CItemView::eventFilter ( QObject *o, QEvent *e )
{
	bool res = false;

	if ( o == d-> m_lineedit ) {
		switch ( e-> type ( )) {
			case QEvent::FocusOut: {
				QFocusEvent *fe = static_cast <QFocusEvent *> ( e );
				fe = fe; // MSVC.NET
				// Don't let a RMB close the editor
				if ( fe->reason ( ) != QFocusEvent::Popup && fe->reason ( ) != QFocusEvent::ActiveWindow )
					terminateEdit ( defaultRenameAction ( ) == Accept );
				break;
			}
			case QEvent::KeyPress: {
				QKeyEvent *ke = static_cast <QKeyEvent *> ( e );

				if ( ke-> key ( ) == Qt::Key_Return || ke-> key ( ) == Qt::Key_Enter ) {
					ke-> accept ( );
					terminateEdit ( true );
					setFocus ( );
					res = true; // eat the event, to break a endless loop
				}
				else if ( ke-> key ( ) == Qt::Key_Escape ) {
					terminateEdit ( false );
					setFocus ( );
				}
				else if ( ke-> key ( ) == Qt::Key_Down || ke-> key ( ) == Qt::Key_Up ) {
					terminateEdit ( true /*defaultRenameAction ( ) == Accept*/ );
					setFocus ( );
				}
				break;
			}
			default:
				break;
		}
	}

	return res ? true : CListView::eventFilter ( o, e );
}


// ---------------------------------------------------------------------------


CItemViewItem::CItemViewItem ( CDocument::Item *item, QListViewItem *parent, QListViewItem *after )
	: CListViewItem ( parent, after )
{
	init ( item );
}

CItemViewItem::CItemViewItem ( CDocument::Item *item, QListView *parent, QListViewItem *after )
	: CListViewItem ( parent, after )
{
	init ( item );
}

CItemViewItem::~CItemViewItem ( )
{
	if ( m_picture )
		m_picture-> release ( );
}

void CItemViewItem::init ( CDocument::Item *item )
{
	m_item = item;
	m_picture = 0;

	m_truncated = ~0;
}

CDocument::Item *CItemViewItem::item ( ) const
{
	return m_item;
}

BrickLink::Picture *CItemViewItem::picture ( ) const
{
	return m_picture;
}

QColor CItemViewItem::shadeColor ( int index ) const
{
	return listView ( )-> d-> m_shades_table [index % ( sizeof( listView ( )-> d-> m_shades_table ) / sizeof( listView ( )-> d-> m_shades_table [0] ))];
}

int CItemViewItem::width ( const QFontMetrics &fm, const QListView *lv, int column ) const
{
	int w = 0;

	switch ( column ) {
		case CItemView::Status      : w = pixmap ( column )-> width ( ); break;
		case CItemView::Picture     : w = 40; break;
		case CItemView::Retain      :
		case CItemView::Stockroom   : w = lv-> style ( ). pixelMetric ( QStyle::PM_IndicatorWidth ); break;
		default                     : w = CListViewItem::width ( fm, lv, column ); break;
	}
	return w + 3;
}

QString CItemViewItem::toolTip ( int column ) const
{
	QString str;

	switch ( column ) {
		case CItemView::Status      : str = CItemView::statusLabel ( m_item-> status ( )); break;
		case CItemView::Picture     : str = text ( CItemView::PartNo ) + " " + text ( CItemView::Description ); break;
		case CItemView::Condition   : str = CItemView::conditionLabel ( m_item-> condition ( )); break;
		case CItemView::Category    : {
			const BrickLink::Category **catpp = m_item-> item ( )-> allCategories ( );
			
			if ( !catpp [1] ) {
				str = catpp [0]-> name ( );
			}
			else {
				str = QString( "<b>%1</b>" ). arg( catpp [0]-> name ( ));
				while ( *++catpp )
					str = str + QString( "<br />" ) + catpp [0]-> name ( );
			}
			break;
		}

		default                     : if ( m_truncated & (((Q_UINT64) 1ULL ) << column )) str = text ( column ); break;
	}
	return str;
}

QString CItemViewItem::text ( int col ) const
{
	static QString dash ( "-" );
	QString str;

	switch ( col ) {
		case CItemView::Status      :
		case CItemView::Picture     :
		case CItemView::Retain      :
		case CItemView::Stockroom   : break;
		case CItemView::LotId       : str = ( m_item-> lotId ( ) == 0 ? dash : QString::number ( m_item-> lotId ( ))); break;
		case CItemView::PartNo      : str = m_item-> item ( )-> id ( ); break;
		case CItemView::Description : str = m_item-> item ( )-> name ( ); break;
		case CItemView::Comments    : str = m_item-> comments ( ); break;
		case CItemView::Remarks     : str = m_item-> remarks ( ); break;
		case CItemView::Quantity    : str = QString::number ( m_item-> quantity ( )); break;
		case CItemView::Bulk        : str = ( m_item-> bulkQuantity ( ) == 1 ? dash : QString::number ( m_item-> bulkQuantity ( ))); break;
		case CItemView::Price       : str = m_item-> price ( ). toLocalizedString ( ); break;
		case CItemView::Total       : str = m_item-> total ( ). toLocalizedString ( ); break;
		case CItemView::Sale        : str = ( m_item-> sale ( ) == 0 ? dash : QString::number ( m_item-> sale ( )) + "%" ); break;
		case CItemView::Condition   : str = ( m_item-> condition ( ) == BrickLink::New ? "N" : "U" ); break;
		case CItemView::Color       : str = m_item-> color ( )-> name ( ); break;
		case CItemView::Category    : str = m_item-> category ( )-> name ( ); break;
		case CItemView::ItemType    : str = m_item-> itemType ( )-> name ( ); break;
		case CItemView::TierQ1      : str = ( m_item-> tierQuantity ( 0 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 0 ))); break;
		case CItemView::TierQ2      : str = ( m_item-> tierQuantity ( 1 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 1 ))); break;
		case CItemView::TierQ3      : str = ( m_item-> tierQuantity ( 2 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 2 ))); break;
		case CItemView::TierP1      : str = m_item-> tierPrice ( 0 ). toLocalizedString ( ); break;
		case CItemView::TierP2      : str = m_item-> tierPrice ( 1 ). toLocalizedString ( ); break;
		case CItemView::TierP3      : str = m_item-> tierPrice ( 2 ). toLocalizedString ( ); break;
		case CItemView::Reserved    : str = m_item-> reserved ( ); break;
		case CItemView::Weight      : str = ( m_item-> weight ( ) == 0 ? dash : CUtility::weightToString ( m_item-> weight ( ), ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), true, true )); break;
		case CItemView::YearReleased: str = ( m_item-> item ( )-> yearReleased ( ) == 0 ? dash : QString::number ( m_item-> item ( )-> yearReleased ( ))); break;

		case CItemView::PriceOrig   : str = m_item-> origPrice ( ). toLocalizedString ( ); break;
		case CItemView::PriceDiff   : str = ( m_item-> price ( ) - m_item-> origPrice ( )). toLocalizedString ( ); break;
		case CItemView::QuantityOrig: str = QString::number ( m_item-> origQuantity ( )); break;
		case CItemView::QuantityDiff: str = QString::number ( m_item-> quantity ( ) - m_item-> origQuantity ( )); break;
	}
	return str;
}

const QPixmap *CItemViewItem::pixmap ( int column ) const
{
	if ( column == CItemView::Status ) {
		return &listView ( )-> d-> m_pixmap_status [QMAX( 0, QMIN( m_item-> status ( ), BrickLink::InvItem::Unknown ))];
	}
	else if ( column == CItemView::Picture ) {
		if ( m_picture && (( m_picture-> item ( ) != m_item-> item ( )) || ( m_picture-> color ( ) != m_item-> color ( )))) {
			m_picture-> release ( );
			m_picture = 0;
		}

		if ( !m_picture ) {
			if ( m_item-> customPicture ( ) )
				m_picture = m_item-> customPicture ( );
			else
				m_picture = BrickLink::inst ( )-> picture ( m_item-> item ( ), m_item-> color ( ));

			if ( m_picture )
				m_picture-> addRef ( );
		}
		if ( m_picture && m_picture-> valid ( )) {
			static QPixmap val2ptr;
			
			val2ptr = m_picture-> pixmap ( );
			return &val2ptr;
		}
		else
			return &listView ( )-> d-> m_pixmap_nopicture;
	}
	else if ( column == CItemView::Color ) {
		return listView ( )-> d-> m_pixmap_color [m_item-> color ( )-> id ( )];
	}
	else
		return 0;
}

QString CItemViewItem::key ( int /*column*/, bool /*ascending*/ ) const
{
	return QString::null;
}

namespace {

template <typename T> static int cmp ( const T &a, const T &b )
{
	if ( a < b )
		return -1;
	else if ( a == b )
		return 0;
	else
		return 1;
}

} // namespace

//#define cmp(a,b)	((a)-(b) < 0 ? - 1 : ((a)-(b) > 0 ? +1 : 0 ));

int CItemViewItem::compare ( QListViewItem *i, int col, bool /*ascending*/ ) const
{
	CItemViewItem *ci = static_cast <CItemViewItem *> ( i );

	switch ( col ) {
		case CItemView::Status      : return cmp( m_item-> status ( ), (*ci-> m_item). status ( ));
		case CItemView::Picture     :
		case CItemView::PartNo      : return qstrcmp ( m_item-> item ( )-> id ( ),       (*ci-> m_item). item ( )-> id ( ));
		case CItemView::LotId       : return m_item-> lotId ( )                        - (*ci-> m_item). lotId ( );
		case CItemView::Description : return qstrcmp ( m_item-> item ( )-> name ( ),     (*ci-> m_item). item ( )-> name ( ));
		case CItemView::Comments    : return m_item-> comments ( ).            compare ( (*ci-> m_item). comments ( ));
		case CItemView::Remarks     : return m_item-> remarks ( ).             compare ( (*ci-> m_item). remarks ( ));
		case CItemView::Quantity    : return m_item-> quantity ( )                     - (*ci-> m_item). quantity ( );
		case CItemView::Bulk        : return m_item-> bulkQuantity ( )                 - (*ci-> m_item). bulkQuantity ( );
		case CItemView::Price       : return cmp( m_item-> price ( ),                    (*ci-> m_item). price ( ));
		case CItemView::Total       : return cmp( m_item-> total ( ),                    (*ci-> m_item). total ( ));
		case CItemView::Sale        : return m_item-> sale ( )                         - (*ci-> m_item). sale ( );
		case CItemView::Condition   : return cmp( m_item-> condition ( ),                (*ci-> m_item). condition ( ));
		case CItemView::Color       : return qstrcmp ( m_item-> color ( )-> name ( ),    (*ci-> m_item). color ( )-> name ( ));
		case CItemView::Category    : return qstrcmp ( m_item-> category ( )-> name ( ), (*ci-> m_item). category ( )-> name ( ));
		case CItemView::ItemType    : return qstrcmp ( m_item-> itemType ( )-> name ( ), (*ci-> m_item). itemType ( )-> name ( ));
		case CItemView::TierQ1      : return m_item-> tierQuantity ( 0 )               - (*ci-> m_item). tierQuantity ( 0 );
		case CItemView::TierQ2      : return m_item-> tierQuantity ( 1 )               - (*ci-> m_item). tierQuantity ( 1 );
		case CItemView::TierQ3      : return m_item-> tierQuantity ( 2 )               - (*ci-> m_item). tierQuantity ( 2 );
		case CItemView::TierP1      : return cmp( m_item-> tierPrice ( 0 ),              (*ci-> m_item). tierPrice ( 0 ));
		case CItemView::TierP2      : return cmp( m_item-> tierPrice ( 1 ),              (*ci-> m_item). tierPrice ( 1 ));
		case CItemView::TierP3      : return cmp( m_item-> tierPrice ( 2 ),              (*ci-> m_item). tierPrice ( 2 ));
		case CItemView::Retain      : return cmp( m_item-> retain ( ) ? 1 : 0,           (*ci-> m_item). retain ( ) ? 1 : 0 );
		case CItemView::Stockroom   : return cmp( m_item-> stockroom ( ) ? 1 : 0,        (*ci-> m_item). stockroom ( ) ? 1 : 0 );
		case CItemView::Reserved    : return m_item-> reserved ( ).            compare ( (*ci-> m_item). reserved ( ));
		case CItemView::Weight      : return cmp( m_item-> weight ( ),                   (*ci-> m_item). weight ( ));
		case CItemView::YearReleased: return cmp( m_item-> item ( )-> yearReleased ( ),  (*ci-> m_item). item ( )-> yearReleased ( ));

		case CItemView::PriceOrig   : return cmp( m_item-> origPrice ( ),                (*ci-> m_item). origPrice ( ));
		case CItemView::PriceDiff   : return cmp(( m_item-> price ( ) - m_item-> origPrice ( )), ( (*ci-> m_item). price ( ) - (*ci-> m_item). origPrice ( )));
		case CItemView::QuantityOrig: return m_item-> origQuantity ( )                 - (*ci-> m_item). origQuantity ( );
		case CItemView::QuantityDiff: return ( m_item-> quantity ( ) - m_item-> origQuantity ( )) - ( (*ci-> m_item). quantity ( ) - (*ci-> m_item). origQuantity ( ));
	}
	return 0;
}

//#undef cmp


void CItemViewItem::setup ( )
{
	widthChanged ( );

	int h;
	int m = listView ( )-> itemMargin ( );
	h = QMAX( 30, listView ( )-> fontMetrics ( ). height ( ) + 2 * m );
	h = QMAX( h, QApplication::globalStrut ( ). height ( ));

	setHeight (( h & 1 ) ? h + 1 : h );
}


void CItemViewItem::paintCell ( QPainter *p, const QColorGroup &cg, int col, int w, int align )
{
	int x = 0, y = 0;
	int h = height ( );
	int margin = listView ( )-> itemMargin ( );
	Q_UINT64 colmask = 1ULL << col;
	CItemView *iv = listView ( );
	int grayout_right_chars = 0;


	const QPixmap *pix = pixmap ( col );
	QString str = text ( col );

	QColor bg;
	QColor fg;
	int checkmark = 0;

	bg = backgroundColor ( );
	fg = cg. text ( );

	switch ( col ) {
		case CItemView::Description:
			if ( m_item-> item ( )-> inventoryUpdated ( ). isValid ( )) {
				QString invstr = CItemView::tr( "Inv" );
				str = str + " [" + invstr + "]";
				grayout_right_chars = invstr. length ( ) + 2;
			}
			break;

		case CItemView::ItemType:
			bg = CUtility::gradientColor ( bg, shadeColor ( m_item-> itemType ( )-> id ( )), 0.1f );
			break;

		case CItemView::Category:
			bg = CUtility::gradientColor ( bg, shadeColor ( m_item-> category ( )-> id ( )), 0.2f );
			break;

		case CItemView::Quantity:
			if ( m_item-> quantity ( ) <= 0 ) {
				bg = CUtility::gradientColor ( bg, m_item-> quantity ( ) == 0 ? Qt::yellow : Qt::red, 0.5f );
				fg = CUtility::contrastColor ( fg, -0.2f );
			}
			break;

		case CItemView::QuantityDiff:
			if ( m_item-> origQuantity ( ) < m_item-> quantity ( ))
				bg = CUtility::gradientColor ( bg, Qt::green, 0.3f );
			else if ( m_item-> origQuantity ( ) > m_item-> quantity ( ))
				bg = CUtility::gradientColor ( bg, Qt::red, 0.3f );
			break;

		case CItemView::PriceOrig:
		case CItemView::QuantityOrig:
			fg = CUtility::gradientColor ( bg, fg, 0.5f );
			break;

		case CItemView::PriceDiff:
			if ( m_item-> origPrice ( ) < m_item-> price ( ))
				bg = CUtility::gradientColor ( bg, Qt::green, 0.3f );
			else if ( m_item-> origPrice ( ) > m_item-> price ( ))
				bg = CUtility::gradientColor ( bg, Qt::red, 0.3f );
			break;

		case CItemView::Total:
			bg = CUtility::gradientColor ( bg, yellow, 0.1f );
			break;

		case CItemView::Condition:
			if ( m_item-> condition ( ) == BrickLink::Used )
				bg = CUtility::contrastColor ( bg, 0.3f );
			break;

		case CItemView::TierP1: 
		case CItemView::TierQ1: 
			bg = CUtility::contrastColor ( bg, 0.06f );
			break;

		case CItemView::TierP2:
		case CItemView::TierQ2: 
			bg = CUtility::contrastColor ( bg, 0.12f );
			break;

		case CItemView::TierP3:
		case CItemView::TierQ3: 
			bg = CUtility::contrastColor ( bg, 0.18f );
			break;

		case CItemView::Retain:
			checkmark = m_item-> retain ( ) ? 1 : -1;
			break;

		case CItemView::Stockroom:
			checkmark = m_item-> stockroom ( ) ? 1 : -1;
			break;
	}
	if ( isSelected ( )) {
		float f = ( col == iv-> currentColumn ( ) && this == iv-> currentItem ( )) ? 1.f : .7f;

		bg = CUtility::gradientColor ( bg, cg. highlight ( ), f );
		fg = cg. highlightedText ( );
	}
	
	p-> fillRect ( x, y, w, h, bg );
	p-> setPen ( cg. mid ( ));
	p-> drawLine ( w - 1, y, x + w - 1, y + h - 1 );
	w--;

	if (( m_item-> errors ( ) & iv-> d-> m_doc-> errorMask ( ) & ( 1 << col ))) {
		p-> setPen ( CUtility::gradientColor ( backgroundColor ( ), Qt::red, 0.75f ));
		p-> drawRect ( x, y, w, h );
		p-> setPen ( CUtility::gradientColor ( backgroundColor ( ), Qt::red, 0.50f ));
		p-> drawRect ( x + 1, y + 1, w - 2, h - 2 );
	}

	p-> setPen ( fg );

	x++;	// extra spacing
	w -=2;

	if ( checkmark != 0 ) {
		const QStyle &style = iv-> style ( );

		int iw = style. pixelMetric ( QStyle::PM_IndicatorWidth );
		int ih = style. pixelMetric ( QStyle::PM_IndicatorHeight );

		if ( iw > w )
			iw = w;
		if ( ih > h )
			ih = h;

		x = ( w - iw ) / 2;
		y = ( h - ih ) / 2;

		style. drawPrimitive ( QStyle::PE_Indicator, p, QRect( x, y, iw, ih ), cg, ( checkmark > 0 ? QStyle::Style_On : QStyle::Style_Off ) | QStyle::Style_Enabled );
	}
	else if ( pix && !pix-> isNull ( )) {
		// clip the pixmap here .. this is cheaper than a cliprect

		int rw = w - 2 * margin;
		int rh = h; // - 2 * margin;

		int sw, sh;

		if ( pix-> height ( ) <= rh ) {
			sw = QMIN( rw, pix-> width ( ));
			sh = QMIN( rh, pix-> height ( ));
		}
		else {
			sw = pix-> width ( ) * rh / pix-> height ( );
			sh = rh;
		}

		int px = x + margin;
		int py = y + /*margin +*/ ( rh - sh ) / 2;

		if ( align == AlignCenter ) 
			px += ( rw - sw ) / 2; // center if there is enough room

		if ( pix-> height ( ) <= rh )
			p-> drawPixmap ( px, py, *pix, 0, 0, sw, sh );
		else
			p-> drawPixmap ( QRect ( px, py, sw, sh ), *pix );

		w -= ( margin + sw );
		x += ( margin + sw );
	}
	if ( !str. isEmpty ( )) {
		int rw = w - 2 * margin;

		if ( !( align & AlignVertical_Mask ))
			align |= AlignVCenter;

		const QFontMetrics &fm = p-> fontMetrics ( );

		if ( fm. width ( str ) > rw ) {
			QString estr = CUtility::ellipsisText ( str, fm, w, align );

			if ( grayout_right_chars ) {
				int killed_chars = ( str. length ( ) - ( estr. length ( ) - 3 ));

				grayout_right_chars -= killed_chars;

				if ( grayout_right_chars > 0 )
					grayout_right_chars += 3;
				else
					grayout_right_chars = 0;
			}

			str = estr;

			m_truncated |= colmask;
		}
		else {
			m_truncated &= ~colmask;
		}

		if ( grayout_right_chars && (( align & AlignHorizontal_Mask ) == AlignLeft )) {
			QString lstr = str. left ( str. length ( ) - grayout_right_chars );
			QString rstr = str. right ( grayout_right_chars );

			int dx = fm. width ( lstr );
			p-> drawText ( x + margin, y, rw, h, align, lstr );
		
			p-> setPen ( CUtility::gradientColor ( bg, fg, 0.5f ));
			p-> drawText ( x + margin + dx, y, rw - dx, h, align, rstr );
		}
		else
			p-> drawText ( x + margin, y, rw, h, align, str );
	}
}

int CItemViewItem::rtti ( ) const
{
	return RTTI;
}

CItemView *CItemViewItem::listView ( ) const
{
	return static_cast <CItemView *> ( CListViewItem::listView ( ));
}


QRect CItemViewItem::globalRect ( int col ) const
{
	QListView *lv = listView ( );

	QPoint p ( lv-> header ( )-> sectionPos ( col ), lv-> itemPos ( this ));
	p = lv-> viewport ( )-> mapToGlobal ( lv-> contentsToViewport ( p ));

	QSize s ( lv-> columnWidth ( col ), height ( ));

	return QRect ( p, s );
}


void CItemViewItem::doubleClicked ( const QPoint &/*p*/, int col )
{
	CItemView *lv = listView ( );

	lv-> terminateEdit ( true );

	switch ( col ) {
		case CItemView::LotId       : break;
		case CItemView::PartNo      : lv-> editWithLineEdit ( this, col, m_item-> item ( )-> id ( )); break;
		case CItemView::Comments    : lv-> editWithLineEdit ( this, col, m_item-> comments ( )); break;
		case CItemView::Remarks     : lv-> editWithLineEdit ( this, col, m_item-> remarks ( )); break;
		case CItemView::Reserved    : lv-> editWithLineEdit ( this, col, m_item-> reserved ( )); break;
		case CItemView::Sale        : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> sale ( )),           "", new QIntValidator ( -1000, 99, 0 )); break;
		case CItemView::Quantity    : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> quantity ( )),       "", new QIntValidator ( -99999, 99999, 0 )); break;
		case CItemView::QuantityDiff: lv-> editWithLineEdit ( this, col, QString::number ( m_item-> quantity ( ) - m_item-> origQuantity ( )), "", new QIntValidator ( -99999, 99999, 0 )); break;
		case CItemView::Bulk        : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> bulkQuantity ( )),   "", new QIntValidator ( 1, 99999, 0 )); break;
		case CItemView::TierQ1      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 0 )), "", new QIntValidator ( 0, 99999, 0 )); break;
		case CItemView::TierQ2      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 1 )), "", new QIntValidator ( 0, 99999, 0 )); break;
		case CItemView::TierQ3      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 2 )), "", new QIntValidator ( 0, 99999, 0 )); break;
		case CItemView::Price       : lv-> editWithLineEdit ( this, col, m_item-> price ( ). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 )); break;
		case CItemView::PriceDiff   : lv-> editWithLineEdit ( this, col, ( m_item-> price ( ) - m_item-> origPrice ( )). toLocalizedString ( true ), "", new CMoneyValidator ( -10000, 10000, 3, 0 )); break;
		case CItemView::Total       : break;
		case CItemView::TierP1      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 0 ) != 0 ? m_item-> tierPrice ( 0 ) : m_item-> price ( )      ). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 )); break;
		case CItemView::TierP2      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 1 ) != 0 ? m_item-> tierPrice ( 1 ) : m_item-> tierPrice ( 0 )). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 )); break;
		case CItemView::TierP3      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 2 ) != 0 ? m_item-> tierPrice ( 2 ) : m_item-> tierPrice ( 1 )). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 )); break;
		case CItemView::Weight      : lv-> editWithLineEdit ( this, col, CUtility::weightToString ( m_item-> weight ( ), ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), false, false ), "", new QDoubleValidator ( 0., 100000., 4, 0 )); break;

		case CItemView::Retain      : m_item-> setRetain ( !m_item-> retain ( ));
		                              repaint ( );
		                              emit lv-> itemChanged ( this, false );
		                              break;

		case CItemView::Stockroom   : m_item-> setStockroom ( !m_item-> stockroom ( ));
		                              repaint ( );
		                              emit lv-> itemChanged ( this, false );
		                              break;

		case CItemView::Condition   : m_item-> setCondition ( m_item-> condition ( ) == BrickLink::New ? BrickLink::Used : BrickLink::New );
		                              repaint ( );
		                              emit lv-> itemChanged ( this, false );
		                              break;

		case CItemView::Status      : {
			BrickLink::InvItem::Status st = m_item-> status ( );

			switch ( st ) {
				case BrickLink::InvItem::Include: st = BrickLink::InvItem::Exclude; break;
				case BrickLink::InvItem::Exclude: st = BrickLink::InvItem::Extra; break;
				case BrickLink::InvItem::Extra  :
				default                         : st = BrickLink::InvItem::Include; break;
			}
			m_item-> setStatus ( st );

			repaint ( );

			emit lv-> itemChanged ( this, false );
			break;
		}

		case CItemView::Picture     :
		case CItemView::Description : {
			CSelectItemDialog d ( false, lv);
			d. setCaption ( CItemView::tr( "Modify Item" ));
			d. setItem ( m_item-> item ( ));

			if ( d. exec ( globalRect ( col )) == QDialog::Accepted ) {
				m_item-> setItem ( d. item ( ));

				widthChanged ( );
				repaint ( );

				emit lv-> itemChanged ( this, true );
			}
			break;
		}

		case CItemView::Color       : {
			CSelectColorDialog d ( lv);
			d. setCaption ( CItemView::tr( "Modify Color" ));
			d. setColor ( m_item-> color ( ));

			if ( d. exec ( globalRect ( col )) == QDialog::Accepted ) {
				m_item-> setColor ( d. color ( ));

				widthChanged ( );
				repaint ( );

				emit lv-> itemChanged ( this, true );
			}
			break;
		}
	}
}


void CItemViewItem::editDone ( int col, const QString &result, bool valid )
{
	QString not_valid = CItemView::tr( "The value you entered was not valid and will not be changed." );

	if ( !valid ) {
		CMessageBox::information ( listView ( ), not_valid );
		return;
	}

	bool no_repaint = false;
	bool grave_change = false;

	switch ( col ) {
		case CItemView::PartNo      : {
			const BrickLink::Item *newitem = BrickLink::inst ( )-> item ( m_item-> itemType ( )-> id ( ), result. latin1 ( ));
			if ( newitem ) {
				m_item-> setItem ( newitem );
				grave_change = true;
			}
			else {
				CMessageBox::information ( listView ( ), not_valid );
				return;
			}
			break;
		}

		case CItemView::Comments    : m_item-> setComments ( result ); break;
		case CItemView::Remarks     : m_item-> setRemarks ( result ); break;
		case CItemView::Reserved    : m_item-> setReserved ( result ); break;

		case CItemView::Sale        : m_item-> setSale ( result. toInt ( )); break;

		case CItemView::Bulk        : m_item-> setBulkQuantity ( result. toInt ( )); break;
		case CItemView::TierQ1      : m_item-> setTierQuantity ( 0, result. toInt ( )); break;
		case CItemView::TierQ2      : m_item-> setTierQuantity ( 1, result. toInt ( )); break;
		case CItemView::TierQ3      : m_item-> setTierQuantity ( 2, result. toInt ( )); break;

		case CItemView::TierP1      : m_item-> setTierPrice ( 0, money_t::fromLocalizedString ( result )); break;
		case CItemView::TierP2      : m_item-> setTierPrice ( 1, money_t::fromLocalizedString ( result )); break;
		case CItemView::TierP3      : m_item-> setTierPrice ( 2, money_t::fromLocalizedString ( result )); break;

		case CItemView::Weight      : m_item-> setWeight ( CUtility::stringToWeight ( result, ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ))); break;

		case CItemView::Quantity    : m_item-> setQuantity ( result. toInt ( )); break;
		case CItemView::QuantityDiff: m_item-> setQuantity ( m_item-> origQuantity ( ) + result. toInt ( )); break;

		case CItemView::Price       : m_item-> setPrice ( money_t::fromLocalizedString ( result )); break;
		case CItemView::PriceDiff   : m_item-> setPrice ( m_item-> origPrice ( ) + money_t::fromLocalizedString ( result )); break;

		default                     : no_repaint = true; break;
	}

//	if ( checkForErrors ( ))
//		no_repaint = false;

	widthChanged ( col );

	if ( !no_repaint )
		repaint ( );

	emit listView ( )-> itemChanged ( this, grave_change );
}

