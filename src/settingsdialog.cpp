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
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QStringBuilder>

#include "settingsdialog.h"
#include "config.h"
#include "bricklink.h"
#include "bricklink_model.h"
#include "framework.h"
#include "ldraw.h"
#include "messagebox.h"
#include "utility.h"
#include "betteritemdelegate.h"


class ShortcutModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    ShortcutModel(const QList<QAction *> &actions, QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
        for (QAction *action : actions) {
            if (!action->property("bsAction").toBool())
                continue;
            if (action->menu())
                continue;
            if (action->text().isEmpty())
                continue;

            Entry entry = { action, action->property("bsShortcut").value<QKeySequence>(),
                            action->shortcut() };
            m_entries << entry;
        }
    }

    void apply()
    {
        QVariantMap saved;
        for (const Entry &entry : qAsConst(m_entries)) {
            if (entry.action && (entry.key != entry.defaultKey))
                saved.insert(entry.action->objectName(), entry.key);
        }
        Config::inst()->setShortcuts(saved);
    }

    int rowCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : m_entries.size();
    }

    int columnCount(const QModelIndex &parent = { }) const override
    {
        return parent.isValid() ? 0 : 3;
    }

    QVariant data(const QModelIndex &idx, int role) const override
    {
        if (!idx.isValid() || idx.parent().isValid() || (idx.row() >= m_entries.count())
                || (idx.column() >= columnCount())) {
            return { };
        }
        const Entry &entry = m_entries.at(idx.row());

        if (role == Qt::DisplayRole) {
            switch (idx.column()) {
            case 0: return entry.action->text();
            case 1: return entry.key.toString(QKeySequence::NativeText);
            case 2: return entry.defaultKey.toString(QKeySequence::NativeText);
            }
        }
        else if (role == Qt::ToolTipRole) {

            std::function<QStringList(QAction *)> buildPath = [&buildPath](QAction *a) {
                QStringList path;
                if (a && !a->text().isEmpty()) {
                    //path << a->text().remove(QRegularExpression(R"(\&[a-zA-Z0-9])"_l1));
                    path.prepend(a->text().remove(QRegularExpression(R"(\&(?!\&))"_l1)));

                    const QList<QWidget *> widgets = a->associatedWidgets();
                    for (auto w : widgets) {
                        if (auto *m = qobject_cast<QMenu *>(w))
                            path = buildPath(m->menuAction()) + path;
                    }
                }
                return path;
            };
            return buildPath(entry.action).join(" / "_l1);
        }
        return { };
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if ((section < 0) || (section >= columnCount()) || (orientation != Qt::Horizontal))
            return { };
        if (role == Qt::DisplayRole) {
            switch (section) {
            case 0: return tr("Action name");
            case 1: return tr("Current");
            case 2: return tr("Default");
            }
        }
        return { };
    }

    QKeySequence shortcut(const QModelIndex &idx) const
    {
        return (idx.isValid() && (idx.row() < m_entries.size()))
                ? m_entries.at(idx.row()).key : QKeySequence { };
    }

    QKeySequence defaultShortcut(const QModelIndex &idx) const
    {
        return (idx.isValid() && (idx.row() < m_entries.size()))
                ? m_entries.at(idx.row()).defaultKey : QKeySequence { };
    }

    bool setShortcut(const QModelIndex &idx, const QKeySequence &newShortcut)
    {
        if (idx.isValid() && (idx.row() < m_entries.size())) {
            int row = idx.row();
            if (m_entries.at(row).key == newShortcut)
                return true; // no change

            if (!newShortcut.isEmpty()) {
                for (const Entry &entry : qAsConst(m_entries)) {
                    if (entry.key == newShortcut)
                        return false; // duplicate
                }
            }

            m_entries[row].key = newShortcut;
            auto idx = index(row, 1);
            emit dataChanged(idx, idx);
            return true;
        }
        return false;
    }

    bool resetShortcut(const QModelIndex &idx)
    {
        if (idx.isValid() && (idx.row() < m_entries.size()))
            return setShortcut(idx, m_entries.at(idx.row()).defaultKey);
        return false;
    }

    void resetAllShortcuts()
    {
        for (auto &entry : m_entries)
            entry.key = entry.defaultKey;
        emit dataChanged(index(0, 1), index(rowCount() - 1, 1));
    }

private:
    struct Entry {
        QPointer<QAction> action;
        QKeySequence defaultKey;
        QKeySequence key;
    };
    QVector<Entry> m_entries;
};

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
        w_font_size_percent->setText(QString::number(v * 10) % u" %");
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
        w_item_image_size_percent->setText(QString::number(v * 10) % u" %");
        int s = int(BrickLink::core()->standardPictureSize().height() * v
                    / 10 / BrickLink::core()->itemImageScaleFactor());
        QImage img(":/images/brickstore_icon.png"_l1);
        img = img.scaled(s, s, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(w_currency, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
#else
    connect(w_currency, &QComboBox::currentTextChanged,
#endif
            this, &SettingsDialog::currentCurrencyChanged);
    connect(w_currency_update, &QAbstractButton::clicked,
            Currency::inst(), &Currency::updateRates);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &SettingsDialog::currenciesUpdated);

    w_bl_username_note->hide();
    connect(w_bl_username, &QLineEdit::textChanged,
            this, [this](const QString &s) {
        bool b = s.contains(QLatin1Char('@'));
        if (w_bl_username_note->isVisible() != b) {
            w_bl_username_note->setVisible(b);
            QPalette pal = QApplication::palette("QLineEdit");
            if (b) {
                pal.setColor(QPalette::Base,
                             Utility::gradientColor(pal.color(QPalette::Base), Qt::red, 0.25));
            }
            w_bl_username->setPalette(pal);
        }
    });

    m_sc_model = new ShortcutModel(FrameWork::inst()->allActions(), this);
    m_sc_proxymodel = new QSortFilterProxyModel(this);
    m_sc_proxymodel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sc_proxymodel->setFilterKeyColumn(-1);
    m_sc_proxymodel->setSourceModel(m_sc_model);
    w_sc_list->setModel(m_sc_proxymodel);
    w_sc_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    w_sc_list->setItemDelegate(new BetterItemDelegate(BetterItemDelegate::AlwaysShowSelection, this));
    w_sc_filter->addAction(QIcon::fromTheme("view-filter"_l1), QLineEdit::LeadingPosition);

    connect(w_sc_list->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [=]() {
        QModelIndex idx = m_sc_proxymodel->mapToSource(w_sc_list->currentIndex());
        w_sc_edit->setEnabled(idx.isValid());
        w_sc_reset->setEnabled(idx.isValid());
        if (idx.isValid()) {
            auto shortcut = m_sc_model->shortcut(idx);
            auto defaultShortcut = m_sc_model->defaultShortcut(idx);
            w_sc_edit->setKeySequence(shortcut);
            w_sc_reset->setEnabled(shortcut != defaultShortcut);
        }
    });
    connect(m_sc_model, &QAbstractItemModel::dataChanged,
            this, [=](const QModelIndex &tl, const QModelIndex &br) {
        QModelIndex idx = m_sc_proxymodel->mapToSource(w_sc_list->currentIndex());
        if (idx.isValid() && QItemSelection(tl.siblingAtColumn(0), br.siblingAtColumn(0))
                .contains(idx.siblingAtColumn(0))) {
            auto shortcut = m_sc_model->shortcut(idx);
            auto defaultShortcut = m_sc_model->defaultShortcut(idx);
            w_sc_edit->setKeySequence(shortcut);
            w_sc_reset->setEnabled(shortcut != defaultShortcut);
        }
    });
    connect(w_sc_edit, &QKeySequenceEdit::editingFinished,
            this, [=]() {
        auto newShortcut = w_sc_edit->keySequence();
        // disallow Alt only shortcuts, because this interferes with standard menu handling
        for (int i = 0; i < newShortcut.count(); ++i) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            auto mod = (newShortcut[i] & Qt::KeyboardModifierMask);
#else
            auto mod = newShortcut[i].keyboardModifiers();
#endif
            if ((mod & Qt::AltModifier) && !(mod & Qt::ControlModifier)) {
                MessageBox::warning(this, { }, tr("Shortcuts with 'Alt' need to also include 'Control' in order to not interfere with the menu system."));
                return;
            }
        }

        if (!m_sc_model->setShortcut(m_sc_proxymodel->mapToSource(w_sc_list->currentIndex()),
                                     w_sc_edit->keySequence())) {
            MessageBox::warning(this, { }, tr("This shortcut is already used by another action."));
        }
    });
    connect(w_sc_reset, &QPushButton::clicked, this, [=]() {
        if (!m_sc_model->resetShortcut(m_sc_proxymodel->mapToSource(w_sc_list->currentIndex()))) {
            MessageBox::warning(this, { }, tr("This shortcut is already used by another action."));
        }
    });
    connect(w_sc_reset_all, &QPushButton::clicked, this, [=]() {
        m_sc_model->resetAllShortcuts();
    });
    connect(w_sc_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sc_proxymodel->setFilterFixedString(text);
    });

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

void SettingsDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    resize(sizeHint());
}

void SettingsDialog::currenciesUpdated()
{
    QString oldprefered = m_preferedCurrency;
    QStringList currencies = Currency::inst()->currencyCodes();
    currencies.sort();
    currencies.removeOne("USD"_l1);
    currencies.prepend("USD"_l1);
    w_currency->clear();
    w_currency->insertItems(0, currencies);
    if (currencies.count() > 1)
        w_currency->insertSeparator(1);
    w_currency->setCurrentIndex(qMax(0, w_currency->findText(oldprefered)));

//    currentCurrencyChanged(w_currency->currentText());
}

void SettingsDialog::load()
{
    // --[ GENERAL ]---------------------------------------------------

    QVector<Config::Translation> translations = Config::inst()->translations();

    if (translations.isEmpty()) {
        w_language->setEnabled(false);
    }
    else {
        const QString currentLanguage = Config::inst()->language();

        for (const auto &trans : translations) {
            if (trans.language == "en"_l1) {
                w_language->addItem(trans.languageName["en"_l1], trans.language);
            } else {
                QString s = trans.languageName["en"_l1] % u" ("
                               % trans.languageName[trans.language] % u')';
                w_language->addItem(s, trans.language);
            }

            if (currentLanguage == trans.language)
                w_language->setCurrentIndex(w_language->count()-1);
        }
    }

    w_metric->setChecked(Config::inst()->measurementSystem() == QLocale::MetricSystem);
    w_imperial->setChecked(Config::inst()->measurementSystem() == QLocale::ImperialSystem);

    w_partout->setCurrentIndex(int(Config::inst()->partOutMode()));
    w_openbrowser->setChecked(Config::inst()->openBrowserOnExport());
    w_restore_session->setChecked(Config::inst()->restoreLastSession());
    w_modifications->setChecked(Config::inst()->visualChangesMarkModified());

    m_preferedCurrency = Config::inst()->defaultCurrencyCode();
    currenciesUpdated();

    QString docdir = QDir::toNativeSeparators(Config::inst()->documentDir());
    w_docdir->setItemData(0, docdir);
    w_docdir->setItemText(0, systemDirName(docdir));

#if !defined(SENTRY_ENABLED)
    w_crash_reports->setEnabled(false);
#else
    w_crash_reports->setChecked(Config::inst()->sentryConsent() == Config::SentryConsent::Given);
#endif

    // --[ INTERFACE ]-------------------------------------------------

    int iconSizeIndex = 0;
    auto iconSize = Config::inst()->iconSize();
    if (iconSize == QSize(22, 22))
        iconSizeIndex = 1;
    else if (iconSize == QSize(32, 32))
        iconSizeIndex = 2;

    w_icon_size->setCurrentIndex(iconSizeIndex);
    w_font_size->setValue(Config::inst()->fontSizePercent() / 10);

    w_item_image_size->setValue(Config::inst()->itemImageSizePercent() / 10);

    // --[ UPDATES ]---------------------------------------------------

    QMap<QByteArray, int> intervals = Config::inst()->updateIntervals();

    w_upd_picture->setValue(sec2day(intervals["Picture"]));
    w_upd_priceguide->setValue(sec2day(intervals["PriceGuide"]));
    w_upd_database->setValue(sec2day(intervals["Database"]));

    // --[ BRICKLINK ]-------------------------------------------------

    QPair<QString, QString> blcred = Config::inst()->brickLinkCredentials();

    w_bl_username->setText(blcred.first);
    w_bl_password->setText(blcred.second);

    // --[ LDRAW ]-----------------------------------------------------

    QString ldrawdir = QDir::toNativeSeparators(Config::inst()->ldrawDir());
    w_ldraw_dir->setItemData(0, ldrawdir.isEmpty() ? QString() : ldrawdir);
    w_ldraw_dir->setItemText(0, ldrawdir.isEmpty() ? w_ldraw_dir->itemText(2) : systemDirName(ldrawdir));
    checkLDrawDir();

    // --[ SHORTCUTS ]-------------------------------------------------

    w_sc_list->sortByColumn(0, Qt::AscendingOrder);
}


void SettingsDialog::save()
{
    // --[ GENERAL ]-------------------------------------------------------------------

    if (w_language->currentIndex() >= 0)
        Config::inst()->setLanguage(w_language->itemData(w_language->currentIndex()).toString());
    Config::inst()->setMeasurementSystem(w_imperial->isChecked() ? QLocale::ImperialSystem : QLocale::MetricSystem);
    Config::inst()->setDefaultCurrencyCode(m_preferedCurrency);

    Config::inst()->setPartOutMode(Config::PartOutMode(w_partout->currentIndex()));
    Config::inst()->setOpenBrowserOnExport(w_openbrowser->isChecked());
    Config::inst()->setRestoreLastSession(w_restore_session->isChecked());
    Config::inst()->setVisualChangesMarkModified(w_modifications->isChecked());

    QDir dd(w_docdir->itemData(0).toString());

    if (dd.exists() && dd.isReadable())
        Config::inst()->setDocumentDir(dd.absolutePath());
    else
        MessageBox::warning(this, { }, tr("The specified document directory does not exist or is not read- and writeable.<br />The document directory setting will not be changed."));

    Config::inst()->setSentryConsent(w_crash_reports->isChecked() ? Config::SentryConsent::Given
                                                                  : Config::SentryConsent::Revoked);

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

    Config::inst()->setBrickLinkCredentials(w_bl_username->text(), w_bl_password->text());

    // --[ LDRAW ]---------------------------------------------------------------------

    const QString ldrawDir = w_ldraw_dir->itemData(0).toString();
    if (ldrawDir != Config::inst()->ldrawDir()) {
        MessageBox::information(nullptr, { }, tr("You have changed the LDraw directory. Please restart BrickStore to apply this setting."));
        Config::inst()->setLDrawDir(ldrawDir);
    }

    // --[ SHORTCUTS ]-----------------------------------------------------------------

    m_sc_model->apply();
}

void SettingsDialog::checkLDrawDir()
{
    QString path = w_ldraw_dir->itemData(0).toString();

    auto setStatus = [this](bool ok, const QString &status, const QString &path = { }) {
        w_ldraw_status->setText(QString::fromLatin1("%1<br><i>%2</i>").arg(status, path));
        auto icon = QIcon::fromTheme(ok ? "vcs-normal"_l1 : "vcs-removed"_l1);
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
#include "settingsdialog.moc"
