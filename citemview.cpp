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

	QString            m_empty_value;


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

	for ( int i = 0; i < CDocument::FieldCount; i++ ) {
		int align = AlignLeft;
		QString t;
		int width = 0;
		int hidden = 0;

		switch ( i ) {
			case CDocument::Status      : align = AlignCenter; width = -16; break;
			case CDocument::Picture     : align = AlignCenter; width = -40; break;
			case CDocument::PartNo      : width = 10; break;
			case CDocument::Description : width = 28; break;
			case CDocument::Comments    : width = 8; break;
			case CDocument::Remarks     : width = 8; break;
			case CDocument::QuantityOrig: width = 5; hidden = 2; break;
			case CDocument::QuantityDiff: width = 5; hidden = 2; break;
			case CDocument::Quantity    : width = 5; break;
			case CDocument::Bulk        : width = 5; break;
			case CDocument::PriceOrig   : width = 8; align = AlignRight; hidden = 2; break;
			case CDocument::PriceDiff   : width = 8; align = AlignRight; hidden = 2; break;
			case CDocument::Price       : width = 8; align = AlignRight; break;
			case CDocument::Total       : width = 8; align = AlignRight; break;
			case CDocument::Sale        : width = 5; align = AlignRight; break;
			case CDocument::Condition   : width = 5; align = AlignHCenter; break;
			case CDocument::Color       : width = 15; break;
			case CDocument::Category    : width = 12; break;
			case CDocument::ItemType    : width = 12; break;
			case CDocument::TierQ1      : width = 5; break;
			case CDocument::TierP1      : width = 8; align = AlignRight; break;
			case CDocument::TierQ2      : width = 5; break;
			case CDocument::TierP2      : width = 8; align = AlignRight; break;
			case CDocument::TierQ3      : width = 5; break;
			case CDocument::TierP3      : width = 8; align = AlignRight; break;
			case CDocument::LotId       : align = AlignLeft; width = 8; hidden = 1; break;
			case CDocument::Retain      : align = AlignCenter; width = 8; hidden = 1; break;
			case CDocument::Stockroom   : align = AlignCenter; width = 8; hidden = 1; break;
			case CDocument::Reserved    : width = 8; hidden = 1; break;
			case CDocument::Weight      : align = AlignRight; width = 6; hidden = 1; break;
			case CDocument::YearReleased: width = 5; hidden = 1; break;
		}
		int cid = addColumn ( t );
		setColumnAlignment ( cid, align );
		setColumnWidthMode ( cid, QListView::Manual );
		setColumnWidth ( cid, 2 * itemMargin ( ) + 3 + ( width <= 0 ? -width : width * fontMetrics ( ). width ( "0" )));

		if ( hidden == 1 )
			setColumnVisible ( cid, false );
		else if ( hidden == 2 )
			setColumnAvailable ( cid, false );
	}

	loadDefaultLayout ( );

	connect ( this, SIGNAL( doubleClicked ( QListViewItem *, const QPoint &, int )), this, SLOT( listItemDoubleClicked ( QListViewItem *, const QPoint &, int )));

	connect ( this, SIGNAL( contentsMoving ( int, int )), this, SLOT( cancelEdit ( )));
	connect ( this, SIGNAL( horizontalSliderPressed ( )), this, SLOT( cancelEdit ( )));
	connect ( this, SIGNAL( verticalSliderPressed ( )),   this, SLOT( cancelEdit ( )));

	languageChange ( );
}

void CItemView::languageChange ( )
{
	for ( int i = 0; i < CDocument::FieldCount; i++ ) {
		QString t;

		switch ( i ) {
			case CDocument::Status      : t = tr( "Status" ); break;
			case CDocument::Picture     : t = tr( "Image" ); break;
			case CDocument::PartNo      : t = tr( "Part #" ); break;
			case CDocument::Description : t = tr( "Description" ); break;
			case CDocument::Comments    : t = tr( "Comments" ); break;
			case CDocument::Remarks     : t = tr( "Remarks" ); break;
			case CDocument::QuantityOrig: t = tr( "Qty. Orig" ); break;
			case CDocument::QuantityDiff: t = tr( "Qty. Diff" ); break;
			case CDocument::Quantity    : t = tr( "Qty." ); break;
			case CDocument::Bulk        : t = tr( "Bulk" ); break;
			case CDocument::PriceOrig   : t = tr( "Pr. Orig" ); break;
			case CDocument::PriceDiff   : t = tr( "Pr. Diff" ); break;
			case CDocument::Price       : t = tr( "Price" ); break;
			case CDocument::Total       : t = tr( "Total" ); break;
			case CDocument::Sale        : t = tr( "Sale" ); break;
			case CDocument::Condition   : t = tr( "Cond." ); break;
			case CDocument::Color       : t = tr( "Color" ); break;
			case CDocument::Category    : t = tr( "Category" ); break;
			case CDocument::ItemType    : t = tr( "Item Type" ); break;
			case CDocument::TierQ1      : t = tr( "Tier Q1" ); break;
			case CDocument::TierP1      : t = tr( "Tier P1" ); break;
			case CDocument::TierQ2      : t = tr( "Tier Q2" ); break;
			case CDocument::TierP2      : t = tr( "Tier P2" ); break;
			case CDocument::TierQ3      : t = tr( "Tier Q3" ); break;
			case CDocument::TierP3      : t = tr( "Tier P3" ); break;
			case CDocument::LotId       : t = tr( "Lot Id" ); break;
			case CDocument::Retain      : t = tr( "Retain" ); break;
			case CDocument::Stockroom   : t = tr( "Stockroom" ); break;
			case CDocument::Reserved    : t = tr( "Reserved" ); break;
			case CDocument::Weight      : t = tr( "Weight" ); break;
			case CDocument::YearReleased: t = tr( "Year" ); break;
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

CDocument *CItemView::document ( ) const
{
	return d-> m_doc;
}

void CItemView::loadDefaultLayout ( )
{
	QMap <QString, QString> map;
	QStringList sl = CConfig::inst ( )-> entryList ( "/ItemView/List" );

	for ( QStringList::const_iterator it = sl. begin ( ); it != sl. end ( ); ++it ) {
		QString val = CConfig::inst ( )-> readEntry ( "/ItemView/List/" + *it );

		if ( val. contains ( "^e" ))
			map [*it] = CConfig::inst ( )-> readListEntry ( "/ItemView/List/" + *it ). join ( "," );
		else
			map [*it] = val;
	}
	loadSettings ( map );
}

void CItemView::saveDefaultLayout ( )
{
	QMap<QString, QString> map = saveSettings ( );

	for ( QMap<QString, QString>::const_iterator it = map. begin ( ); it != map. end ( ); ++it )
		CConfig::inst ( )-> writeEntry ( "/ItemView/List/" + it. key ( ), it. data ( ));
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

	setColumnAvailable ( CDocument::Bulk, !b );
	setColumnAvailable ( CDocument::Sale, !b );
	setColumnAvailable ( CDocument::TierQ1, !b );
	setColumnAvailable ( CDocument::TierQ2, !b );
	setColumnAvailable ( CDocument::TierQ3, !b );
	setColumnAvailable ( CDocument::TierP1, !b );
	setColumnAvailable ( CDocument::TierP2, !b );
	setColumnAvailable ( CDocument::TierP3, !b );
	setColumnAvailable ( CDocument::Reserved, !b );
	setColumnAvailable ( CDocument::Stockroom, !b );
	setColumnAvailable ( CDocument::Retain, !b );
	setColumnAvailable ( CDocument::LotId, !b );
	setColumnAvailable ( CDocument::Comments, !b );
}

bool CItemView::isDifferenceMode ( ) const
{
	return d-> m_diff_mode;
}

void CItemView::setDifferenceMode ( bool b )
{
	d-> m_diff_mode = b;

	setColumnAvailable ( CDocument::PriceOrig, b );
	setColumnAvailable ( CDocument::PriceDiff, b );
	setColumnAvailable ( CDocument::QuantityOrig, b );
	setColumnAvailable ( CDocument::QuantityDiff, b );

	if ( b ) {
		if ( isColumnVisible ( CDocument::Quantity )) {
			showColumn ( CDocument::QuantityDiff );
			showColumn ( CDocument::QuantityOrig );

			header ( )-> moveSection ( CDocument::QuantityDiff, header ( )-> mapToIndex ( CDocument::Quantity ));
			header ( )-> moveSection ( CDocument::QuantityOrig, header ( )-> mapToIndex ( CDocument::QuantityDiff ));
		}

		if ( isColumnVisible ( CDocument::Price )) {
			showColumn ( CDocument::PriceDiff );
			showColumn ( CDocument::PriceOrig );
		
			header ( )-> moveSection ( CDocument::PriceDiff, header ( )-> mapToIndex ( CDocument::Price ));
			header ( )-> moveSection ( CDocument::PriceOrig, header ( )-> mapToIndex ( CDocument::PriceDiff ));
		}
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

		if ( !b && ( field >= 0 ) && ( field < CDocument::FieldCount )) { // normal field
			b = ( regexp. search ( ivi-> text ( field )) >= 0 );
		}
		else if ( !b && ( field < 0 )) { // special field
			int fieldindex = -field - 1;

			static int matchfields [FilterCountSpecial][CDocument::FieldCount + 1] = {
				{ /*All       */ CDocument::PartNo, CDocument::Description, CDocument::Comments, CDocument::Remarks, CDocument::Quantity, CDocument::Price, CDocument::Color, CDocument::Category, CDocument::ItemType, CDocument::TierQ1, CDocument::TierP1, CDocument::TierQ2, CDocument::TierP2, CDocument::TierQ3, CDocument::TierP3, CDocument::Weight, CDocument::YearReleased, -1 },
				{ /*Prices    */ CDocument::Price, CDocument::TierP1, CDocument::TierP2, CDocument::TierP3, CDocument::Total, -1 },
				{ /*Texts     */ CDocument::Description, CDocument::Comments, CDocument::Remarks, -1 },
				{ /*Quantities*/ CDocument::Quantity, CDocument::TierQ1, CDocument::TierQ2, CDocument::TierQ3, -1 },
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

void CItemView::editWithLineEdit ( CItemViewItem *ivi, int col, const QString &text, const QString &mask, QValidator *valid, const QString &empty_value )
{
	if ( !ivi || ( ivi-> listView ( ) != this ) || ( col < 0 ) || ( col >= columns ( )))
		return;

	d-> m_active_validator = valid;

	d-> m_lineedit-> setAlignment ( columnAlignment ( col ));
	d-> m_lineedit-> setValidator ( valid );
	d-> m_lineedit-> setInputMask ( mask );
	d-> m_lineedit-> setText ( text );
	d-> m_lineedit-> selectAll ( );

	d-> m_empty_value = empty_value;

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
				if ( d-> m_lineedit-> text ( ). isEmpty ( ) && !d-> m_empty_value. isEmpty ( ))
					d-> m_lineedit-> setText ( d-> m_empty_value );

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
		case CDocument::Status      : w = pixmap ( column )-> width ( ); break;
		case CDocument::Picture     : w = 40; break;
		case CDocument::Retain      :
		case CDocument::Stockroom   : w = lv-> style ( ). pixelMetric ( QStyle::PM_IndicatorWidth ); break;
		default                     : w = CListViewItem::width ( fm, lv, column ); break;
	}
	return w + 3;
}

QString CItemViewItem::toolTip ( int column ) const
{
	QString str;

	switch ( column ) {
		case CDocument::Status      : str = CItemView::statusLabel ( m_item-> status ( )); break;
		case CDocument::Picture     : str = text ( CDocument::PartNo ) + " " + text ( CDocument::Description ); break;
		case CDocument::Condition   : str = CItemView::conditionLabel ( m_item-> condition ( )); break;
		case CDocument::Category    : {
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
		case CDocument::Status      :
		case CDocument::Picture     :
		case CDocument::Retain      :
		case CDocument::Stockroom   : break;
		case CDocument::LotId       : str = ( m_item-> lotId ( ) == 0 ? dash : QString::number ( m_item-> lotId ( ))); break;
		case CDocument::PartNo      : str = m_item-> item ( )-> id ( ); break;
		case CDocument::Description : str = m_item-> item ( )-> name ( ); break;
		case CDocument::Comments    : str = m_item-> comments ( ); break;
		case CDocument::Remarks     : str = m_item-> remarks ( ); break;
		case CDocument::Quantity    : str = QString::number ( m_item-> quantity ( )); break;
		case CDocument::Bulk        : str = ( m_item-> bulkQuantity ( ) == 1 ? dash : QString::number ( m_item-> bulkQuantity ( ))); break;
		case CDocument::Price       : str = m_item-> price ( ). toLocalizedString ( ); break;
		case CDocument::Total       : str = m_item-> total ( ). toLocalizedString ( ); break;
		case CDocument::Sale        : str = ( m_item-> sale ( ) == 0 ? dash : QString::number ( m_item-> sale ( )) + "%" ); break;
		case CDocument::Condition   : str = ( m_item-> condition ( ) == BrickLink::New ? "N" : "U" ); break;
		case CDocument::Color       : str = m_item-> color ( )-> name ( ); break;
		case CDocument::Category    : str = m_item-> category ( )-> name ( ); break;
		case CDocument::ItemType    : str = m_item-> itemType ( )-> name ( ); break;
		case CDocument::TierQ1      : str = ( m_item-> tierQuantity ( 0 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 0 ))); break;
		case CDocument::TierQ2      : str = ( m_item-> tierQuantity ( 1 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 1 ))); break;
		case CDocument::TierQ3      : str = ( m_item-> tierQuantity ( 2 ) == 0 ? dash : QString::number ( m_item-> tierQuantity ( 2 ))); break;
		case CDocument::TierP1      : str = m_item-> tierPrice ( 0 ). toLocalizedString ( ); break;
		case CDocument::TierP2      : str = m_item-> tierPrice ( 1 ). toLocalizedString ( ); break;
		case CDocument::TierP3      : str = m_item-> tierPrice ( 2 ). toLocalizedString ( ); break;
		case CDocument::Reserved    : str = m_item-> reserved ( ); break;
		case CDocument::Weight      : str = ( m_item-> weight ( ) == 0 ? dash : CUtility::weightToString ( m_item-> weight ( ), ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), true, true )); break;
		case CDocument::YearReleased: str = ( m_item-> item ( )-> yearReleased ( ) == 0 ? dash : QString::number ( m_item-> item ( )-> yearReleased ( ))); break;

		case CDocument::PriceOrig   : str = m_item-> origPrice ( ). toLocalizedString ( ); break;
		case CDocument::PriceDiff   : str = ( m_item-> price ( ) - m_item-> origPrice ( )). toLocalizedString ( ); break;
		case CDocument::QuantityOrig: str = QString::number ( m_item-> origQuantity ( )); break;
		case CDocument::QuantityDiff: str = QString::number ( m_item-> quantity ( ) - m_item-> origQuantity ( )); break;
	}
	return str;
}

const QPixmap *CItemViewItem::pixmap ( int column ) const
{
	if ( column == CDocument::Status ) {
		return &listView ( )-> d-> m_pixmap_status [QMAX( 0, QMIN( m_item-> status ( ), BrickLink::InvItem::Unknown ))];
	}
	else if ( column == CDocument::Picture ) {
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
	else if ( column == CDocument::Color ) {
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
		case CDocument::Status      : return cmp( m_item-> status ( ), (*ci-> m_item). status ( ));
		case CDocument::Picture     :
		case CDocument::PartNo      : return qstrcmp ( m_item-> item ( )-> id ( ),       (*ci-> m_item). item ( )-> id ( ));
		case CDocument::LotId       : return m_item-> lotId ( )                        - (*ci-> m_item). lotId ( );
		case CDocument::Description : return qstrcmp ( m_item-> item ( )-> name ( ),     (*ci-> m_item). item ( )-> name ( ));
		case CDocument::Comments    : return m_item-> comments ( ).            compare ( (*ci-> m_item). comments ( ));
		case CDocument::Remarks     : return m_item-> remarks ( ).             compare ( (*ci-> m_item). remarks ( ));
		case CDocument::Quantity    : return m_item-> quantity ( )                     - (*ci-> m_item). quantity ( );
		case CDocument::Bulk        : return m_item-> bulkQuantity ( )                 - (*ci-> m_item). bulkQuantity ( );
		case CDocument::Price       : return cmp( m_item-> price ( ),                    (*ci-> m_item). price ( ));
		case CDocument::Total       : return cmp( m_item-> total ( ),                    (*ci-> m_item). total ( ));
		case CDocument::Sale        : return m_item-> sale ( )                         - (*ci-> m_item). sale ( );
		case CDocument::Condition   : return cmp( m_item-> condition ( ),                (*ci-> m_item). condition ( ));
		case CDocument::Color       : return qstrcmp ( m_item-> color ( )-> name ( ),    (*ci-> m_item). color ( )-> name ( ));
		case CDocument::Category    : return qstrcmp ( m_item-> category ( )-> name ( ), (*ci-> m_item). category ( )-> name ( ));
		case CDocument::ItemType    : return qstrcmp ( m_item-> itemType ( )-> name ( ), (*ci-> m_item). itemType ( )-> name ( ));
		case CDocument::TierQ1      : return m_item-> tierQuantity ( 0 )               - (*ci-> m_item). tierQuantity ( 0 );
		case CDocument::TierQ2      : return m_item-> tierQuantity ( 1 )               - (*ci-> m_item). tierQuantity ( 1 );
		case CDocument::TierQ3      : return m_item-> tierQuantity ( 2 )               - (*ci-> m_item). tierQuantity ( 2 );
		case CDocument::TierP1      : return cmp( m_item-> tierPrice ( 0 ),              (*ci-> m_item). tierPrice ( 0 ));
		case CDocument::TierP2      : return cmp( m_item-> tierPrice ( 1 ),              (*ci-> m_item). tierPrice ( 1 ));
		case CDocument::TierP3      : return cmp( m_item-> tierPrice ( 2 ),              (*ci-> m_item). tierPrice ( 2 ));
		case CDocument::Retain      : return cmp( m_item-> retain ( ) ? 1 : 0,           (*ci-> m_item). retain ( ) ? 1 : 0 );
		case CDocument::Stockroom   : return cmp( m_item-> stockroom ( ) ? 1 : 0,        (*ci-> m_item). stockroom ( ) ? 1 : 0 );
		case CDocument::Reserved    : return m_item-> reserved ( ).            compare ( (*ci-> m_item). reserved ( ));
		case CDocument::Weight      : return cmp( m_item-> weight ( ),                   (*ci-> m_item). weight ( ));
		case CDocument::YearReleased: return cmp( m_item-> item ( )-> yearReleased ( ),  (*ci-> m_item). item ( )-> yearReleased ( ));

		case CDocument::PriceOrig   : return cmp( m_item-> origPrice ( ),                (*ci-> m_item). origPrice ( ));
		case CDocument::PriceDiff   : return cmp(( m_item-> price ( ) - m_item-> origPrice ( )), ( (*ci-> m_item). price ( ) - (*ci-> m_item). origPrice ( )));
		case CDocument::QuantityOrig: return m_item-> origQuantity ( )                 - (*ci-> m_item). origQuantity ( );
		case CDocument::QuantityDiff: return ( m_item-> quantity ( ) - m_item-> origQuantity ( )) - ( (*ci-> m_item). quantity ( ) - (*ci-> m_item). origQuantity ( ));
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
		case CDocument::Description:
			if ( m_item-> item ( )-> hasInventory ( )) {
				QString invstr = CItemView::tr( "Inv" );
				str = str + " [" + invstr + "]";
				grayout_right_chars = invstr. length ( ) + 2;
			}
			break;

		case CDocument::ItemType:
			bg = CUtility::gradientColor ( bg, shadeColor ( m_item-> itemType ( )-> id ( )), 0.1f );
			break;

		case CDocument::Category:
			bg = CUtility::gradientColor ( bg, shadeColor ( m_item-> category ( )-> id ( )), 0.2f );
			break;

		case CDocument::Quantity:
			if ( m_item-> quantity ( ) <= 0 ) {
				bg = CUtility::gradientColor ( bg, m_item-> quantity ( ) == 0 ? Qt::yellow : Qt::red, 0.5f );
				fg = CUtility::contrastColor ( fg, -0.2f );
			}
			break;

		case CDocument::QuantityDiff:
			if ( m_item-> origQuantity ( ) < m_item-> quantity ( ))
				bg = CUtility::gradientColor ( bg, Qt::green, 0.3f );
			else if ( m_item-> origQuantity ( ) > m_item-> quantity ( ))
				bg = CUtility::gradientColor ( bg, Qt::red, 0.3f );
			break;

		case CDocument::PriceOrig:
		case CDocument::QuantityOrig:
			fg = CUtility::gradientColor ( bg, fg, 0.5f );
			break;

		case CDocument::PriceDiff:
			if ( m_item-> origPrice ( ) < m_item-> price ( ))
				bg = CUtility::gradientColor ( bg, Qt::green, 0.3f );
			else if ( m_item-> origPrice ( ) > m_item-> price ( ))
				bg = CUtility::gradientColor ( bg, Qt::red, 0.3f );
			break;

		case CDocument::Total:
			bg = CUtility::gradientColor ( bg, yellow, 0.1f );
			break;

		case CDocument::Condition:
			if ( m_item-> condition ( ) == BrickLink::Used )
				bg = CUtility::contrastColor ( bg, 0.3f );
			break;

		case CDocument::TierP1: 
		case CDocument::TierQ1: 
			bg = CUtility::contrastColor ( bg, 0.06f );
			break;

		case CDocument::TierP2:
		case CDocument::TierQ2: 
			bg = CUtility::contrastColor ( bg, 0.12f );
			break;

		case CDocument::TierP3:
		case CDocument::TierQ3: 
			bg = CUtility::contrastColor ( bg, 0.18f );
			break;

		case CDocument::Retain:
			checkmark = m_item-> retain ( ) ? 1 : -1;
			break;

		case CDocument::Stockroom:
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
		case CDocument::LotId       : break;
		case CDocument::PartNo      : lv-> editWithLineEdit ( this, col, m_item-> item ( )-> id ( )); break;
		case CDocument::Comments    : lv-> editWithLineEdit ( this, col, m_item-> comments ( )); break;
		case CDocument::Remarks     : lv-> editWithLineEdit ( this, col, m_item-> remarks ( )); break;
		case CDocument::Reserved    : lv-> editWithLineEdit ( this, col, m_item-> reserved ( )); break;
		case CDocument::Sale        : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> sale ( )),           "", new QIntValidator ( -1000, 99, 0 ), "0" ); break;
		case CDocument::Quantity    : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> quantity ( )),       "", new QIntValidator ( -99999, 99999, 0 ), "0" ); break;
		case CDocument::QuantityDiff: lv-> editWithLineEdit ( this, col, QString::number ( m_item-> quantity ( ) - m_item-> origQuantity ( )), "", new QIntValidator ( -99999, 99999, 0 ), "0" ); break;
		case CDocument::Bulk        : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> bulkQuantity ( )),   "", new QIntValidator ( 1, 99999, 0 ), "1" ); break;
		case CDocument::TierQ1      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 0 )), "", new QIntValidator ( 0, 99999, 0 ), "0" ); break;
		case CDocument::TierQ2      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 1 )), "", new QIntValidator ( 0, 99999, 0 ), "0" ); break;
		case CDocument::TierQ3      : lv-> editWithLineEdit ( this, col, QString::number ( m_item-> tierQuantity ( 2 )), "", new QIntValidator ( 0, 99999, 0 ), "0" ); break;
		case CDocument::Price       : lv-> editWithLineEdit ( this, col, m_item-> price ( ). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 ), "0" ); break;
		case CDocument::PriceDiff   : lv-> editWithLineEdit ( this, col, ( m_item-> price ( ) - m_item-> origPrice ( )). toLocalizedString ( true ), "", new CMoneyValidator ( -10000, 10000, 3, 0 ), "0" ); break;
		case CDocument::Total       : break;
		case CDocument::TierP1      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 0 ) != 0 ? m_item-> tierPrice ( 0 ) : m_item-> price ( )      ). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 ), "0" ); break;
		case CDocument::TierP2      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 1 ) != 0 ? m_item-> tierPrice ( 1 ) : m_item-> tierPrice ( 0 )). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 ), "0" ); break;
		case CDocument::TierP3      : lv-> editWithLineEdit ( this, col, ( m_item-> tierPrice ( 2 ) != 0 ? m_item-> tierPrice ( 2 ) : m_item-> tierPrice ( 1 )). toLocalizedString ( true ), "", new CMoneyValidator ( 0, 10000, 3, 0 ), "0" ); break;
		case CDocument::Weight      : lv-> editWithLineEdit ( this, col, CUtility::weightToString ( m_item-> weight ( ), ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ), false, false ), "", new QDoubleValidator ( 0., 100000., 4, 0 ), "0" ); break;

		case CDocument::Retain      : {
			CDocument::Item item = *m_item;
			item. setRetain ( !m_item-> retain ( ));
			lv-> document ( )-> changeItem ( m_item, item );
			break;
		}
		case CDocument::Stockroom   : {
			CDocument::Item item = *m_item;
			item. setStockroom ( !m_item-> stockroom ( ));
			lv-> document ( )-> changeItem ( m_item, item );
			break;
		}
		case CDocument::Condition   : {
			CDocument::Item item = *m_item;
			item. setCondition ( m_item-> condition ( ) == BrickLink::New ? BrickLink::Used : BrickLink::New );
			lv-> document ( )-> changeItem ( m_item, item );
			break;
		}
		case CDocument::Status      : {
			BrickLink::InvItem::Status st = m_item-> status ( );

			switch ( st ) {
				case BrickLink::InvItem::Include: st = BrickLink::InvItem::Exclude; break;
				case BrickLink::InvItem::Exclude: st = BrickLink::InvItem::Extra; break;
				case BrickLink::InvItem::Extra  :
				default                         : st = BrickLink::InvItem::Include; break;
			}
			CDocument::Item item = *m_item;
			item. setStatus ( st );
			lv-> document ( )-> changeItem ( m_item, item );
			break;
		}

		case CDocument::Picture     :
		case CDocument::Description : {
			CSelectItemDialog d ( false, lv);
			d. setCaption ( CItemView::tr( "Modify Item" ));
			d. setItem ( m_item-> item ( ));

			if ( d. exec ( globalRect ( col )) == QDialog::Accepted ) {
				CDocument::Item item = *m_item;
				item. setItem ( d. item ( ));
				lv-> document ( )-> changeItem ( m_item, item );
			}
			break;
		}

		case CDocument::Color       : {
			CSelectColorDialog d ( lv);
			d. setCaption ( CItemView::tr( "Modify Color" ));
			d. setColor ( m_item-> color ( ));

			if ( d. exec ( globalRect ( col )) == QDialog::Accepted ) {
				CDocument::Item item = *m_item;
				item. setColor ( d. color ( ));
				lv-> document ( )-> changeItem ( m_item, item );
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

	CDocument::Item item = *m_item;

	switch ( col ) {
		case CDocument::PartNo      : {
			const BrickLink::Item *newitem = BrickLink::inst ( )-> item ( m_item-> itemType ( )-> id ( ), result. latin1 ( ));
			if ( newitem ) {
				item. setItem ( newitem );
			}
			else {
				CMessageBox::information ( listView ( ), not_valid );
				return;
			}
			break;
		}

		case CDocument::Comments    : item. setComments ( result ); break;
		case CDocument::Remarks     : item. setRemarks ( result ); break;
		case CDocument::Reserved    : item. setReserved ( result ); break;

		case CDocument::Sale        : item. setSale ( result. toInt ( )); break;

		case CDocument::Bulk        : item. setBulkQuantity ( result. toInt ( )); break;
		case CDocument::TierQ1      : item. setTierQuantity ( 0, result. toInt ( )); break;
		case CDocument::TierQ2      : item. setTierQuantity ( 1, result. toInt ( )); break;
		case CDocument::TierQ3      : item. setTierQuantity ( 2, result. toInt ( )); break;

		case CDocument::TierP1      : item. setTierPrice ( 0, money_t::fromLocalizedString ( result )); break;
		case CDocument::TierP2      : item. setTierPrice ( 1, money_t::fromLocalizedString ( result )); break;
		case CDocument::TierP3      : item. setTierPrice ( 2, money_t::fromLocalizedString ( result )); break;

		case CDocument::Weight      : item. setWeight ( CUtility::stringToWeight ( result, ( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightImperial ))); break;

		case CDocument::Quantity    : item. setQuantity ( result. toInt ( )); break;
		case CDocument::QuantityDiff: item. setQuantity ( m_item-> origQuantity ( ) + result. toInt ( )); break;

		case CDocument::Price       : item. setPrice ( money_t::fromLocalizedString ( result )); break;
		case CDocument::PriceDiff   : item. setPrice ( m_item-> origPrice ( ) + money_t::fromLocalizedString ( result )); break;

		default                     : return;
	}

	if ( item == *m_item )
		return;

	listView ( )-> document ( )-> changeItem ( m_item, item );
}

