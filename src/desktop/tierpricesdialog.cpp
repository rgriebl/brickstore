// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QBoxLayout>
#include <QGridLayout>
#include <QSpinBox>

#include "desktop/desktopuihelpers.h"
#include "tierpricesdialog.h"


TierPricesDialog::TierPricesDialog(QWidget *parent)
    : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Set tier prices relative to the lot's base price")));

    auto grid = new QGridLayout();

    for (int i = 0; i < 3; ++i) {
        auto *spin = m_spinBoxes[i] = new QSpinBox();
        spin->setAlignment(Qt::AlignRight);
        spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
        spin->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        spin->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        spin->setRange(0, 99);
        spin->setValue(0);
        spin->setPrefix(u"-"_qs);
        spin->setSuffix(u"%"_qs);
        spin->setSpecialValueText(u"  " + tr("No change"));

        auto label = new QLabel(tr("Tier %1").arg(i + 1));
        label->setAlignment(Qt::AlignCenter);

        grid->addWidget(label, 0, i);
        grid->addWidget(spin, 1, i);

        new EventFilter(spin, { QEvent::FocusIn }, DesktopUIHelpers::selectAllFilter);

        connect(spin, &QSpinBox::valueChanged,
                this, &TierPricesDialog::checkValues);
    }

    layout->addLayout(grid);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setFocusProxy(m_spinBoxes[0]);

    checkValues();
}

void TierPricesDialog::checkValues()
{
    bool allValid = true;
    bool allZeros = true;

    // all prior ones need to be either 0 or smaller
    for (int i = 0; i < 3; ++i) {
        int value = m_spinBoxes[i]->value();
        bool valid = true;

        for (int k = i - 1; k >= 0; --k) {
            int prevValue = m_spinBoxes[k]->value();
            valid = valid && (!value || (!prevValue || (prevValue < value)));
        }
        m_spinBoxes[i]->setProperty("showInputError", !valid);

        allValid = allValid && valid;
        allZeros = allZeros && !value;
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(allValid && !allZeros);
}

std::array<double,3> TierPricesDialog::percentagesOff() const
{
    return { double(m_spinBoxes[0]->value()), double(m_spinBoxes[1]->value()),
            double(m_spinBoxes[2]->value()) };
}

#include "moc_tierpricesdialog.cpp"
