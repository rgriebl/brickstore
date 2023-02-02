/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <chrono>

#include <QLineEdit>
#include <QLayout>
#include <QLabel>
#include <QStyle>
#include <QButtonGroup>
#include <QRadioButton>
#include <QValidator>
#include <QTextEdit>
#include <QPushButton>
#include <QCursor>
#include <QCheckBox>
#include <QToolButton>
#include <QWheelEvent>
#include <QShortcut>
#include <QToolTip>
#include <QTimer>
#include <QAction>
#include <QMenu>
#include <QStringListModel>

#include "bricklink/core.h"
#include "bricklink/picture.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/currency.h"
#include "common/eventfilter.h"
#include "common/humanreadabletimedelta.h"
#include "utility/utility.h"
#include "additemdialog.h"
#include "inventorywidget.h"
#include "desktopuihelpers.h"
#include "picturewidget.h"
#include "priceguidewidget.h"
#include "selectcolor.h"
#include "selectitem.h"
#include "view.h"


using namespace std::chrono_literals;


class ValidatorSpinBox : public QAbstractSpinBox
{
public:
    static void setValidator(QAbstractSpinBox *spinbox, const QValidator *validator)
    {
        // why is lineEdit() protected?
        (spinbox->*(&ValidatorSpinBox::lineEdit))()->setValidator(validator);
    }
};


AddItemDialog::AddItemDialog(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_favoriteFilters(new QStringListModel(this))
{
    setupUi(this);

    m_view = nullptr;
    m_caption_fmt        = windowTitle();

    m_tier_type = new QButtonGroup(this);
    m_tier_type->setExclusive(true);
    m_tier_type->addButton(w_radio_percent, 0);
    m_tier_type->addButton(w_radio_currency, 1);

    m_condition = new QButtonGroup(this);
    m_condition->setExclusive(true);
    m_condition->addButton(w_radio_new, int(BrickLink::Condition::New));
    m_condition->addButton(w_radio_used, int(BrickLink::Condition::Used));

    w_select_item->setFilterFavoritesModel(m_favoriteFilters);
    w_select_color->setWidthToContents(true);

    w_add = new QPushButton();
    w_add->setDefault(true);
    w_buttons->addButton(w_add, QDialogButtonBox::ActionRole);

    w_picture->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_picture->setLineWidth(2);

    w_price_guide->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_price_guide->setLineWidth(2);

    w_inventory->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_inventory->setLineWidth(2);

    m_invGoToAction = new QAction(this);
    connect(m_invGoToAction, &QAction::triggered, this, [this]() {
        const auto selected = w_inventory->selected();
        if (selected.m_item)
            goToItem(selected.m_item, selected.m_color);
    });
    w_inventory->addAction(m_invGoToAction);

    w_qty->setRange(1, DocumentModel::maxQuantity);
    w_qty->setValue(1);
    new EventFilter(w_qty, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

    w_bulk->setRange(1, DocumentModel::maxQuantity);
    w_bulk->setValue(1);
    new EventFilter(w_bulk, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

    double maxPrice = DocumentModel::maxLocalPrice(m_currency_code);

    w_price->setRange(0, maxPrice);
    new EventFilter(w_price, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);
    w_cost->setRange(0, maxPrice);
    new EventFilter(w_cost, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

    w_tier_qty[0] = w_tier_qty_0;
    w_tier_qty[1] = w_tier_qty_1;
    w_tier_qty[2] = w_tier_qty_2;
    w_tier_price[0] = w_tier_price_0;
    w_tier_price[1] = w_tier_price_1;
    w_tier_price[2] = w_tier_price_2;

    for (int i = 0; i < 3; i++) {
        w_tier_qty[i]->setRange(0, DocumentModel::maxQuantity);
        w_tier_price[i]->setRange(0, maxPrice);
        w_tier_qty[i]->setValue(0);
        w_tier_price[i]->setValue(0);
        new EventFilter(w_tier_qty[i], { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);
        new EventFilter(w_tier_price[i], { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

        connect(w_tier_qty[i], QOverload<int>::of(&QSpinBox::valueChanged),
                this, &AddItemDialog::checkTieredPrices);
        connect(w_tier_price[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &AddItemDialog::checkTieredPrices);
    }

    connect(w_select_item, &SelectItem::hasColors,
            this, [this](bool b) {
        w_select_color->setEnabled(b);
        if (!b) {
            w_select_color->setCurrentColor(BrickLink::core()->color(0));
            w_select_color->setColorLock(false);
        }
    });
    connect(w_select_item, &SelectItem::hasSubConditions,
            w_subcondition, &QWidget::setEnabled);
    connect(w_select_item, &SelectItem::itemSelected,
            this, [this](const BrickLink::Item *item, bool confirmed) {
        updateItemAndColor();
        w_select_color->setCurrentColorAndItem(w_select_color->currentColor(), item);
        recordBrowseEntry();
        if (confirmed)
            w_add->animateClick();
    });
    connect(w_select_item, &SelectItem::showInColor,
            this, [this](const BrickLink::Color *color) {
        w_select_color->setCurrentColor(color);
    });

    connect(w_select_color, &SelectColor::colorSelected,
            this, [this](const BrickLink::Color *, bool confirmed) {
        updateItemAndColor();
        recordBrowseEntry();
        if (confirmed)
            w_add->animateClick();
    });
    connect(w_select_color, &SelectColor::colorLockChanged,
            w_select_item, &SelectItem::setColorFilter);

    connect(w_price, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AddItemDialog::checkAddPossible);
    connect(w_qty, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddItemDialog::checkAddPossible);
    connect(m_tier_type, &QButtonGroup::idClicked,
            this, &AddItemDialog::setTierType);
    connect(w_bulk, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddItemDialog::checkAddPossible);

    connect(w_add, &QAbstractButton::clicked,
            this, &AddItemDialog::addClicked);

    connect(w_price_guide, &PriceGuideWidget::priceDoubleClicked,
            this, [this](double p) {
        p *= Currency::inst()->rate(m_currency_code);
        w_price->setValue(p);
        checkAddPossible();
    });

    w_radio_percent->click();

    m_backMenu = new QMenu(this);
    m_nextMenu = new QMenu(this);
    connect(m_backMenu, &QMenu::aboutToShow, this, [this]() {
        buildBrowseMenu(BrowseMenuType::Back);
    });
    connect(m_nextMenu, &QMenu::aboutToShow, this, [this]() {
        buildBrowseMenu(BrowseMenuType::Next);
    });
    connect(w_go_back, &QToolButton::clicked, this, [this]() {
        replayBrowseEntry(BrowseMenuType::Back, m_browseStackIndex + 1);
    });
    connect(w_go_next, &QToolButton::clicked, this, [this]() {
        replayBrowseEntry(BrowseMenuType::Next, m_browseStackIndex - 1);
    });
    connect(m_backMenu, &QMenu::triggered, this, [this](QAction *a) {
        replayBrowseEntry(BrowseMenuType::Back, a->data().toInt());
    });
    connect(m_nextMenu, &QMenu::triggered, this, [this](QAction *a) {
        replayBrowseEntry(BrowseMenuType::Next, a->data().toInt());
    });

    w_go_back->setMenu(m_backMenu);
    w_go_back->setProperty("noMenuArrow", true);
    w_go_next->setMenu(m_nextMenu);
    w_go_next->setProperty("noMenuArrow", true);

    updateBrowseActions();

    auto *menu = new QMenu(this);
    m_toggles[0] = menu->addAction(QIcon::fromTheme(u"help-about"_qs), tr("Show item information"));
    m_toggles[1] = menu->addAction(QIcon::fromTheme(u"go-jump-definition"_qs), tr("Show item inventory and appearance"));
    m_toggles[2] = menu->addAction(QIcon::fromTheme(u"taxes-finances"_qs), tr("Show price guide"));

    for (int i = 0; i < 3; ++i) {
        m_toggles[i]->setCheckable(true);
        m_toggles[i]->setChecked(true);
        connect(m_toggles[i], &QAction::toggled, this, [this, i](bool on) {
            w_splitter_bottom->widget(i)->setVisible(on);
        });
    }
    menu->addSeparator();

    m_sellerMode = menu->addAction(QIcon::fromTheme(u"insert-more-mark"_qs), tr("Show additional input fields for sellers"));
    m_sellerMode->setCheckable(true);
    m_sellerMode->setChecked(true);
    connect(m_sellerMode, &QAction::toggled,
            this, &AddItemDialog::setSellerMode);

    menu->addSeparator();

    m_historyMenu = menu->addMenu(tr("Browsing history"));
    m_historyMenu->setProperty("scrollableMenu", true);

    connect(m_historyMenu, &QMenu::aboutToShow, this, [this]() {
        buildBrowseMenu(BrowseMenuType::History);
    });
    connect(m_historyMenu, &QMenu::triggered, this, [this](QAction *a) {
        replayBrowseEntry(BrowseMenuType::History, a->data().toInt());
    });

    w_menu->setMenu(menu);
    w_menu->setProperty("noMenuArrow", true);

    w_splitter_bottom->setStretchFactor(0, 2);
    w_splitter_bottom->setStretchFactor(1, 1);
    w_splitter_bottom->setStretchFactor(2, 0);
    w_splitter_bottom->setStretchFactor(3, 1);

    checkTieredPrices();

    QByteArray ba = Config::inst()->value(u"MainWindow/AddItemDialog/Geometry"_qs)
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/VSplitter"_qs)
            .toByteArray();
    if (!ba.isEmpty())
        w_splitter_vertical->restoreState(ba);
    ba = Config::inst()->value(u"MainWindow/AddItemDialog/HSplitter"_qs)
            .toByteArray();
    if (!ba.isEmpty()) {
        char hidden = ba.at(0);
        for (int i = 0; i < 3; ++i) {
            if (hidden & (1 << i)) {
                w_splitter_bottom->widget(i)->hide();
                m_toggles[i]->setChecked(false);
            }
        }
        if (hidden & (1 << 7)) {
            m_sellerMode->setChecked(false);
            setSellerMode(false);
        }

        w_splitter_bottom->restoreState(ba.mid(1));
    }

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/SelectItem"_qs).toByteArray();
    if (!w_select_item->restoreState(ba))
        w_select_item->restoreState(SelectItem::defaultState());

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/SelectColor"_qs).toByteArray();
    if (!w_select_color->restoreState(ba))
        w_select_color->restoreState(SelectColor::defaultState());

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/Inventory"_qs).toByteArray();
    w_inventory->restoreState(ba);

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/ItemDetails"_qs).toByteArray();
    restoreState(ba);

    ba = Config::inst()->value(u"MainWindow/AddItemDialog/BrowseState"_qs).toByteArray();
    restoreBrowseState(ba);

    m_favoriteFilters->setStringList(Config::inst()->value(u"MainWindow/AddItemDialog/Filter"_qs).toStringList());

    new EventFilter(w_last_added, { QEvent::ToolTip }, [this](QObject *, QEvent *e) { // dynamic tooltip
        const auto *he = static_cast<QHelpEvent *>(e);
        if (m_addHistory.size() > 1) {
            static const QString pre = u"<p style='white-space:pre'>"_qs;
            static const QString post = u"</p>"_qs;
            QString tips;

            for (const auto &entry : m_addHistory)
                tips = tips + pre + addhistoryTextFor(entry.first, entry.second) + post;

            QToolTip::showText(he->globalPos(), tips, w_last_added, w_last_added->geometry());
        }
        return EventFilter::StopEventProcessing;
    });

    m_historyTimer = new QTimer(this);
    m_historyTimer->setInterval(30s);
    connect(m_historyTimer, &QTimer::timeout, this, &AddItemDialog::updateAddHistoryText);


    if (QAction *a = ActionManager::inst()->qAction("bricklink_catalog")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item)
                Application::openUrl(BrickLink::Core::urlForCatalogInfo(item, color));
        });
    }
    if (QAction *a = ActionManager::inst()->qAction("bricklink_priceguide")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item && (color || !item->itemType()->hasColors()))
                Application::openUrl(BrickLink::Core::urlForPriceGuideInfo(item, color));
        });
    }
    if (QAction *a = ActionManager::inst()->qAction("bricklink_lotsforsale")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item && (color || !item->itemType()->hasColors()))
                Application::openUrl(BrickLink::Core::urlForLotsForSale(item, color));
        });
    }

    auto listMinHeight = QFontMetrics(font()).height();
    w_select_item->setMinimumHeight(listMinHeight);
    w_select_color->setMinimumHeight(listMinHeight);

    languageChange();
}

void AddItemDialog::languageChange()
{
    Ui::AddItemDialog::retranslateUi(this);
    m_price_label_fmt = w_label_currency->text();

    w_add->setText(tr("Add"));
    m_invGoToAction->setText(tr("Go to Item"));

    updateCurrencyCode();
    updateCaption();
    updateAddHistoryText();
}

AddItemDialog::~AddItemDialog()
{
    Config::inst()->setValue(u"MainWindow/AddItemDialog/Filter"_qs, m_favoriteFilters->stringList());

    Config::inst()->setValue(u"MainWindow/AddItemDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"MainWindow/AddItemDialog/VSplitter"_qs, w_splitter_vertical->saveState());

    QByteArray ba = w_splitter_bottom->saveState();
    char hidden = 0;
    for (int i = 0; i < 3; ++i) {
        if (!m_toggles[i]->isChecked())
            hidden |= (1 << i);
    }
    if (!m_sellerMode->isChecked())
        hidden |= (1 << 7);
    ba.prepend(hidden);
    Config::inst()->setValue(u"MainWindow/AddItemDialog/HSplitter"_qs, ba);
    Config::inst()->setValue(u"MainWindow/AddItemDialog/SelectItem"_qs, w_select_item->saveState());
    Config::inst()->setValue(u"MainWindow/AddItemDialog/SelectColor"_qs, w_select_color->saveState());
    Config::inst()->setValue(u"MainWindow/AddItemDialog/Inventory"_qs, w_inventory->saveState());
    Config::inst()->setValue(u"MainWindow/AddItemDialog/ItemDetails"_qs, saveState());
    Config::inst()->setValue(u"MainWindow/AddItemDialog/BrowseState"_qs, saveBrowseState());

    w_picture->setItemAndColor(nullptr);
    w_price_guide->setPriceGuide(nullptr);
    w_inventory->setItem(nullptr, nullptr);
}

void AddItemDialog::updateCaption()
{
    setWindowTitle(m_caption_fmt.arg(m_view ? m_view->document()->filePathOrTitle()
                                              : QString { }));
}

void AddItemDialog::updateCurrencyCode()
{
    m_currency_code = m_view ? m_view->model()->currencyCode() : u"USD"_qs;

    w_price_guide->setCurrencyCode(m_currency_code);

    w_label_currency->setText(m_price_label_fmt.arg(m_currency_code));
    w_radio_currency->setText(m_currency_code);
}

void AddItemDialog::attach(View *view)
{
    if (m_view) {
        disconnect(m_view->document(), &Document::titleChanged,
                   this, &AddItemDialog::updateCaption);
        disconnect(m_view->model(), &DocumentModel::currencyCodeChanged,
                   this, &AddItemDialog::updateCurrencyCode);
    }
    m_view = view;
    if (m_view) {
        connect(m_view->document(), &Document::titleChanged,
                this, &AddItemDialog::updateCaption);
        connect(m_view->model(), &DocumentModel::currencyCodeChanged,
                this, &AddItemDialog::updateCurrencyCode);
    }
    setEnabled(m_view);

    updateCaption();
    updateCurrencyCode();
}

void AddItemDialog::goToItem(const BrickLink::Item *item, const BrickLink::Color *color)
{
    if (item) {
        w_select_item->clearFilter();
        w_select_item->setCurrentItem(item, true);
        if (color)
            w_select_color->setCurrentColorAndItem(color, item);
    }
}

void AddItemDialog::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);

    if (e->isAccepted()) {
        hide();
        emit closed();
    }
}

void AddItemDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}

void AddItemDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        close();
        return;
    } else if ((!e->modifiers() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
               || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter))) {
        if (w_add->isVisible() && w_add->isEnabled())
            w_add->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
}

void AddItemDialog::setSellerMode(bool b)
{
    QWidget *wl [] = {
        w_bulk,
        w_bulk_label,
        w_tier_label,
        w_radio_percent,
        w_radio_currency,
        w_tier_price_0,
        w_tier_price_1,
        w_tier_price_2,
        w_tier_qty_0,
        w_tier_qty_1,
        w_tier_qty_2,
        w_comments,
        w_comments_label,

        nullptr
    };

    for (QWidget **wpp = wl; *wpp; wpp++) {
        if (b)
            (*wpp)->show();
        else
            (*wpp)->hide();
    }

    if (!b) {
        w_bulk->setValue(1);
        w_tier_qty[0]->setValue(0);
        w_comments->setText(QString());
        checkTieredPrices();
    }
}

void AddItemDialog::updateItemAndColor()
{
    auto item = w_select_item->currentItem();
    auto color = w_select_color->currentColor();

    w_picture->setItemAndColor(item, color);

    if (item && color) {
        w_price_guide->setPriceGuide(BrickLink::core()->priceGuideCache()->priceGuide(item, color, true));
        w_inventory->setItem(item, color);
    }
    else {
        w_price_guide->setPriceGuide(nullptr);
        w_inventory->setItem(nullptr, nullptr);
    }
    checkAddPossible();
}

void AddItemDialog::setTierType(int type)
{
    (type == 0 ? w_radio_percent : w_radio_currency)->setChecked(true);

    for (auto *tp : std::as_const(w_tier_price)) {
        if (type == 0) {
            tp->setRange(0, 99);
            tp->setDecimals(0);
        } else {
            tp->setRange(0, DocumentModel::maxLocalPrice(m_currency_code));
            tp->setDecimals(3);
        }
        tp->setValue(0);
    }
}

void AddItemDialog::checkTieredPrices()
{
    bool valid = true;

    for (int i = 0; i < 3; i++) {
        w_tier_qty [i]->setEnabled(valid);

        valid = valid && (w_tier_qty [i]->text().toInt() > 0);

        w_tier_price [i]->setEnabled(valid);
    }
    checkAddPossible();
}

double AddItemDialog::tierPriceValue(int i)
{
    if ((i < 0) || (i > 2))
        return 0.;

    double val;

    if (m_tier_type->checkedId() == 0)     // %
        val = Currency::fromString(w_price->text()) * (100 - w_tier_price [i]->text().toInt()) / 100;
    else // $
        val = Currency::fromString(w_tier_price [i]->text());

    return val;
}

void AddItemDialog::updateAddHistoryText()
{
    if (m_addHistory.isEmpty()) {
        w_last_added->setText(tr("Your recently added items will be listed here"));
        m_historyTimer->stop();
    } else {
        const auto &hist = *m_addHistory.crbegin();
        w_last_added->setText(addhistoryTextFor(hist.first, hist.second));
        m_historyTimer->start();
    }
}

QString AddItemDialog::addhistoryTextFor(const QDateTime &when, const BrickLink::Lot &lot)
{
    auto now = QDateTime::currentDateTime();
    QString cs;
    if (lot.color() && lot.color()->id()) {
        QColor color = lot.color()->color();
        cs = uR"(<b><font color=")" + Utility::textColor(color).name() %
                uR"(" style="background-color: )" + color.name() + uR"(;">&nbsp;)" %
                lot.colorName() + uR"(&nbsp;</font></b>&nbsp;&nbsp;)";
    }

    QString s = tr("Added %1").arg(HumanReadableTimeDelta::toString(now, when)) %
            u":&nbsp;&nbsp;<b>" + QString::number(lot.quantity()) + u"</b>&nbsp;&nbsp;" + cs %
            lot.itemName() + u" <i>[" + QLatin1String(lot.itemId()) + u"]</i>";

    return s;
}

QByteArray AddItemDialog::saveState() const
{
    bool tierCurrency = w_radio_currency->isChecked();

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("AI") << qint32(3)
       << w_qty->value()
       << w_bulk->value()
       << Currency::fromString(w_price->text())
       << Currency::fromString(w_cost->text())
       << tierCurrency;

    for (int i = 0; i < 3; ++i) {
        const QString s = w_tier_price[i]->text();
        ds << w_tier_qty[i]->value() << (tierCurrency ? Currency::fromString(s) : s.toDouble());
    }

    ds << w_radio_new->isChecked()
       << w_subcondition->currentIndex()
       << w_comments->text()
       << w_remarks->text()
       << w_merge->isChecked();

    ds << w_picture->prefer3D();
    return ba;
}

bool AddItemDialog::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "AI") || (version < 2) || (version > 3))
        return false;

    int qty;
    int bulk;
    double price;
    double cost;
    int tierQty[3];
    double tierPrice[3];
    bool tierCurrency;
    bool isNew;
    int subConditionIndex;
    QString comments;
    QString remarks;
    bool consolidate;
    bool prefer3D = true;

    ds >> qty >> bulk >> price >> cost >> tierCurrency;
    for (int i = 0; i < 3; ++i)
        ds >> tierQty[i] >> tierPrice[i];
    ds >> isNew >> subConditionIndex >> comments >> remarks >> consolidate;
    if (version >= 3)
        ds >> prefer3D;

    if (ds.status() != QDataStream::Ok)
        return false;

    w_qty->setValue(qty);
    w_bulk->setValue(bulk);
    w_price->setValue(price);
    w_cost->setValue(cost);
    setTierType(tierCurrency ? 1 : 0);
    for (int i = 0; i < 3; ++i) {
        w_tier_qty[i]->setValue(tierQty[i]);
        w_tier_price[i]->setValue(tierPrice[i]);
    }
    w_radio_new->setChecked(isNew);
    w_radio_used->setChecked(!isNew);
    w_subcondition->setCurrentIndex(subConditionIndex);
    w_comments->setText(comments);
    w_remarks->setText(remarks);
    w_merge->setChecked(consolidate);
    checkAddPossible();

    w_picture->setPrefer3D(prefer3D);

    return true;
}

bool AddItemDialog::checkAddPossible()
{
    bool priceOk = w_price->hasAcceptableInput();
    bool qtyOk = w_qty->hasAcceptableInput();
    bool bulkOk = w_bulk->hasAcceptableInput();
    w_price->setProperty("showInputError", !priceOk);
    w_qty->setProperty("showInputError", !qtyOk);
    w_bulk->setProperty("showInputError", !bulkOk);

    bool acceptable = w_select_item->currentItem() && priceOk && qtyOk && bulkOk;

    if (auto currentType = w_select_item->currentItemType()) {
        if (currentType->hasColors())
            acceptable = acceptable && (w_select_color->currentColor());
    }

    for (int i = 0; i < 3; i++) {
        bool tierEnabled = w_tier_price [i]->isEnabled();

        bool tqtyOk = w_tier_qty[i]->hasAcceptableInput();
        bool tpriceOk = w_tier_price[i]->hasAcceptableInput();
        bool tpriceLower = true;
        bool tqtyHigher = true;

        if (i > 0) {
            tpriceLower = (tierPriceValue(i - 1) > tierPriceValue(i));
            tqtyHigher = (w_tier_qty[i - 1]->text().toInt() < w_tier_qty[i]->text().toInt());
        } else {
            tpriceLower = (Currency::fromString(w_price->text()) > tierPriceValue(i));
        }
        w_tier_price[i]->setProperty("showInputError", tierEnabled && (!tpriceOk || !tpriceLower));
        w_tier_qty[i]->setProperty("showInputError", tierEnabled && (!tqtyOk || !tqtyHigher));

        if (tierEnabled)
            acceptable = acceptable && tpriceOk && tqtyOk && tpriceLower && tqtyHigher;
    }

    w_add->setEnabled(acceptable);
    return acceptable;
}

void AddItemDialog::addClicked()
{
    if (!checkAddPossible() || !m_view)
        return;

    const BrickLink::Item *item = w_select_item->currentItem();
    const BrickLink::Color *color;
    if (item && item->itemType() && item->itemType()->hasColors())
        color = w_select_color->currentColor();
    else
        color = BrickLink::core()->color(0);

    auto lot = new BrickLink::Lot(item, color);

    lot->setQuantity(w_qty->text().toInt());
    lot->setPrice(Currency::fromString(w_price->text()));
    lot->setCost(Currency::fromString(w_cost->text()));
    lot->setBulkQuantity(w_bulk->text().toInt());
    lot->setCondition(static_cast <BrickLink::Condition>(m_condition->checkedId()));
    if (lot->itemType() && lot->itemType()->hasSubConditions())
        lot->setSubCondition(static_cast<BrickLink::SubCondition>(1 + w_subcondition->currentIndex()));
    lot->setRemarks(w_remarks->text());
    lot->setComments(w_comments->text());

    for (int i = 0; i < 3; i++) {
        if (!w_tier_price [i]->isEnabled())
            break;

        lot->setTierQuantity(i, w_tier_qty [i]->text().toInt());
        lot->setTierPrice(i, tierPriceValue(i));
    }

    m_addHistory.emplace_back(QDateTime::currentDateTime(), *lot);
    while (m_addHistory.size() > 6)
        m_addHistory.pop_front();
    updateAddHistoryText();

    m_view->model()->addLots({ lot }, w_merge->isChecked() ? DocumentModel::AddLotMode::ConsolidateWithExisting
                                                           : DocumentModel::AddLotMode::AddAsNew)
            .then([view = m_view](int lastAddedRow) {
        if (lastAddedRow >= 0) {
            view->setLatestRow(lastAddedRow);
        }
    });
}

void AddItemDialog::recordBrowseEntry(bool onlyUpdateHistory)
{
    if (m_currentlyRestoringBrowseHistory)
        return;

    auto item = w_select_item->currentItem();
    auto color = w_select_color->currentColor();

    if (!item)
        return;

    QByteArray fullItemId = item->itemTypeId() + item->id();

    BrowseHistoryEntry he { fullItemId, color ? color->id() : BrickLink::Color::InvalidId,
                QDateTime::currentDateTime(), w_select_item->saveState(),
                w_select_color->saveState(), saveState() };

    if (!onlyUpdateHistory) {
        // Add, if not already the first item. If the first item is the same, but without color,
        // then replace the uncolored entry
        if (m_browseStack.isEmpty() || (m_browseStack.at(m_browseStackIndex).m_fullItemId != fullItemId)) {
            if (m_browseStackIndex > 0)
                m_browseStack.remove(0, m_browseStackIndex);
            m_browseStackIndex = 0;
            m_browseStack.prepend(he);
            if (m_browseStack.size() > MaxBrowseStack)
                m_browseStack.resize(MaxBrowseStack);
        } else if (color) {
            m_browseStack[m_browseStackIndex] = he;
        }
    }

    // (Re)add item to the history, unless it is the last added anyway
    if (m_browseHistory.isEmpty() || (m_browseHistory.constFirst().m_fullItemId != fullItemId)) {
        m_browseHistory.removeIf([fullItemId](const auto &i) { return i.m_fullItemId == fullItemId; });
        m_browseHistory.prepend(he);
        if (m_browseHistory.size() > MaxBrowseHistory)
            m_browseHistory.resize(MaxBrowseHistory);
    }

    updateBrowseActions();
}

bool AddItemDialog::replayBrowseEntry(BrowseMenuType type, int pos)
{
    const auto &vector = (type == BrowseMenuType::History) ? m_browseHistory : m_browseStack;

    if (pos < 0 || pos >= vector.size())
        return false;

    const auto &bhe = vector.at(pos);
    if (bhe.m_fullItemId.isEmpty())
        return false;

    if (type != BrowseMenuType::History)
        m_browseStackIndex = pos;

    m_currentlyRestoringBrowseHistory = true;
    w_select_item->restoreState(bhe.m_itemState);
    w_select_color->restoreState(bhe.m_colorState);
    restoreState(bhe.m_addState);
    m_currentlyRestoringBrowseHistory = false;

    recordBrowseEntry(type != BrowseMenuType::History);
    updateBrowseActions();
    return true;
}

void AddItemDialog::updateBrowseActions()
{
    w_go_back->setEnabled(m_browseStackIndex < (m_browseStack.size() - 1));
    w_go_next->setEnabled(m_browseStackIndex > 0);
}

void AddItemDialog::buildBrowseMenu(BrowseMenuType type)
{
    const auto &vector = (type == BrowseMenuType::History) ? m_browseHistory : m_browseStack;

    QMenu *m = nullptr;
    int d = 1;
    int start = 0;

    switch (type) {
    case BrowseMenuType::Back:
        m = m_backMenu;
        start = m_browseStackIndex + 1;
        break;
    case BrowseMenuType::Next:
        m = m_nextMenu;
        d = -1;
        start = m_browseStackIndex - 1;
        break;
    case BrowseMenuType::History:
        m = m_historyMenu;
        start = 0;
        break;
    }
    if (!m)
        return;
    m->clear();

    if (type == BrowseMenuType::History) {
        auto clearHistory = m->addAction(tr("Clear history"));
        connect(clearHistory, &QAction::triggered, this, [this]() {
            m_browseHistory.clear();
            updateBrowseActions();
        });
        m->addSeparator();

        if (m_browseHistory.isEmpty())
            m->addAction(tr("No browsing history"))->setEnabled(false);
    }

    auto now = QDateTime::currentDateTime();

    for (int i = start; (d < 0) ? (i >= 0) : (i < vector.size()); i += d) {
        const auto &bhe = vector.at(i);

        if (!bhe.m_fullItemId.isEmpty()) {
            auto *item = BrickLink::core()->item(bhe.m_fullItemId.at(0), bhe.m_fullItemId.mid(1));
            auto *color = BrickLink::core()->color(bhe.m_colorId);

            if (item) {
                QString s = item->name();
                if (color && color->id())
                    s = color->name() + u' ' + s;
                if (type == BrowseMenuType::History)
                    s = s + u"\t(" + HumanReadableTimeDelta::toString(now, bhe.m_lastVisited) + u')';

                auto *a = m->addAction(s);
                a->setData(i);
                auto *pic = BrickLink::core()->pictureCache()->picture(item, color, true);
                if (pic && pic->isValid())
                    a->setIcon(QPixmap::fromImage(pic->image()));
            }
        }
    }
}

QByteArray AddItemDialog::saveBrowseState() const
{
    auto saveBrowseHistoryEntry = [](QDataStream &ds, const BrowseHistoryEntry &bhe) {
        ds << bhe.m_fullItemId << bhe.m_colorId << bhe.m_lastVisited
           << bhe.m_itemState << bhe.m_colorState << bhe.m_addState;
    };

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("BS") << qint32(2)
       << int(m_browseHistory.size()) << int(m_browseStack.size()) << int(m_browseStackIndex);

    QByteArray compressedHistory;
    QDataStream dsCompressed(&compressedHistory, QIODevice::WriteOnly);

    for (int i = 0; i < m_browseHistory.size(); ++i)
        saveBrowseHistoryEntry(dsCompressed, m_browseHistory.at(i));
    for (int i = 0; i < m_browseStack.size(); ++i)
        saveBrowseHistoryEntry(dsCompressed, m_browseStack.at(i));

    ds << qCompress(compressedHistory, 9);
    return ba;
}

bool AddItemDialog::restoreBrowseState(const QByteArray &ba)
{
    auto restoreBrowseHistoryEntry = [](QDataStream &ds, BrowseHistoryEntry &bhe) -> bool {
        ds >> bhe.m_fullItemId >> bhe.m_colorId >> bhe.m_lastVisited
           >> bhe.m_itemState >> bhe.m_colorState >> bhe.m_addState;

        return (ds.status() == QDataStream::Ok);
    };

    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "BS") || (version < 1) || (version > 2))
        return false;

    int bhSize, bsSize, bsIndex;
    ds >> bhSize >> bsSize >> bsIndex;

    if (ds.status() != QDataStream::Ok)
        return false;

    if ((bhSize < 0) || (bhSize > MaxBrowseHistory) || (bsSize < 0) || (bsSize > MaxBrowseStack)
            || (bsIndex < -1) || (bsIndex >= bsSize)) {
        return false;
    }

    QByteArray compressedHistory;
    std::unique_ptr<QDataStream> dsCompressed;
    QDataStream *dsData = &ds;
    if (version >= 2) {
        ds >> compressedHistory;
        compressedHistory = qUncompress(compressedHistory);

        if (compressedHistory.isEmpty() || (ds.status() != QDataStream::Ok))
            return false;

        dsCompressed.reset(new QDataStream(compressedHistory));
        dsData = dsCompressed.get();
    }

    QVector<BrowseHistoryEntry> history;
    history.reserve(bhSize);
    for (int i = 0; i < bhSize; ++i) {
        BrowseHistoryEntry bhe;
        if (!restoreBrowseHistoryEntry(*dsData, bhe))
            return false;
        history.append(bhe);
    }

    QVector<BrowseHistoryEntry> stack;
    stack.reserve(bsSize);
    for (int i = 0; i < bsSize; ++i) {
        BrowseHistoryEntry bhe;
        if (!restoreBrowseHistoryEntry(*dsData, bhe))
            return false;
        stack.append(bhe);
    }

    m_browseHistory = history;
    m_browseStack = stack;
    m_browseStackIndex = bsIndex;

    updateBrowseActions();
    return true;
}

#include "moc_additemdialog.cpp"
