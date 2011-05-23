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

#include "settingsdialog.h"
#include "config.h"
#include "bricklink.h"
#include "utility.h"
#include "messagebox.h"

static int sec2day(int s)
{
    return (s + 60*60*24 - 1) / (60*60*24);
}

static int day2sec(int d)
{
    return d * (60*60*24);
}


SettingsDialog::SettingsDialog(const QString &start_on_page, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    m_currency_status_fmt = w_currency_status->text();
    w_currency_status->setText(QString());

    w_upd_reset->setAttribute(Qt::WA_MacSmallSize);

    connect(w_docdir_select, SIGNAL(clicked()), this, SLOT(selectDocDir()));
    connect(w_upd_reset, SIGNAL(clicked()), this, SLOT(resetUpdateIntervals()));
    connect(w_currency, SIGNAL(currentIndexChanged(QString)), this, SLOT(currentCurrencyChanged(QString)));
    connect(w_currency_status, SIGNAL(linkActivated(QString)), Currency::inst(), SLOT(updateRates()));
    connect(Currency::inst(), SIGNAL(ratesChanged()), this, SLOT(currenciesUpdated()));

    load();

    QWidget *w = w_tabs->widget(0);
    if (!start_on_page.isEmpty())
        w = findChild<QWidget *>(start_on_page);
    w_tabs->setCurrentWidget(w);
}

void SettingsDialog::currentCurrencyChanged(const QString &ccode)
{
    qreal rate = Currency::inst()->rate(ccode);

    QString fmt;
    if (!rate)
        fmt = tr("could not find a cross rate for %1").arg(ccode);
    else if (rate != qreal(1))
        fmt = tr("1 %1 equals %2 USD").arg(ccode).arg(qreal(1) / rate, 0, 'g', 3);

    w_currency_status->setText(m_currency_status_fmt.arg(fmt));
    m_preferedCurrency = ccode;
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

void SettingsDialog::currenciesUpdated()
{
    QString oldprefered = m_preferedCurrency;
    QStringList currencies = Currency::inst()->currencyCodes();
    currencies.sort();
    currencies.removeOne(QLatin1String("USD"));
    currencies.prepend(QLatin1String("USD"));
    w_currency->clear();
    w_currency->insertItems(0, currencies);
    if (currencies.count() > 1)
        w_currency->insertSeparator(1);
    w_currency->setCurrentIndex(qMax(0, w_currency->findText(oldprefered)));

//    currentCurrencyChanged(w_currency->currentText());
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
            if (trans.language == QLatin1String("en"))
                w_language->addItem(trans.languageName[QLatin1String("en")], trans.language);
            else
                w_language->addItem(QString("%1 (%2)").arg(trans.languageName[QLatin1String("en")], trans.languageName[trans.language]), trans.language);

            QLocale l(trans.language);

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

    m_preferedCurrency = Config::inst()->defaultCurrencyCode();
    currenciesUpdated();

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

    bool addnew = Config::inst()->value("/Defaults/AddItems/Condition", "new").toString() == QLatin1String("new");
    w_def_add_cond_new->setChecked(addnew);
    w_def_add_cond_used->setChecked(!addnew);

    w_def_setpg_time->addItem(tr("Last 6 Months Sales"), BrickLink::PastSix);
    w_def_setpg_time->addItem(tr("Current Inventory"), BrickLink::Current);
    
    w_def_setpg_price->addItem(tr("Minimum"), BrickLink::Lowest);
    w_def_setpg_price->addItem(tr("Average"), BrickLink::Average);
    w_def_setpg_price->addItem(tr("Quantity Average"), BrickLink::WAverage);
    w_def_setpg_price->addItem(tr("Maximum"), BrickLink::Highest);

    BrickLink::Time timedef = static_cast<BrickLink::Time>(Config::inst()->value(QLatin1String("/Defaults/SetToPG/Time"), BrickLink::PastSix).toInt());
    BrickLink::Price pricedef = static_cast<BrickLink::Price>(Config::inst()->value(QLatin1String("/Defaults/SetToPG/Price"), BrickLink::Average).toInt());

    w_def_setpg_time->setCurrentIndex(w_def_setpg_time->findData(timedef));
    w_def_setpg_price->setCurrentIndex(w_def_setpg_price->findData(pricedef));

    // --[ NETWORK ]-------------------------------------------------------------------

    QPair<QString, QString> blcred = Config::inst()->loginForBrickLink();

    w_bl_username->setText(blcred.first);
    w_bl_password->setText(blcred.second);

    // ---------------------------------------------------------------------
}


void SettingsDialog::save()
{
    // --[ GENERAL ]-------------------------------------------------------------------

    if (w_language->currentIndex() >= 0)
        Config::inst()->setLanguage(w_language->itemData(w_language->currentIndex()).toString());
    Config::inst()->setMeasurementSystem(w_imperial->isChecked() ? QLocale::ImperialSystem : QLocale::MetricSystem);
    Config::inst()->setDefaultCurrencyCode(m_preferedCurrency);

    QDir dd(w_docdir->text());
    if (dd.exists() && dd.isReadable())
        Config::inst()->setDocumentDir(w_docdir->text());
    else
        MessageBox::warning(this, tr("The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed."));

    Config::inst()->setCloseEmptyDocuments(w_closeempty->isChecked ());
    Config::inst()->setValue("/General/Export/OpenBrowser", w_openbrowser->isChecked());

    // --[ UPDATES ]-------------------------------------------------------------------

    QMap<QByteArray, int> intervals;

    intervals.insert("Picture", day2sec(w_upd_picture->value()));
    intervals.insert("PriceGuide", day2sec(w_upd_priceguide->value()));
    intervals.insert("Database", day2sec(w_upd_database->value()));
    intervals.insert("LDraw", day2sec(w_upd_ldraw->value()));

    Config::inst()->setUpdateIntervals(intervals);

    // --[ DEFAULTS ]-------------------------------------------------------------------

    const BrickLink::ItemType *itype;
    BrickLink::ItemTypeModel *model;

    model = static_cast<BrickLink::ItemTypeModel *>(w_def_import_type->model());
    itype = model->itemType(model->index(w_def_import_type->currentIndex(), 0));
    Config::inst()->setValue("/Defaults/ImportInventory/ItemType", itype->id());

    model = static_cast<BrickLink::ItemTypeModel *>(w_def_add_type->model());
    itype = model->itemType(model->index(w_def_add_type->currentIndex(), 0));
    Config::inst()->setValue("/Defaults/AddItems/ItemType", itype->id());

    Config::inst()->setValue("/Defaults/AddItems/Condition", QLatin1String(w_def_add_cond_new->isChecked() ? "new" : "used"));
    Config::inst()->setValue("/Defaults/SetToPG/Time", w_def_setpg_time->itemData(w_def_setpg_time->currentIndex()));
    Config::inst()->setValue("/Defaults/SetToPG/Price", w_def_setpg_price->itemData(w_def_setpg_price->currentIndex()));

    // --[ NETWORK ]-------------------------------------------------------------------

    Config::inst()->setLoginForBrickLink(w_bl_username->text(), w_bl_password->text());
}
