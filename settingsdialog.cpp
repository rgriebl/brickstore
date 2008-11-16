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
#include <QBuffer>
#include <QHttp>

#include "settingsdialog.h"
#include "config.h"
#include "bricklink.h"
#include "messagebox.h"

static int sec2day ( int s )
{
	return ( s + 60*60*24 - 1 ) / ( 60*60*24 );
}

static int day2sec ( int d )
{
	return d * ( 60*60*24 );
}


SettingsDialog::SettingsDialog(const QString &start_on_page, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f), m_http(0), m_buffer(0)
{
    setupUi(this);
    
    w_upd_reset->setAttribute(Qt::WA_MacSmallSize);

    connect(w_rate_fixed, SIGNAL(toggled(bool)), this, SLOT(rateTypeToggled(bool)));
    connect(w_docdir_select, SIGNAL(clicked()), this, SLOT(selectDocDir()));
    connect(w_upd_reset, SIGNAL(clicked()), this, SLOT(resetUpdateIntervals()));
    connect(w_rate_ecb, SIGNAL(clicked()), this, SLOT(getRateFromECB()));

    w_proxy_port->setValidator(new QIntValidator(1, 65535, w_proxy_port));

    load();

    if (!start_on_page.isEmpty()) {
        if (QWidget *w = findChild<QWidget *>(start_on_page))
            w_tabs->setCurrentWidget(w);
    }
}

void SettingsDialog::getRateFromECB()
{
    w_rate_ecb->setEnabled(false);
    if (!m_http) {
        m_http = new QHttp(this);
        connect(m_http, SIGNAL(done(bool)), this, SLOT(gotRateFromECB(bool)));
        m_buffer = new QBuffer(m_http);
    }
    m_buffer->open(QIODevice::ReadWrite);
    m_http->setHost(QLatin1String("www.ecb.europa.eu"));
    m_http->get(QLatin1String("/stats/eurofxref/eurofxref-daily.xml"), m_buffer);
}

void SettingsDialog::gotRateFromECB(bool error)
{
    m_buffer->close();
    if (error) {
        MessageBox::warning(this, tr("There was an error downloading the exchange rates from the ECB server:<br>%2").arg(m_http->errorString()));
    } else {
        QDomDocument doc;
        bool parse_ok = false;

        if (doc.setContent(m_buffer->data())) {
            QDomNodeList cubelist = doc.elementsByTagName(QLatin1String("Cube"));
            double usd_eur = -1, xxx_eur = -1;
            QString find_currency = w_currency->lineEdit()->text().trimmed();

            for (int i = 0; i < cubelist.count(); ++i) {
                QDomElement el = cubelist.at(i).toElement();
                QString currency = el.attribute(QLatin1String("currency"));
                double rate = el.attribute(QLatin1String("rate")).toDouble();

                if (!currency.isEmpty() && rate) {
                    if (currency == QLatin1String("USD"))
                        usd_eur = rate;
                    else if (currency.compare(find_currency, Qt::CaseInsensitive) == 0)
                        xxx_eur = rate;
                }
            }

            parse_ok = (usd_eur > 0);

            if (find_currency.compare(QLatin1String("EUR"), Qt::CaseInsensitive) == 0)
                xxx_eur = 1;
            if (parse_ok && xxx_eur < 0)
                MessageBox::information(this, tr("The ECB exchange rate service doesn't list the requested currency"));
            if (parse_ok && xxx_eur > 0)
                w_rate->setText(QString::number(xxx_eur / usd_eur));
        }
        if (!parse_ok)
            MessageBox::warning(this, tr("There was an error parsing the exchange rates from the ECB server"));
    }
    m_buffer->close();
    w_rate_ecb->setEnabled(true);
}

void SettingsDialog::rateTypeToggled(bool fixed)
{
    w_rate->setReadOnly(!fixed);
}

void SettingsDialog::selectDocDir()
{
	QString newdir = QFileDialog::getExistingDirectory(this, tr("Document directory location"), w_docdir->text());

	if (!newdir.isNull())
		w_docdir->setText(QDir::convertSeparators(newdir));
}

void SettingsDialog::resetUpdateIntervals()
{
	QMap<QByteArray, int> intervals = Config::inst()->updateIntervalsDefault();

	w_upd_picture->setValue(sec2day(intervals["Picture"]));
	w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
	w_upd_database->setValue(sec2day(intervals["Database"]));
	w_upd_ldraw->setValue(sec2day(intervals["LDraw"]));
}

void SettingsDialog::accept()
{
    save();
    QDialog::accept();
}


void SettingsDialog::load()
{
	// --[ GENERAL ]-------------------------------------------------------------------

	QList<Config::Translation> translations = Config::inst()->translations();

	if (translations.isEmpty()) {
		w_language->setEnabled(false);
	}
	else {
	    bool localematch = false;
	    QLocale l_active;

		foreach (const Config::Translation &trans, translations) {
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

    w_metric->setChecked(Config::inst()->isMeasurementMetric());
	w_imperial->setChecked(Config::inst()->isMeasurementImperial());

	w_rate_fixed->setChecked(true);
	//w_rate_daily->setChecked(false);
    w_currency->setEditText("EUR");
	w_rate->setText("0.67");
	/*
	w_local_currency->setChecked(Money::inst()->isLocalized());
	w_local_currency_label->setText(w_local_currency_label->text().arg(Money::inst()->localCurrencySymbol()));
	w_local_currency_factor->setText(QString::number(Money::inst()->factor()));
	w_local_currency_factor->setValidator(newQDoubleValidator(w_local_currency_factor));
	*/

	w_openbrowser->setChecked(Config::inst()->value("/General/Export/OpenBrowser", true).toBool());
	w_closeempty->setChecked(Config::inst()->closeEmptyDocuments());

	w_docdir->setText(QDir::convertSeparators(Config::inst()->documentDir()));

	// --[ UPDATES ]-------------------------------------------------------------------

	QMap<QByteArray, int> intervals = Config::inst()->updateIntervals();

	w_upd_picture->setValue(sec2day(intervals["Picture"]));
	w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
	w_upd_database->setValue(sec2day(intervals["Database"]));
	w_upd_ldraw->setValue(sec2day(intervals["LDraw"]));

	// --[ DEFAULTS ]-------------------------------------------------------------------

	const BrickLink::ItemType *itype;

	itype = BrickLink::core()->itemType(Config::inst()->value("/Defaults/ImportInventory/ItemType", 'S').toInt());
	BrickLink::ItemTypeModel *importmodel = new BrickLink::ItemTypeModel(this);
	importmodel->setFilterWithoutInventory(true);
	w_def_import_type->setModel(importmodel);

	int importdef = importmodel->index(itype ? itype : BrickLink::core()->itemType('S')).row();
	w_def_import_type->setCurrentIndex(importdef);

	itype = BrickLink::core()->itemType(Config::inst()->value("/Defaults/AddItems/ItemType", 'P').toInt());
	BrickLink::ItemTypeModel *addmodel = new BrickLink::ItemTypeModel(this);
	w_def_add_type->setModel(addmodel);

	int adddef = addmodel->index(itype ? itype : BrickLink::core()->itemType('P')).row();
	w_def_add_type->setCurrentIndex(adddef);

	w_def_add_cond_new->setChecked(Config::inst()->value("/Defaults/AddItems/Condition", "new").toString() == QLatin1String("new"));

	QStringList timel, pricel;

	timel << tr("All Time Sales") << tr("Last 6 Months Sales") << tr("Current Inventory");
	pricel << tr("Minimum") << tr("Average") << tr("Quantity Average") << tr("Maximum");

	w_def_setpg_time->addItems(timel);
	w_def_setpg_price->addItems(pricel);

	int timedef = Config::inst()->value("/Defaults/SetToPG/Time", 1).toInt();
	int pricedef = Config::inst()->value("/Defaults/SetToPG/Price", 1).toInt();

    w_def_setpg_time->setCurrentIndex(timedef);
    w_def_setpg_price->setCurrentIndex(pricedef);

	// --[ NETWORK ]-------------------------------------------------------------------

	QPair<QString, QString> blcred = Config::inst()->loginForBrickLink();

	w_bl_username->setText(blcred.first);
	w_bl_password->setText(blcred.second);

	QNetworkProxy proxy = Config::inst()->proxy();

	w_proxy_host->setText(proxy.hostName());
	w_proxy_port->setEditText(QString::number(proxy.port()));
	w_proxy_user->setText(proxy.user());
	w_proxy_pass->setText(proxy.password());

	w_proxy_enable->setChecked(proxy.type() != QNetworkProxy::NoProxy);

	// ---------------------------------------------------------------------
}


void SettingsDialog::save()
{
}
