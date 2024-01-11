// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>

#include "common/actionmanager.h"
#include "common/documentmodel.h"
#include "incdecpricesdialog.h"


IncDecPricesDialog::IncDecPricesDialog(const QString &text, bool showTiers,
                                       const QString &currencyCode, QWidget *parent)
    : QDialog(parent)
    , m_currencyCode(currencyCode)
{
    auto layout = new QVBoxLayout(this);

    auto label = new QLabel(this);
    label->setText(text);
    layout->addWidget(label);

    auto hlayout = new QHBoxLayout();
    hlayout->setSpacing(0);

    m_value = new QDoubleSpinBox(this);
    m_value->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    m_value->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    m_value->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_value->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    hlayout->addWidget(m_value);

    hlayout->addSpacing(11);

    auto percent = new QToolButton(this);
    percent->setProperty("iconScaling", true);
    percent->setText(u"%"_qs);
    percent->setCheckable(true);
    percent->setChecked(true);
    percent->setShortcut(tr("Ctrl+%"));
    percent->setToolTip(ActionManager::toolTipLabel(tr("Percent"), percent->shortcut()));
    hlayout->addWidget(percent);

    auto fixed = new QToolButton(this);
    fixed->setProperty("iconScaling", true);
    fixed->setText(currencyCode);
    fixed->setCheckable(true);
    fixed->setShortcut(tr("Ctrl+$"));
    fixed->setToolTip(ActionManager::toolTipLabel(tr("Fixed %1 amount").arg(currencyCode),
                                                  fixed->shortcut()));
    hlayout->addWidget(fixed);
    layout->addLayout(hlayout);

    m_percentOrFixed = new QButtonGroup(this);
    m_percentOrFixed->addButton(percent, 0);
    m_percentOrFixed->addButton(fixed, 1);

    if (showTiers) {
        m_applyToTiers = new QCheckBox(tr("Also apply this change to &tier prices"), this);
        layout->addWidget(m_applyToTiers);
    }

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_percentOrFixed, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, [this](auto *) {
        switchType(m_percentOrFixed->checkedId());
    });
    connect(m_value, &QDoubleSpinBox::textChanged,
            this, [this, buttons]() {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(m_value->hasAcceptableInput());
    });

    switchType(0);
    m_value->selectAll();
    setFocusProxy(m_value);
    setFocus();
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
        double maxPrice = DocumentModel::maxLocalPrice(m_currencyCode);

        m_value->setRange(-maxPrice, maxPrice);
        m_value->setDecimals(3);
    }
}

bool IncDecPricesDialog::isFixed() const
{
    return (m_percentOrFixed->checkedId() == 1);
}

double IncDecPricesDialog::value() const
{
    if (m_value->hasAcceptableInput())
        return m_value->value();
    return 0;
}

bool IncDecPricesDialog::applyToTiers() const
{
    return m_applyToTiers && m_applyToTiers->isChecked();
}

#include "moc_incdecpricesdialog.cpp"
