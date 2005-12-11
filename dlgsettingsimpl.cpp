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
#include <qtoolbutton.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qvalidator.h>
#include <qbuttongroup.h>
#include <qtabwidget.h>

#include "cmessagebox.h"
#include "bricklink.h"
#include "cconfig.h"
#include "citemtypecombo.h"
#include "cmoney.h"
#include "cresource.h"
#include "cinfobar.h"

#include "dlgsettingsimpl.h"


namespace {

static int sec2day ( int s )
{
	return ( s + 60*60*24 - 1 ) / ( 60*60*24 );
}

static int day2sec ( int d )
{
	return d * ( 60*60*24 );
}

} // namespace

DlgSettingsImpl::DlgSettingsImpl( QWidget *parent, const char *name, bool modal, WFlags fl )
	: DlgSettings ( parent, name, modal, fl )
{
	// ---------------------------------------------------------------------

	w_export_browser-> setChecked ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ));

	w_local_currency-> setChecked ( CMoney::inst ( )-> isLocalized ( ));
	w_local_currency_label-> setText ( w_local_currency_label-> text ( ). arg ( CMoney::inst ( )-> localCurrencySymbol ( )));
	w_local_currency_factor-> setText ( QString::number ( CMoney::inst ( )-> factor ( )));
	w_local_currency_factor-> setValidator ( new QDoubleValidator ( w_local_currency_factor ));
	
	w_weight-> setButton (( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightMetric ) ? 0 : 1 );

	w_infobar_look-> setCurrentItem ( CConfig::inst ( )-> infoBarLook ( ));

	w_doc_dir-> setText ( CConfig::inst ( )-> documentDir ( ));
	connect ( w_doc_dir_select, SIGNAL( clicked ( )), this, SLOT( selectDocDir ( )));
	w_doc_dir_select-> setIconSet ( CResource::inst ( )-> iconSet ( "file_open" ));

	// ---------------------------------------------------------------------

	int db, inv, pic, pg;
	int dbd, invd, picd, pgd;
	
	CConfig::inst ( )-> blUpdateIntervals ( db, inv, pic, pg );
	CConfig::inst ( )-> blUpdateIntervalsDefaults ( dbd, invd, picd, pgd );

	w_cache_databases-> setChecked (( db ));
	w_days_databases-> setValue ( db ? sec2day( db ) : dbd );

	w_cache_inventories-> setChecked (( inv ));
	w_days_inventories-> setValue ( inv ? sec2day( inv ) : invd );

	w_cache_pictures-> setChecked (( pic ));
	w_days_pictures-> setValue ( pic ? sec2day( pic ) : picd );

	w_cache_priceguides-> setChecked (( pg ));
	w_days_priceguides-> setValue ( pg ? sec2day( pg ) : pgd );

	w_cache_dir-> setText ( BrickLink::inst ( )-> dataPath ( ));
	connect ( w_cache_dir_select, SIGNAL( clicked ( )), this, SLOT( selectCacheDir ( )));
	w_cache_dir_select-> setIconSet ( CResource::inst ( )-> iconSet ( "file_open" ));

	// ---------------------------------------------------------------------

	const BrickLink::ItemType *itype;

	itype = BrickLink::inst ( )-> itemType ( CConfig::inst ( )-> readNumEntry ( "/Defaults/ImportInventory/ItemType", 'S' ));
	m_def_inv_itemtype = new CItemTypeCombo ( w_def_inv_itemtype, true );
	m_def_inv_itemtype-> setCurrentItemType ( itype ? itype : BrickLink::inst ( )-> itemType ( 'S' ));

	itype = BrickLink::inst ( )-> itemType ( CConfig::inst ( )-> readNumEntry ( "/Defaults/AddItems/ItemType", 'P' ));
	m_def_add_itemtype = new CItemTypeCombo ( w_def_add_itemtype, false );
	m_def_add_itemtype-> setCurrentItemType ( itype ? itype : BrickLink::inst ( )-> itemType ( 'P' ));

	w_def_add_itemcond-> setButton ( CConfig::inst ( )-> readEntry ( "/Defaults/AddItems/Condition", "new" ) == "new" ? 0 : 1 );

	QStringList timel, pricel;

	timel << tr( "All Time Sales" ) << tr( "Past 6 Months Sales" ) << tr( "Current Inventory" );
	pricel << tr( "Lowest" ) << tr( "Average" ) << tr( "Weighted Average" ) << tr( "Highest" );

	w_def_pg_type_time-> insertStringList ( timel );
	w_def_pg_type_price-> insertStringList ( pricel );

	int timedef = CConfig::inst ( )-> readNumEntry ( "/Defaults/SetToPG/Time", 1 );
	int pricedef = CConfig::inst ( )-> readNumEntry ( "/Defaults/SetToPG/Price", 1 );

	if (( timedef >= 0 ) && ( timedef < int( timel. count ( ))))
		w_def_pg_type_time-> setCurrentItem ( timedef );
	if (( pricedef >= 0 ) && ( pricedef < int( pricel. count ( ))))
		w_def_pg_type_price-> setCurrentItem ( pricedef );

	// ---------------------------------------------------------------------

	w_bl_username-> setText ( CConfig::inst ( )-> blLoginUsername ( ));
	w_bl_password-> setText ( CConfig::inst ( )-> blLoginPassword ( ));

	w_proxy_port-> setValidator ( new QIntValidator ( 1, 65535, w_proxy_port ));

	w_proxy_name-> setText ( CConfig::inst ( )-> proxyName ( ));
	w_proxy_port-> setText ( QString::number ( CConfig::inst ( )-> proxyPort ( )));

	w_proxy_enable-> setChecked ( CConfig::inst ( )-> useProxy ( ));

	// ---------------------------------------------------------------------
}

DlgSettingsImpl::~DlgSettingsImpl ( )
{
}

void DlgSettingsImpl::setCurrentPage ( const char *page )
{
	for ( int i = 0; i < w_tab-> count ( ); i++ ) {
		if ( qstrcmp ( w_tab-> page ( i )-> name ( ), page ) == 0 ) {
			w_tab-> setCurrentPage ( i );
			break;
		}
	}
}

void DlgSettingsImpl::selectCacheDir ( )
{
	QString newdir = QFileDialog::getExistingDirectory ( w_cache_dir-> text ( ), this, 0, tr( "Cache directory location" ));

	if ( !newdir. isNull ( ))
		w_cache_dir-> setText ( newdir );
}

void DlgSettingsImpl::selectDocDir ( )
{
	QString newdir = QFileDialog::getExistingDirectory ( w_doc_dir-> text ( ), this, 0, tr( "Document directory location" ));

	if ( !newdir. isNull ( ))
		w_doc_dir-> setText ( newdir );
}

void DlgSettingsImpl::done ( int r )
{
	if ( r == QDialog::Accepted ) {

		// ---------------------------------------------------------------------

		CConfig::inst ( )-> writeEntry ( "/General/Export/OpenBrowser",  w_export_browser-> isChecked ( ));

		CMoney::inst ( )-> setLocalization ( w_local_currency-> isChecked ( ));
		CMoney::inst ( )-> setFactor ( w_local_currency_factor-> text ( ). toDouble ( ));

		CConfig::inst ( )-> setInfoBarLook ( w_infobar_look-> currentItem ( ));

		CConfig::inst ( )-> setWeightSystem ( w_weight-> selectedId ( ) == 0 ? CConfig::WeightMetric : CConfig::WeightImperial );

		QDir dd ( w_doc_dir-> text ( ));
		if ( dd. exists ( ) && dd. isReadable ( ))
			CConfig::inst ( )-> setDocumentDir ( w_doc_dir-> text ( ));
		else
			CMessageBox::warning ( this, tr( "The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed." ));

		// ---------------------------------------------------------------------

		int db, inv, pic, pg;

		db  = day2sec( w_cache_databases->   isChecked ( ) ? w_days_databases->   value ( ) : 0 );
		inv = day2sec( w_cache_inventories-> isChecked ( ) ? w_days_inventories-> value ( ) : 0 );
		pic = day2sec( w_cache_pictures->    isChecked ( ) ? w_days_pictures->    value ( ) : 0 );
		pg  = day2sec( w_cache_priceguides-> isChecked ( ) ? w_days_priceguides-> value ( ) : 0 );

		CConfig::inst ( )-> setBlUpdateIntervals ( db, inv, pic, pg );

		QDir cd ( w_cache_dir-> text ( ));
		if ( cd. exists ( ) && cd. isReadable ( ))
			CConfig::inst ( )-> writeEntry ( "/BrickLink/DataDir", w_cache_dir-> text ( ));
		else
			CMessageBox::warning ( this, tr( "The specified cache directory does not exist or is not read- and writeable.<br />The cache directory setting will not be changed." ));

		// ---------------------------------------------------------------------

		const BrickLink::ItemType *itype;

		itype = m_def_inv_itemtype-> currentItemType ( );
		if ( itype )
			CConfig::inst ( )-> writeEntry ( "/Defaults/ImportInventory/ItemType", itype-> id ( ));

		itype = m_def_add_itemtype-> currentItemType ( );
		if ( itype )
			CConfig::inst ( )-> writeEntry ( "/Defaults/AddItems/ItemType", itype-> id ( ));
		CConfig::inst ( )-> writeEntry ( "/Defaults/AddItems/Condition", w_def_add_itemcond-> selectedId ( ) == 0 ? "new" : "used" );

		CConfig::inst ( )-> writeEntry ( "/Defaults/SetToPG/Time",  w_def_pg_type_time-> currentItem ( ));
		CConfig::inst ( )-> writeEntry ( "/Defaults/SetToPG/Price", w_def_pg_type_price-> currentItem ( ));
		
		// ---------------------------------------------------------------------

		CConfig::inst ( )-> setBlLoginUsername ( w_bl_username-> text ( ));
		CConfig::inst ( )-> setBlLoginPassword ( w_bl_password-> text ( ));
	
		CConfig::inst ( )-> setProxy ( w_proxy_enable-> isChecked ( ), w_proxy_name-> text ( ), w_proxy_port-> text ( ). toInt ( ));
	}

	DlgSettings::done ( r );
}

