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
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>

#include "incdecpricesdialog.h"
#include "utility/utility.h"
#include "framework.h"


IncDecPricesDialog::IncDecPricesDialog(const QString &text, bool showTiers,
                                       const QString &currencyCode, QWidget *parent)
    : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);

    auto label = new QLabel();
    label->setText(text);
    layout->addWidget(label);

    auto hlayout = new QHBoxLayout();
    hlayout->setSpacing(0);

    m_value = new QDoubleSpinBox();
    m_value->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    m_value->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    m_value->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_value->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    hlayout->addWidget(m_value);

    hlayout->addSpacing(11);

    auto percent = new QToolButton();
    percent->setAutoRaise(true);
    percent->setText("%"_l1);
    percent->setCheckable(true);
    percent->setChecked(true);
    percent->setShortcut(tr("Ctrl+%"));
    percent->setToolTip(Utility::toolTipLabel(tr("Percent"), percent->shortcut()));
    hlayout->addWidget(percent);

    auto fixed = new QToolButton();
    fixed->setAutoRaise(true);
    fixed->setText(currencyCode);
    fixed->setCheckable(true);
    fixed->setShortcut(tr("Ctrl+$"));
    fixed->setToolTip(Utility::toolTipLabel(tr("Fixed %1 amount").arg(currencyCode),
                                            fixed->shortcut()));
    hlayout->addWidget(fixed);
    layout->addLayout(hlayout);

    m_percentOrFixed = new QButtonGroup(this);
    m_percentOrFixed->addButton(percent, 0);
    m_percentOrFixed->addButton(fixed, 1);

    if (showTiers) {
        m_applyToTiers = new QCheckBox(tr("Also apply this change to &tier prices"));
        layout->addWidget(m_applyToTiers);
    }

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_percentOrFixed, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, [this](auto *) {
        switchType(m_percentOrFixed->checkedId());
    });
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(m_value, QOverload<const QString &>::of(&QDoubleSpinBox::valueChanged),
#else
    connect(m_value, &QDoubleSpinBox::textChanged,
#endif
            this, [this, buttons]() {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(m_value->hasAcceptableInput());
    });

    switchType(0);
    setFocusProxy(m_value);
    m_value->selectAll();
}

void IncDecPricesDialog::switchType(int type)
{
    auto oldType = m_value->property("bsType");
    if (!oldType.isNull() && oldType.toInt() == type)
        return;
    m_value->setProperty("bsType", type);
    m_value->setValue(0);

    if (type == 0) { // percent
        m_value->setRange(-99, 1000);
        m_value->setDecimals(2);
    } else { // fixed
        m_value->setRange(-FrameWork::maxPrice, FrameWork::maxPrice);
        m_value->setDecimals(3);
    }
}

double IncDecPricesDialog::fixed() const
{
    if (m_value->hasAcceptableInput() && (m_percentOrFixed->checkedId() == 1))
        return m_value->value();
    return 0;
}

double IncDecPricesDialog::percent() const
{
    if (m_value->hasAcceptableInput() && (m_percentOrFixed->checkedId() == 0))
        return m_value->value();
    return 0;
}

bool IncDecPricesDialog::applyToTiers() const
{
    return m_applyToTiers && m_applyToTiers->isChecked();
}

#include "moc_incdecpricesdialog.cpp"
