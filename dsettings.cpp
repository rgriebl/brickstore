/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <QTabWidget>
#include <QFileDialog>
#include <QNetworkProxy>

#include "dsettings.h"
#include "cconfig.h"
#include "bricklink.h"

static int sec2day ( int s )
{
	return ( s + 60*60*24 - 1 ) / ( 60*60*24 );
}

static int day2sec ( int d )
{
	return d * ( 60*60*24 );
}


DSettings::DSettings(const QString &start_on_page, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);
    
    connect(w_rate_fixed, SIGNAL(toggled(bool)), this, SLOT(rateTypeToggled(bool)));    
    connect(w_docdir_select, SIGNAL(clicked()), this, SLOT(selectDocDir()));   
    connect(w_upd_reset, SIGNAL(clicked()), this, SLOT(resetUpdateIntervals()));
    
    w_proxy_port->setValidator(new QIntValidator(1, 65535, w_proxy_port));

    load();
    
    if (!start_on_page.isEmpty()) {
        if (QWidget *w = findChild<QWidget *>(start_on_page))
            w_tabs->setCurrentWidget(w);
    }
}

void DSettings::rateTypeToggled(bool fixed)
{
    w_rate->setReadOnly(!fixed);
}

void DSettings::selectDocDir()
{
	QString newdir = QFileDialog::getExistingDirectory(this, tr("Document directory location"), w_docdir->text());

	if (!newdir.isNull())
		w_docdir->setText(QDir::convertSeparators(newdir));
}

void DSettings::resetUpdateIntervals()
{
	QMap<QByteArray, int> intervals = CConfig::inst()->updateIntervalsDefault();
	
	w_upd_picture->setValue(sec2day(intervals["Picture"]));
	w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
	w_upd_database->setValue(sec2day(intervals["Database"]));
	w_upd_ldraw->setValue(sec2day(intervals["LDraw"]));
}

void DSettings::accept()
{
    save();
    QDialog::accept();
}


void DSettings::load()
{
	// --[ GENERAL ]-------------------------------------------------------------------

	QList<CConfig::Translation> translations = CConfig::inst()->translations();

	if (translations.isEmpty()) {
		w_language->setEnabled(false);
	}
	else {
	    bool localematch = false;
	    QLocale l_active;
	
		foreach (const CConfig::Translation &trans, translations) {
			w_language->addItem(QString("%1 (%2)").arg(trans.m_names[QLatin1String("en")], trans.m_names[trans.m_langid]));

			QLocale l(trans.m_langid);

			if (!localematch) {
				if (l.language() == l_active.language()) {
					if (l.country() == l_active.country())
						localematch = true;
						
					w_language->setCurrentIndex(w_language->count()-1);
				}
			}
 		}
	}

    w_metric->setChecked(CConfig::inst()->isMeasurementMetric());
	w_imperial->setChecked(CConfig::inst()->isMeasurementImperial());

	w_rate_fixed->setChecked(true);
	w_rate_daily->setChecked(false);
    w_currency->setEditText("EUR");
	w_rate->setText("0.67");
	/*
	w_local_currency->setChecked(CMoney::inst()->isLocalized());
	w_local_currency_label->setText(w_local_currency_label->text().arg(CMoney::inst()->localCurrencySymbol()));
	w_local_currency_factor->setText(QString::number(CMoney::inst()->factor()));
	w_local_currency_factor->setValidator(newQDoubleValidator(w_local_currency_factor));
	*/
	
	w_openbrowser->setChecked(CConfig::inst()->value("/General/Export/OpenBrowser", true).toBool());
	w_closeempty->setChecked(CConfig::inst()->closeEmptyDocuments());

	w_docdir->setText(QDir::convertSeparators(CConfig::inst()->documentDir()));

	// --[ UPDATES ]-------------------------------------------------------------------

	QMap<QByteArray, int> intervals = CConfig::inst()->updateIntervals();
	
	w_upd_picture->setValue(sec2day(intervals["Picture"]));
	w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
	w_upd_database->setValue(sec2day(intervals["Database"]));
	w_upd_ldraw->setValue(sec2day(intervals["LDraw"]));

	// --[ DEFAULTS ]-------------------------------------------------------------------

	const BrickLink::ItemType *itype;

	itype = BrickLink::core()->itemType(CConfig::inst()->value("/Defaults/ImportInventory/ItemType", 'S').toInt());
	BrickLink::ItemTypeProxyModel *importmodel = new BrickLink::ItemTypeProxyModel(BrickLink::core()->itemTypeModel());
	importmodel->setFilterWithoutInventory(true);
	w_def_import_type->setModel(importmodel);
	
	int importdef = importmodel->index(itype ? itype : BrickLink::core()->itemType('S')).row();
	w_def_import_type->setCurrentIndex(importdef);

	itype = BrickLink::core()->itemType(CConfig::inst()->value("/Defaults/AddItems/ItemType", 'P').toInt());
	BrickLink::ItemTypeProxyModel *addmodel = new BrickLink::ItemTypeProxyModel(BrickLink::core()->itemTypeModel());
	w_def_add_type->setModel(addmodel);
	
	int adddef = addmodel->index(itype ? itype : BrickLink::core()->itemType('P')).row();
	w_def_add_type->setCurrentIndex(adddef);

	w_def_add_cond_new->setChecked(CConfig::inst()->value("/Defaults/AddItems/Condition", "new").toString() == QLatin1String("new"));

	QStringList timel, pricel;

	timel << tr("All Time Sales") << tr("Last 6 Months Sales") << tr("Current Inventory");
	pricel << tr("Minimum") << tr("Average") << tr("Quantity Average") << tr("Maximum");

	w_def_setpg_time->addItems(timel);
	w_def_setpg_price->addItems(pricel);

	int timedef = CConfig::inst()->value("/Defaults/SetToPG/Time", 1).toInt();
	int pricedef = CConfig::inst()->value("/Defaults/SetToPG/Price", 1).toInt();

    w_def_setpg_time->setCurrentIndex(timedef);
    w_def_setpg_price->setCurrentIndex(pricedef);

	// --[ NETWORK ]-------------------------------------------------------------------

	QPair<QString, QString> blcred = CConfig::inst()->loginForBrickLink();

	w_bl_username->setText(blcred.first);
	w_bl_password->setText(blcred.second);

	QNetworkProxy proxy = CConfig::inst()->proxy();
	
	w_proxy_host->setText(proxy.hostName());
	w_proxy_port->setEditText(QString::number(proxy.port()));
	w_proxy_user->setText(proxy.user());
	w_proxy_pass->setText(proxy.password());

	w_proxy_enable->setChecked(proxy.type() != QNetworkProxy::NoProxy);

	// ---------------------------------------------------------------------
}


void DSettings::save()
{
}
