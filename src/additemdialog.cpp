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

#include "config.h"
#include "currency.h"
#include "bricklink.h"
#include "bricklink_model.h"
#include "window.h"
#include "document.h"
#include "picturewidget.h"
#include "priceguidewidget.h"
#include "appearsinwidget.h"
#include "selectitem.h"
#include "selectcolor.h"
#include "additemdialog.h"


class ValidatorSpinBox : public QSpinBox
{
public:
    static void setValidator(QSpinBox *spinbox, const QValidator *validator)
    {
        // why is lineEdit() protected?
        (spinbox->*(&ValidatorSpinBox::lineEdit))()->setValidator(validator);
    }
};


AddItemDialog::AddItemDialog(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setupUi(this);

    m_window = nullptr;
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
    w_buttons->addButton(w_add, QDialogButtonBox::ActionRole);

    auto itId = Config::inst()->value("/Defaults/AddItems/ItemType", 'P').value<char>();
    w_select_item->setCurrentItemType(BrickLink::core()->itemType(itId));
    w_select_item->setCurrentCategory(BrickLink::CategoryModel::AllCategories);

    w_picture->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    w_picture->setLineWidth(2);

    w_price_guide->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    w_price_guide->setLineWidth(2);

    w_appears_in->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    w_appears_in->setLineWidth(2);

    ValidatorSpinBox::setValidator(w_qty, new QIntValidator(1, 99999, w_qty));
    w_qty->setValue(1);

    ValidatorSpinBox::setValidator(w_bulk, new QIntValidator(1, 99999, w_bulk));
    w_bulk->setValue(1);

    w_price->setValidator(new CurrencyValidator(0.000, 10000, 3, w_price));

    w_tier_qty [0] = w_tier_qty_0;
    w_tier_qty [1] = w_tier_qty_1;
    w_tier_qty [2] = w_tier_qty_2;
    w_tier_price [0] = w_tier_price_0;
    w_tier_price [1] = w_tier_price_1;
    w_tier_price [2] = w_tier_price_2;

    for (int i = 0; i < 3; i++) {
        ValidatorSpinBox::setValidator(w_tier_qty[i], new QIntValidator(1, 99999, w_tier_qty[i]));
        w_tier_qty [i]->setValue(0);
        w_tier_price [i]->setText(QString());

        connect(w_tier_qty [i], QOverload<int>::of(&QSpinBox::valueChanged),
                this, &AddItemDialog::checkTieredPrices);
        connect(w_tier_price [i], &QLineEdit::textChanged,
                this, &AddItemDialog::checkTieredPrices);
    }

    m_percent_validator = new QIntValidator(1, 99, this);
    m_money_validator = new CurrencyValidator(0.001, 10000, 3, this);

    connect(w_select_item, &SelectItem::hasColors,
            w_select_color, &QWidget::setEnabled);
    connect(w_select_item, &SelectItem::itemSelected,
            this, &AddItemDialog::updateItemAndColor);
    connect(w_select_color, &SelectColor::colorSelected,
            this, &AddItemDialog::updateItemAndColor);
    connect(w_price, &QLineEdit::textChanged,
            this, &AddItemDialog::showTotal);
    connect(w_qty, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddItemDialog::showTotal);
    connect(m_tier_type, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &AddItemDialog::setTierType);
    connect(w_bulk, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddItemDialog::checkAddPossible);

    connect(w_add, &QAbstractButton::clicked,
            this, &AddItemDialog::addClicked);

    connect(w_price_guide, &PriceGuideWidget::priceDoubleClicked,
            this, &AddItemDialog::setPrice);

    //TODO5 ??? connect(Config::inst(), &Config::simpleModeChanged, this, &AddItemDialog::setSimpleMode);

    new QShortcut(Qt::Key_Escape, this, SLOT(close()));

    if (Config::inst()->value("/Defaults/AddItems/Condition", "new").toString() != "new")
        w_radio_used->setChecked(true);
    else
        w_radio_new->setChecked(true);

    w_radio_percent->click();

    showTotal();
    checkTieredPrices();

    languageChange();
}

void AddItemDialog::languageChange()
{
    Ui::AddItemDialog::retranslateUi(this);
    m_price_label_fmt = w_label_currency->text();

    w_add->setText(tr("Add"));

    updateCurrencyCode();
    updateCaption();
}

AddItemDialog::~AddItemDialog()
{
    w_picture->setPicture(nullptr);
    w_price_guide->setPriceGuide(nullptr);
    w_appears_in->setItem(nullptr, nullptr);
}

void AddItemDialog::updateCaption()
{
    setWindowTitle(m_caption_fmt.arg(m_window ? m_window->document()->title() : QString()));
}

void AddItemDialog::updateCurrencyCode()
{
    m_currency_code = m_window ? m_window->document()->currencyCode() : QLatin1String("USD");

    w_price_guide->setCurrencyCode(m_currency_code);

    QString local = Currency::localSymbol(m_currency_code);

    w_label_currency->setText(m_price_label_fmt.arg(local));
    w_radio_currency->setText(local);
    //w_price->setText(Currency::toString(0, m_currencycode));
}

void AddItemDialog::attach(Window *w)
{
    if (m_window) {
        disconnect(m_window->document(), &Document::titleChanged,
                   this, &AddItemDialog::updateCaption);
        disconnect(m_window->document(), &Document::currencyCodeChanged,
                   this, &AddItemDialog::updateCurrencyCode);
    }
    m_window = w;
    if (m_window) {
        connect(m_window->document(), &Document::titleChanged,
                this, &AddItemDialog::updateCaption);
        connect(m_window->document(), &Document::currencyCodeChanged,
                this, &AddItemDialog::updateCurrencyCode);
    }
    setEnabled(m_window);

    updateCaption();
    updateCurrencyCode();
}

void AddItemDialog::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() == Qt::ControlModifier) {
        double o = windowOpacity() + double(e->angleDelta().y()) / 1200.0;
        setWindowOpacity(double(qMin(qMax(o, 0.2), 1.0)));

        e->accept();
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

void AddItemDialog::setSimpleMode(bool b)
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
            (*wpp)->hide();
        else
            (*wpp)->show();
    }

    if (b) {
        w_bulk->setValue(1);
        w_tier_qty [0]->setValue(0);
        w_comments->setText(QString());
        checkTieredPrices();
    }
}

void AddItemDialog::updateItemAndColor()
{
    showItemInColor(w_select_item->currentItem(), w_select_color->currentColor());
}

void AddItemDialog::showItemInColor(const BrickLink::Item *it, const BrickLink::Color *col)
{
    if (it && col) {
        w_picture->setPicture(BrickLink::core()->picture(it, col, true));
        w_price_guide->setPriceGuide(BrickLink::core()->priceGuide(it, col, true));
        w_appears_in->setItem(it, col);
    }
    else {
        w_picture->setPicture(nullptr);
        w_price_guide->setPriceGuide(nullptr);
        w_appears_in->setItem(nullptr, nullptr);
    }
    checkAddPossible();
}

void AddItemDialog::showTotal()
{
    double tot = 0;

    if (w_price->hasAcceptableInput() && w_qty->hasAcceptableInput())
        tot = Currency::fromString(w_price->text()) * w_qty->text().toInt();

    w_total->setText(Currency::toString(tot, m_currency_code));

    checkAddPossible();
}

void AddItemDialog::setTierType(int type)
{
    QValidator *valid = (type == 0) ? m_percent_validator : m_money_validator;
    QString text = (type == 0) ? QString("0") : Currency::toString(0, m_currency_code);

    for (const auto &tp : w_tier_price) {
        tp->setValidator(valid);
        tp->setText(text);
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
    if (i < 0 || i > 2)
        return 0.;

    double val;

    if (m_tier_type->checkedId() == 0)     // %
        val = Currency::fromString(w_price->text()) * (100 - w_tier_price [i]->text().toInt()) / 100;
    else // $
        val = Currency::fromString(w_tier_price [i]->text());

    return val;
}

bool AddItemDialog::checkAddPossible()
{
    bool acceptable = w_select_item->currentItem() &&
                      w_select_color->currentColor() &&
                      w_price->hasAcceptableInput() &&
                      w_qty->hasAcceptableInput() &&
                      w_bulk->hasAcceptableInput();

    for (int i = 0; i < 3; i++) {
        if (!w_tier_price [i]->isEnabled())
            break;

        acceptable = acceptable && w_tier_qty[i]->hasAcceptableInput()
                && w_tier_price[i]->hasAcceptableInput();

        if (i > 0) {
            acceptable = acceptable && (tierPriceValue(i - 1) > tierPriceValue(i))
                    && (w_tier_qty[i - 1]->text().toInt() < w_tier_qty[i]->text().toInt());
        } else {
            acceptable = acceptable && (Currency::fromString(w_price->text()) > tierPriceValue(i));
        }
    }


    w_add->setEnabled(acceptable);
    return acceptable;
}

void AddItemDialog::addClicked()
{
    if (!checkAddPossible() || !m_window)
        return;

    auto *ii = new BrickLink::InvItem(w_select_color->currentColor(), w_select_item->currentItem());

    ii->setQuantity(w_qty->text().toInt());
    ii->setPrice(Currency::fromString(w_price->text()));
    ii->setBulkQuantity(w_bulk->text().toInt());
    ii->setCondition(static_cast <BrickLink::Condition>(m_condition->checkedId()));
    ii->setRemarks(w_remarks->text());
    ii->setComments(w_comments->text());

    for (int i = 0; i < 3; i++) {
        if (!w_tier_price [i]->isEnabled())
            break;

        ii->setTierQuantity(i, w_tier_qty [i]->text().toInt());
        ii->setTierPrice(i, tierPriceValue(i));
    }

    m_window->addItem(ii, w_merge->isChecked() ? Window::MergeAction_Force | Window::MergeKeep_Old : Window::MergeAction_None);
}

void AddItemDialog::setPrice(double d)
{
    w_price->setText(Currency::toString(d, m_currency_code));
    checkAddPossible();
}

#include "moc_additemdialog.cpp"
