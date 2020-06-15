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
#include <qtoolbutton.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <q3filedialog.h>
#include <qlabel.h>
#include <qvalidator.h>
#include <q3buttongroup.h>
#include <qtabwidget.h>

#include "cmessagebox.h"
#include "bricklink.h"
#include "cconfig.h"
#include "citemtypecombo.h"
#include "cmoney.h"
#include "cresource.h"

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

DlgSettingsImpl::DlgSettingsImpl( QWidget *parent, const char *name, bool modal, Qt::WFlags fl )
	: DlgSettings ( parent, name, modal, fl )
{
	// ---------------------------------------------------------------------

	QLocale l_active;
	bool found_best_match = false;

	w_export_browser-> setChecked ( CConfig::inst ( )-> readBoolEntry ( "/General/Export/OpenBrowser", true ));

	readAvailableLanguages ( );

	if ( m_languages. isEmpty ( )) {
		w_language-> setEnabled ( false );
	}
	else {
		for ( LanguageList::const_iterator it = m_languages. begin ( ); it != m_languages. end ( ); ++it ) {
			w_language-> insertItem ((*it). second );

			QLocale l ((*it). first );

			if ( !found_best_match ) {
				if ( l. language ( ) == l_active. language ( )) {
					if ( l. country ( ) == l_active. country ( )) {
						found_best_match = true;
					}
					w_language-> setCurrentItem ( w_language-> count ( ) - 1 );
				}
			}
		}
	}

	w_local_currency-> setChecked ( CMoney::inst ( )-> isLocalized ( ));
	w_local_currency_label-> setText ( w_local_currency_label-> text ( ). arg ( CMoney::inst ( )-> localCurrencySymbol ( )));
	w_local_currency_factor-> setText ( QString::number ( CMoney::inst ( )-> factor ( )));
	w_local_currency_factor-> setValidator ( new QDoubleValidator ( w_local_currency_factor ));
	
	w_weight-> setButton (( CConfig::inst ( )-> weightSystem ( ) == CConfig::WeightMetric ) ? 0 : 1 );

	w_doc_dir-> setText ( QDir::convertSeparators ( CConfig::inst ( )-> documentDir ( )));
	connect ( w_doc_dir_select, SIGNAL( clicked ( )), this, SLOT( selectDocDir ( )));
    w_doc_dir_select-> setIcon ( CResource::inst ( )-> icon ( "file_open" ));

	w_doc_close_empty-> setChecked ( CConfig::inst ( )-> closeEmptyDocuments ( ));

	// ---------------------------------------------------------------------

	int pic, pg;
	int picd, pgd;
	
	CConfig::inst ( )-> blUpdateIntervals ( pic, pg );
	CConfig::inst ( )-> blUpdateIntervalsDefaults ( picd, pgd );

	w_cache_pictures-> setChecked (( pic ));
	w_days_pictures-> setValue ( pic ? sec2day( pic ) : picd );

	w_cache_priceguides-> setChecked (( pg ));
	w_days_priceguides-> setValue ( pg ? sec2day( pg ) : pgd );

	w_cache_dir-> setText ( QDir::convertSeparators ( BrickLink::inst ( )-> dataPath ( )));
	connect ( w_cache_dir_select, SIGNAL( clicked ( )), this, SLOT( selectCacheDir ( )));
    w_cache_dir_select-> setIcon ( CResource::inst ( )-> icon ( "file_open" ));

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

	timel << tr( "All Time Sales" ) << tr( "Last 6 Months Sales" ) << tr( "Current Inventory" );
	pricel << tr( "Minimum" ) << tr( "Average" ) << tr( "Quantity Average" ) << tr( "Maximum" );

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
	QString newdir = Q3FileDialog::getExistingDirectory ( w_cache_dir-> text ( ), this, 0, tr( "Cache directory location" ));

	if ( !newdir. isNull ( ))
		w_cache_dir-> setText ( QDir::convertSeparators ( newdir ));
}

void DlgSettingsImpl::selectDocDir ( )
{
	QString newdir = Q3FileDialog::getExistingDirectory ( w_doc_dir-> text ( ), this, 0, tr( "Document directory location" ));

	if ( !newdir. isNull ( ))
		w_doc_dir-> setText ( QDir::convertSeparators ( newdir ));
}

void DlgSettingsImpl::done ( int r )
{
	if ( r == QDialog::Accepted ) {

		// ---------------------------------------------------------------------

		CConfig::inst ( )-> writeEntry ( "/General/Export/OpenBrowser",  w_export_browser-> isChecked ( ));

		if ( w_language-> currentItem ( ) >= 0 )
			CConfig::inst ( )-> setLanguage ( m_languages [w_language-> currentItem ( )]. first );

		CMoney::inst ( )-> setLocalization ( w_local_currency-> isChecked ( ));
		CMoney::inst ( )-> setFactor ( w_local_currency_factor-> text ( ). toDouble ( ));

		CConfig::inst ( )-> setWeightSystem ( w_weight-> selectedId ( ) == 0 ? CConfig::WeightMetric : CConfig::WeightImperial );

		QDir dd ( w_doc_dir-> text ( ));
		if ( dd. exists ( ) && dd. isReadable ( ))
			CConfig::inst ( )-> setDocumentDir ( w_doc_dir-> text ( ));
		else
			CMessageBox::warning ( this, tr( "The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed." ));

		CConfig::inst ( )-> setCloseEmptyDocuments ( w_doc_close_empty-> isChecked  ( ));

		// ---------------------------------------------------------------------

		int pic, pg;

		pic = day2sec( w_cache_pictures->    isChecked ( ) ? w_days_pictures->    value ( ) : 0 );
		pg  = day2sec( w_cache_priceguides-> isChecked ( ) ? w_days_priceguides-> value ( ) : 0 );

		CConfig::inst ( )-> setBlUpdateIntervals ( pic, pg );

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


bool DlgSettingsImpl::readAvailableLanguages ( )
{
	m_languages. clear ( );

	QDomDocument doc;
	QFile file ( CResource::inst ( )-> locate ( "translations/translations.xml" ));

	if ( file. open ( QIODevice::ReadOnly )) {
		QString err_str;
		int err_line = 0, err_col = 0;
	
		if ( doc. setContent ( &file, &err_str, &err_line, &err_col )) {
			QDomElement root = doc. documentElement ( );

			if ( root. isElement ( ) && root. nodeName ( ) == "translations" ) {
				bool found_default = false;

				for ( QDomNode trans = root. firstChild ( ); !trans. isNull ( ); trans = trans. nextSibling ( )) {
					if ( !trans. isElement ( ) || ( trans. nodeName ( ) != "translation" ))
						continue;
					QDomNamedNodeMap map = trans. attributes ( );

					QString lang_id = map. namedItem ( "lang" ). toAttr ( ). value ( );
					bool lang_default = map.contains ( "default" );

					if ( lang_default ) {
						if ( found_default )
							goto error;
						found_default = true;
					}

					if ( lang_id. isEmpty ( ))
						goto error;

					QString defname, trname;

					for ( QDomNode name = trans. firstChild ( ); !name. isNull ( ); name = name. nextSibling ( )) {
						if ( !name. isElement ( ) || ( name. nodeName ( ) != "name" ))
							continue;
						QDomNamedNodeMap map = name. attributes ( );

						QString tr_id = map. namedItem ( "lang" ). toAttr ( ). value ( );

						if ( tr_id. isEmpty ( ))
							defname = name. toElement ( ). text ( );
						else if ( tr_id == lang_id )
							trname = name. toElement ( ). text ( );
					}

					if ( defname. isEmpty ( ))
						goto error;

					QPair <QString, QString> lang_pair;
					lang_pair. first = lang_id;
					if ( trname. isEmpty ( ))
						lang_pair. second = defname;
					else
						lang_pair. second = QString ( "%1 (%2)" ). arg( defname, trname );

					m_languages << lang_pair;
				}
				return true;
			}
		}
	}
error:
	return false;
}

