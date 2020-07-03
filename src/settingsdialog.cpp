/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QComboBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QImage>

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

static QString systemDirName(const QString &path)
{
    static const QStandardPaths::StandardLocation locations[] = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::MusicLocation,
        QStandardPaths::MoviesLocation,
        QStandardPaths::PicturesLocation,
        QStandardPaths::TempLocation,
        QStandardPaths::HomeLocation,
        QStandardPaths::DataLocation,
        QStandardPaths::CacheLocation
    };

    for (auto &location : locations) {
        if (QDir(QStandardPaths::writableLocation(location)) == QDir(path)) {
            QString name = QStandardPaths::displayName(location);
            if (!name.isEmpty())
                return name;
            else
                break;
        }
    }
    return path;
}


SettingsDialog::SettingsDialog(const QString &start_on_page, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    connect(w_font_size, &QAbstractSlider::valueChanged,
            this, [this](int v) {
        w_font_size_percent->setText(QString("%1 %").arg(v * 10));
        QFont f = font();
        qreal defaultFontSize = qApp->property("_bs_defaultFontSize").toReal();
        if (defaultFontSize <= 0)
            defaultFontSize = 10;
        f.setPointSizeF(defaultFontSize * qreal(v) / 10.);
        w_font_example->setFont(f);
    });

    connect(w_font_size_reset, &QToolButton::clicked,
            this, [this]() { w_font_size->setValue(10); });

    auto setImageSize = [this](int v) {
        w_item_image_size_percent->setText(QString("%1 %").arg(v * 10));
        int s = 80 * v / 10;
        QImage img(":/images/brickstore_icon.png");
        img = img.scaled(s, s);
        w_item_image_example->setPixmap(QPixmap::fromImage(img));
    };

    connect(w_item_image_size, &QAbstractSlider::valueChanged,
            this, setImageSize);
    setImageSize(10);

    connect(w_item_image_size_reset, &QToolButton::clicked,
            this, [this]() { w_item_image_size->setValue(10); });



    w_upd_reset->setAttribute(Qt::WA_MacSmallSize);
#if defined(Q_OS_MACOS)
    w_currency_update->setStyle(QStyleFactory::create("fusion"));
#endif

    w_docdir->insertItem(0, style()->standardIcon(QStyle::SP_DirIcon), QString());
    w_docdir->insertItem(1, QIcon(), tr("Other..."));
    w_docdir->insertSeparator(1);

    int is = fontMetrics().height();
    w_currency_update->setIconSize(QSize(is, is));

    connect(w_docdir, QOverload<int>::of(&QComboBox::activated),
            this, &SettingsDialog::selectDocDir);
    connect(w_upd_reset, &QAbstractButton::clicked,
            this, &SettingsDialog::resetUpdateIntervals);
    connect(w_currency, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::currentCurrencyChanged);
    connect(w_currency_update, &QAbstractButton::clicked,
            Currency::inst(), &Currency::updateRates);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &SettingsDialog::currenciesUpdated);

    load();

    QWidget *w = w_tabs->widget(0);
    if (!start_on_page.isEmpty())
        w = findChild<QWidget *>(start_on_page);
    w_tabs->setCurrentWidget(w);
}

void SettingsDialog::currentCurrencyChanged(const QString &ccode)
{
    qreal rate = Currency::inst()->rate(ccode);

    QString s;
    if (qFuzzyIsNull(rate))
        s = tr("could not find a cross rate for %1").arg(ccode);
    else if (!qFuzzyCompare(rate, 1))
        s = tr("1 %1 equals %2 USD").arg(ccode).arg(qreal(1) / rate, 0, 'f', 3);

    w_currency_status->setText(s);
    m_preferedCurrency = ccode;
}

void SettingsDialog::selectDocDir(int index)
{
    if (index > 0) {
        QString newdir = QFileDialog::getExistingDirectory(this, tr("Document directory location"), w_docdir->itemData(0).toString());

        if (!newdir.isNull()) {
            w_docdir->setItemData(0, QDir::toNativeSeparators(newdir));
            w_docdir->setItemText(0, systemDirName(newdir));
        }
    }
    w_docdir->setCurrentIndex(0);
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

        for (const auto &trans : translations) {
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

    QString docdir = QDir::toNativeSeparators(Config::inst()->documentDir());
    w_docdir->setItemData(0, docdir);
    w_docdir->setItemText(0, systemDirName(docdir));

    // --[ INTERFACE ]-----------------------------------------------------------------

    int iconSizeIndex = 0;
    auto iconSize = Config::inst()->iconSize();
    if (iconSize == QSize(22, 22))
        iconSizeIndex = 1;
    else if (iconSize == QSize(32, 32))
        iconSizeIndex = 2;

    w_icon_size->setCurrentIndex(iconSizeIndex);
    w_font_size->setValue(Config::inst()->fontSizePercent() / 10);

    w_item_image_size->setValue(Config::inst()->itemImageSizePercent() / 10);

    // --[ UPDATES ]-------------------------------------------------------------------

    QMap<QByteArray, int> intervals = Config::inst()->updateIntervals();

    w_upd_picture->setValue(sec2day(intervals["Picture"]));
    w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
    w_upd_database->setValue(sec2day(intervals["Database"]));
    w_upd_ldraw->setValue(sec2day(intervals["LDraw"]));

    // --[ DEFAULTS ]-------------------------------------------------------------------

    const BrickLink::ItemType *itype;

    itype = BrickLink::core()->itemType(Config::inst()->value("/Defaults/ImportInventory/ItemType", 'S').toChar().toLatin1());
    auto *importmodel = new BrickLink::ItemTypeModel(this);
    importmodel->setFilterWithoutInventory(true);
    w_def_import_type->setModel(importmodel);

    int importdef = importmodel->index(itype ? itype : BrickLink::core()->itemType('S')).row();
    w_def_import_type->setCurrentIndex(importdef);

    itype = BrickLink::core()->itemType(Config::inst()->value("/Defaults/AddItems/ItemType", 'P').toChar().toLatin1());
    auto *addmodel = new BrickLink::ItemTypeModel(this);
    w_def_add_type->setModel(addmodel);

    int adddef = addmodel->index(itype ? itype : BrickLink::core()->itemType('P')).row();
    w_def_add_type->setCurrentIndex(adddef);

    bool addnew = Config::inst()->value("/Defaults/AddItems/Condition", "new").toString() == QLatin1String("new");
    w_def_add_cond_new->setChecked(addnew);
    w_def_add_cond_used->setChecked(!addnew);

    w_def_setpg_time->addItem(tr("Last 6 Months Sales"), int(BrickLink::Time::PastSix));
    w_def_setpg_time->addItem(tr("Current Inventory"), int(BrickLink::Time::Current));
    
    w_def_setpg_price->addItem(tr("Minimum"), int(BrickLink::Price::Lowest));
    w_def_setpg_price->addItem(tr("Average"), int(BrickLink::Price::Average));
    w_def_setpg_price->addItem(tr("Quantity Average"), int(BrickLink::Price::WAverage));
    w_def_setpg_price->addItem(tr("Maximum"), int(BrickLink::Price::Highest));

    int timedef = Config::inst()->value(QLatin1String("/Defaults/SetToPG/Time"), int(BrickLink::Time::PastSix)).toInt();
    int pricedef = Config::inst()->value(QLatin1String("/Defaults/SetToPG/Price"), int(BrickLink::Price::Average)).toInt();

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

    QDir dd(w_docdir->itemData(0).toString());

    if (dd.exists() && dd.isReadable())
        Config::inst()->setDocumentDir(dd.absolutePath());
    else
        MessageBox::warning(this, tr("The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed."));

    Config::inst()->setCloseEmptyDocuments(w_closeempty->isChecked ());
    Config::inst()->setValue("/General/Export/OpenBrowser", w_openbrowser->isChecked());

    // --[ INTERFACE ]-----------------------------------------------------------------

    int iconWidth = 0;
    switch (w_icon_size->currentIndex()) {
    case 1: iconWidth = 22; break;
    case 2: iconWidth = 32; break;
    default: break;
    }

    Config::inst()->setIconSize(QSize(iconWidth, iconWidth));
    Config::inst()->setFontSizePercent(w_font_size->value() * 10);

    Config::inst()->setItemImageSizePercent(w_item_image_size->value() * 10);

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
    if ((itype = model->itemType(model->index(w_def_import_type->currentIndex(), 0))))
        Config::inst()->setValue("/Defaults/ImportInventory/ItemType", itype->id());

    model = static_cast<BrickLink::ItemTypeModel *>(w_def_add_type->model());
    if ((itype = model->itemType(model->index(w_def_add_type->currentIndex(), 0))))
        Config::inst()->setValue("/Defaults/AddItems/ItemType", itype->id());

    Config::inst()->setValue("/Defaults/AddItems/Condition", QLatin1String(w_def_add_cond_new->isChecked() ? "new" : "used"));
    Config::inst()->setValue("/Defaults/SetToPG/Time", w_def_setpg_time->itemData(w_def_setpg_time->currentIndex()));
    Config::inst()->setValue("/Defaults/SetToPG/Price", w_def_setpg_price->itemData(w_def_setpg_price->currentIndex()));

    // --[ NETWORK ]-------------------------------------------------------------------

    Config::inst()->setLoginForBrickLink(w_bl_username->text(), w_bl_password->text());
}

#include "moc_settingsdialog.cpp"
