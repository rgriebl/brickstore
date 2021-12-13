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
#include <QStringBuilder>
#include <QTimer>
#include <QAction>

#include "bricklink/core.h"
#include "bricklink/model.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "utility/currency.h"
#include "utility/humanreadabletimedelta.h"
#include "utility/utility.h"
#include "view.h"
#include "picturewidget.h"
#include "priceguidewidget.h"
#include "appearsinwidget.h"
#include "selectitem.h"
#include "selectcolor.h"
#include "smartvalidator.h"
#include "additemdialog.h"


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

    w_select_color->setWidthToContents(true);

    w_add = new QPushButton();
    w_add->setDefault(true);
    w_buttons->addButton(w_add, QDialogButtonBox::ActionRole);

    w_picture->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_picture->setLineWidth(2);

    w_price_guide->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_price_guide->setLineWidth(2);

    w_appears_in->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    w_appears_in->setLineWidth(2);

    w_qty->setRange(1, DocumentModel::maxQuantity);
    w_qty->setValue(1);
    w_qty->installEventFilter(this);

    w_bulk->setRange(1, DocumentModel::maxQuantity);
    w_bulk->setValue(1);
    w_bulk->installEventFilter(this);

    double maxPrice = DocumentModel::maxLocalPrice(m_currency_code);

    w_price->setRange(0, maxPrice);
    w_price->installEventFilter(this);
    w_cost->setRange(0, maxPrice);
    w_cost->installEventFilter(this);

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
        w_tier_qty[i]->installEventFilter(this);
        w_tier_price[i]->installEventFilter(this);

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
            w_select_color->unlockColor();
        }
    });
    connect(w_select_item, &SelectItem::hasSubConditions,
            w_subcondition, &QWidget::setEnabled);
    connect(w_select_item, &SelectItem::itemSelected,
            this, [this](const BrickLink::Item *item, bool confirmed) {
        updateItemAndColor();
        w_select_color->setCurrentColorAndItem(w_select_color->currentColor(), item);
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

    w_toggles[0] = w_toggle_picture;
    w_toggles[1] = w_toggle_appears_in;
    w_toggles[2] = w_toggle_price_guide;
    for (int i = 0; i < 3; ++i) {
        connect(w_toggles[i], &QToolButton::toggled, this, [this, i](bool on) {
            w_splitter_bottom->widget(i)->setVisible(on);
        });
    }
    connect(w_toggle_seller_mode, &QToolButton::toggled,
            this, &AddItemDialog::setSellerMode);

    w_splitter_bottom->setStretchFactor(0, 2);
    w_splitter_bottom->setStretchFactor(1, 1);
    w_splitter_bottom->setStretchFactor(2, 0);
    w_splitter_bottom->setStretchFactor(3, 1);

    checkTieredPrices();

    QByteArray ba = Config::inst()->value("/MainWindow/AddItemDialog/Geometry"_l1)
            .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    ba = Config::inst()->value("/MainWindow/AddItemDialog/VSplitter"_l1)
            .toByteArray();
    if (!ba.isEmpty())
        w_splitter_vertical->restoreState(ba);
    ba = Config::inst()->value("/MainWindow/AddItemDialog/HSplitter"_l1)
            .toByteArray();
    if (!ba.isEmpty()) {
        char hidden = ba.at(0);
        for (int i = 0; i < 3; ++i) {
            if (hidden & (1 << i)) {
                w_splitter_bottom->widget(i)->hide();
                w_toggles[i]->setChecked(false);
            }
        }
        if (hidden & (1 << 7)) {
            w_toggle_seller_mode->setChecked(false);
            setSellerMode(false);
        }

        w_splitter_bottom->restoreState(ba.mid(1));
    }

    ba = Config::inst()->value("/MainWindow/AddItemDialog/SelectItem"_l1)
            .toByteArray();
    if (!w_select_item->restoreState(ba))
        w_select_item->restoreState(SelectItem::defaultState());

    ba = Config::inst()->value("/MainWindow/AddItemDialog/SelectColor"_l1)
            .toByteArray();
    if (!w_select_color->restoreState(ba))
        w_select_color->restoreState(SelectColor::defaultState());

    ba = Config::inst()->value("/MainWindow/AddItemDialog/ItemDetails"_l1)
            .toByteArray();
    restoreState(ba);

    w_last_added->installEventFilter(this); // dynamic tooltip

    m_historyTimer = new QTimer(this);
    m_historyTimer->setInterval(30s);
    connect(m_historyTimer, &QTimer::timeout, this, &AddItemDialog::updateHistoryText);


    if (QAction *a = ActionManager::inst()->qAction("bricklink_catalog")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item) {
                BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, item, color);
            }
        });
    }
    if (QAction *a = ActionManager::inst()->qAction("bricklink_priceguide")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item && (color || !item->itemType()->hasColors())) {
                BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, item, color);
            }
        });
    }
    if (QAction *a = ActionManager::inst()->qAction("bricklink_lotsforsale")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select_item->currentItem();
            const auto color = w_select_color->currentColor();
            if (item && (color || !item->itemType()->hasColors())) {
                BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, item, color);
            }
        });
    }

    languageChange();
}

void AddItemDialog::languageChange()
{
    Ui::AddItemDialog::retranslateUi(this);
    m_price_label_fmt = w_label_currency->text();

    w_add->setText(tr("Add"));

    updateCurrencyCode();
    updateCaption();
    updateHistoryText();
}

AddItemDialog::~AddItemDialog()
{
    Config::inst()->setValue("/MainWindow/AddItemDialog/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/AddItemDialog/VSplitter"_l1, w_splitter_vertical->saveState());

    QByteArray ba = w_splitter_bottom->saveState();
    char hidden = 0;
    for (int i = 0; i < 3; ++i) {
        if (!w_toggles[i]->isChecked())
            hidden |= (1 << i);
    }
    if (!w_toggle_seller_mode->isChecked())
        hidden |= (1 << 7);
    ba.prepend(hidden);
    Config::inst()->setValue("/MainWindow/AddItemDialog/HSplitter"_l1, ba);
    Config::inst()->setValue("/MainWindow/AddItemDialog/SelectItem"_l1, w_select_item->saveState());
    Config::inst()->setValue("/MainWindow/AddItemDialog/SelectColor"_l1, w_select_color->saveState());
    Config::inst()->setValue("/MainWindow/AddItemDialog/ItemDetails"_l1, saveState());

    w_picture->setItemAndColor(nullptr);
    w_price_guide->setPriceGuide(nullptr);
    w_appears_in->setItem(nullptr, nullptr);
}

void AddItemDialog::updateCaption()
{
    setWindowTitle(m_caption_fmt.arg(m_view ? m_view->document()->fileNameOrTitle()
                                              : QString { }));
}

void AddItemDialog::updateCurrencyCode()
{
    m_currency_code = m_view ? m_view->model()->currencyCode() : "USD"_l1;

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

bool AddItemDialog::eventFilter(QObject *watched, QEvent *event)
{
    bool res = QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::FocusIn) {
        if (auto *le = qobject_cast<QLineEdit *>(watched))
            QMetaObject::invokeMethod(le, &QLineEdit::selectAll, Qt::QueuedConnection);
        else if (auto *sb = qobject_cast<QAbstractSpinBox *>(watched))
            QMetaObject::invokeMethod(sb, &QAbstractSpinBox::selectAll, Qt::QueuedConnection);

    } else if (event->type() == QEvent::ToolTip && watched == w_last_added) {
        const auto *he = static_cast<QHelpEvent *>(event);
        if (m_addHistory.size() > 1) {
            static const QString pre = "<p style='white-space:pre'>"_l1;
            static const QString post = "</p>"_l1;
            QString tips;

            for (const auto &entry : m_addHistory)
                tips = tips % pre % historyTextFor(entry.first, entry.second) % post;

            QToolTip::showText(he->globalPos(), tips, w_last_added, w_last_added->geometry());
        }
        return true;
    }
    return res;
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

    if (item && color) {
        w_picture->setItemAndColor(item, color);
        w_price_guide->setPriceGuide(BrickLink::core()->priceGuide(item, color, true));
        w_appears_in->setItem(item, color);
    }
    else {
        w_picture->setItemAndColor(nullptr);
        w_price_guide->setPriceGuide(nullptr);
        w_appears_in->setItem(nullptr, nullptr);
    }
    checkAddPossible();
}

void AddItemDialog::setTierType(int type)
{
    (type == 0 ? w_radio_percent : w_radio_currency)->setChecked(true);

    for (auto *tp : qAsConst(w_tier_price)) {
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

void AddItemDialog::updateHistoryText()
{
    if (m_addHistory.empty()) {
        w_last_added->setText(tr("Your recently added items will be listed here"));
        m_historyTimer->stop();
    } else {
        const auto &hist = *m_addHistory.crbegin();
        w_last_added->setText(historyTextFor(hist.first, hist.second));
        m_historyTimer->start();
    }
}

QString AddItemDialog::historyTextFor(const QDateTime &when, const Lot &lot)
{
    auto now = QDateTime::currentDateTime();
    QString cs;
    if (lot.color() && lot.color()->id()) {
        QColor color = lot.color()->color();
        cs = R"(<b><font color=")"_l1 % Utility::textColor(color).name() %
                R"(" style="background-color: )"_l1 % color.name() % R"(;">&nbsp;)"_l1 %
                lot.colorName() % R"(&nbsp;</font></b>&nbsp;&nbsp;)"_l1;
    }

    QString s = tr("Added %1").arg(HumanReadableTimeDelta::toString(now, when)) %
            u":&nbsp;&nbsp;<b>" % QString::number(lot.quantity()) % u"</b>&nbsp;&nbsp;" % cs %
            lot.itemName() % u" <i>[" + QLatin1String(lot.itemId()) % u"]</i>";

    return s;
}

QByteArray AddItemDialog::saveState() const
{
    bool tierCurrency = w_radio_currency->isChecked();

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("AI") << qint32(2)
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
    return ba;
}

bool AddItemDialog::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "AI") || (version != 2))
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

    ds >> qty >> bulk >> price >> cost >> tierCurrency;
    for (int i = 0; i < 3; ++i)
        ds >> tierQty[i] >> tierPrice[i];
    ds >> isNew >> subConditionIndex >> comments >> remarks >> consolidate;

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
    return true;
}

bool AddItemDialog::checkAddPossible()
{

    bool acceptable = w_select_item->currentItem()
            && w_price->hasAcceptableInput()
            && w_qty->hasAcceptableInput()
            && w_bulk->hasAcceptableInput();

    if (auto currentType = w_select_item->currentItemType()) {
        if (currentType->hasColors())
            acceptable = acceptable && (w_select_color->currentColor());
    }

    for (int i = 0; i < 3; i++) {
        if (!w_tier_price [i]->isEnabled())
            break;

        acceptable = acceptable
                && w_tier_qty[i]->hasAcceptableInput()
                && w_tier_price[i]->hasAcceptableInput();

        if (i > 0) {
            acceptable = acceptable
                    && (tierPriceValue(i - 1) > tierPriceValue(i))
                    && (w_tier_qty[i - 1]->text().toInt() < w_tier_qty[i]->text().toInt());
        } else {
            acceptable = acceptable
                    && (Currency::fromString(w_price->text()) > tierPriceValue(i));
        }
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

    auto lot = new BrickLink::Lot(color, item);

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

    m_addHistory.emplace_back(qMakePair(QDateTime::currentDateTime(), *lot));
    while (m_addHistory.size() > 6)
        m_addHistory.pop_front();
    updateHistoryText();

    m_view->addLots({ lot }, w_merge->isChecked() ? View::AddLotMode::ConsolidateWithExisting
                                                  : View::AddLotMode::AddAsNew);
}

#include "moc_additemdialog.cpp"
