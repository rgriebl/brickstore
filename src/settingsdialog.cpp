/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include "bricklink_model.h"
#include "ldraw.h"
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
        QStandardPaths::AppLocalDataLocation,
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

    w_font_size_percent->setFixedWidth(w_font_size_percent->width());
    w_item_image_size_percent->setFixedWidth(w_item_image_size_percent->width());

    auto setFontSize = [this](int v) {
        w_font_size_percent->setText(QString("%1 %").arg(v * 10));
        QFont f = font();
        qreal defaultFontSize = qApp->property("_bs_defaultFontSize").toReal();
        if (defaultFontSize <= 0)
            defaultFontSize = 10;
        f.setPointSizeF(defaultFontSize * qreal(v) / 10.);
        w_font_example->setFont(f);
    };

    connect(w_font_size, &QAbstractSlider::valueChanged,
            this, setFontSize);
    setFontSize(10);

    connect(w_font_size_reset, &QToolButton::clicked,
            this, [this]() { w_font_size->setValue(10); });

    auto setImageSize = [this](int v) {
        w_item_image_size_percent->setText(QString("%1 %").arg(v * 10));
        int s = int(BrickLink::core()->standardPictureSize().height() * v
                    / 10 / BrickLink::core()->itemImageScaleFactor());
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
    w_modifications_label->setAttribute(Qt::WA_MacSmallSize);

    w_docdir->insertItem(0, style()->standardIcon(QStyle::SP_DirIcon), QString());
    w_docdir->insertSeparator(1);
    w_docdir->insertItem(2, QIcon(), tr("Other..."));

    int is = fontMetrics().height();
    w_currency_update->setIconSize(QSize(is, is));

    w_ldraw_dir->insertItem(0, style()->standardIcon(QStyle::SP_DirIcon), QString());
    w_ldraw_dir->insertSeparator(1);
    w_ldraw_dir->insertItem(2, QIcon(), tr("Auto Detect"));
    w_ldraw_dir->insertItem(3, QIcon(), tr("Other..."));

    connect(w_ldraw_dir, QOverload<int>::of(&QComboBox::activated),
            this, &SettingsDialog::selectLDrawDir);
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
        QString newdir = QFileDialog::getExistingDirectory(this, tr("Document directory location"),
                                                           w_docdir->itemData(0).toString());
        if (!newdir.isNull()) {
            w_docdir->setItemData(0, QDir::toNativeSeparators(newdir));
            w_docdir->setItemText(0, systemDirName(newdir));
        }
    }
    w_docdir->setCurrentIndex(0);
}

void SettingsDialog::selectLDrawDir(int index)
{
    if (index == 2) {
        // auto detect
        w_ldraw_dir->setItemData(0, QString());
        w_ldraw_dir->setItemText(0, w_ldraw_dir->itemText(2));
    } if (index == 3) {
        QString newdir = QFileDialog::getExistingDirectory(this, tr("LDraw directory location"),
                                                           w_ldraw_dir->itemData(0).toString());
        if (!newdir.isNull()) {
            w_ldraw_dir->setItemData(0, QDir::toNativeSeparators(newdir));
            w_ldraw_dir->setItemText(0, systemDirName(newdir));
        }
    }
    w_ldraw_dir->setCurrentIndex(0);
    checkLDrawDir();
}

void SettingsDialog::resetUpdateIntervals()
{
    QMap<QByteArray, int> intervals = Config::inst()->updateIntervalsDefault();

    w_upd_picture->setValue(sec2day(intervals["Picture"]));
    w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
    w_upd_database->setValue(sec2day(intervals["Database"]));
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

    QVector<Config::Translation> translations = Config::inst()->translations();

    if (translations.isEmpty()) {
        w_language->setEnabled(false);
    }
    else {
        const QString currentLanguage = Config::inst()->language();

        for (const auto &trans : translations) {
            if (trans.language == QLatin1String("en"))
                w_language->addItem(trans.languageName[QLatin1String("en")], trans.language);
            else
                w_language->addItem(QString("%1 (%2)").arg(trans.languageName[QLatin1String("en")], trans.languageName[trans.language]), trans.language);

            if (currentLanguage == trans.language)
                w_language->setCurrentIndex(w_language->count()-1);
        }
    }

    w_metric->setChecked(Config::inst()->measurementSystem() == QLocale::MetricSystem);
    w_imperial->setChecked(Config::inst()->measurementSystem() == QLocale::ImperialSystem);

    w_partout->setCurrentIndex(int(Config::inst()->partOutMode()));
    w_modifications->setChecked(Config::inst()->visualChangesMarkModified());

    m_preferedCurrency = Config::inst()->defaultCurrencyCode();
    currenciesUpdated();

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

    // --[ BRICKLINK ]---------------------------------------------------------------

    QPair<QString, QString> blcred = Config::inst()->loginForBrickLink();

    w_bl_username->setText(blcred.first);
    w_bl_password->setText(blcred.second);

    // --[ LDRAW ]-------------------------------------------------------------------

    QString ldrawdir = QDir::toNativeSeparators(Config::inst()->ldrawDir());
    w_ldraw_dir->setItemData(0, ldrawdir.isEmpty() ? QString() : ldrawdir);
    w_ldraw_dir->setItemText(0, ldrawdir.isEmpty() ? w_ldraw_dir->itemText(2) : systemDirName(ldrawdir));
    checkLDrawDir();

    // ---------------------------------------------------------------------
}


void SettingsDialog::save()
{
    // --[ GENERAL ]-------------------------------------------------------------------

    if (w_language->currentIndex() >= 0)
        Config::inst()->setLanguage(w_language->itemData(w_language->currentIndex()).toString());
    Config::inst()->setMeasurementSystem(w_imperial->isChecked() ? QLocale::ImperialSystem : QLocale::MetricSystem);
    Config::inst()->setDefaultCurrencyCode(m_preferedCurrency);

    Config::inst()->setPartOutMode(Config::PartOutMode(w_partout->currentIndex()));
    Config::inst()->setVisualChangesMarkModified(w_modifications->isChecked());

    QDir dd(w_docdir->itemData(0).toString());

    if (dd.exists() && dd.isReadable())
        Config::inst()->setDocumentDir(dd.absolutePath());
    else
        MessageBox::warning(this, { }, tr("The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed."));

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

    Config::inst()->setUpdateIntervals(intervals);

    // --[ BRICKLINK ]-----------------------------------------------------------------

    Config::inst()->setLoginForBrickLink(w_bl_username->text(), w_bl_password->text());

    // --[ LDRAW ]---------------------------------------------------------------------

    const QString ldrawDir = w_ldraw_dir->itemData(0).toString();
    if (ldrawDir != Config::inst()->ldrawDir()) {
        MessageBox::information(nullptr, { }, tr("You have changed the LDraw directory. Please restart BrickStore to apply this setting."));
        Config::inst()->setLDrawDir(ldrawDir);
    }
}

void SettingsDialog::checkLDrawDir()
{
    QString path = w_ldraw_dir->itemData(0).toString();

    auto setStatus = [this](bool ok, const QString &status, const QString &path = { }) {
        w_ldraw_status->setText(QString::fromLatin1("%1<br><i>%2</i>").arg(status, path));
        auto icon = QIcon::fromTheme(ok ? "vcs-normal" : "vcs-removed");
        w_ldraw_status_icon->setPixmap(icon.pixmap(fontMetrics().height() * 3 / 2));
    };

    if (path.isEmpty()) {
        const auto ldrawDirs = LDraw::Core::potentialDrawDirs();
        QString ldrawDir;
        for (auto &ld : ldrawDirs) {
            if (LDraw::Core::isValidLDrawDir(ld)) {
                ldrawDir = ld;
                break;
            }
        }
        if (!ldrawDir.isEmpty())
            setStatus(true, tr("Auto-detected an LDraw installation at:"), ldrawDir);
        else
            setStatus(false, tr("No LDraw installation could be auto-detected."));
    } else {
        if (LDraw::Core::isValidLDrawDir(path))
            setStatus(true, tr("Valid LDraw installation at:"), path);
        else
            setStatus(false, tr("Not a valid LDraw installation."));
    }
}

#include "moc_settingsdialog.cpp"
