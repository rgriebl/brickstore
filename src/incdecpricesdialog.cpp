/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#include <QPushButton>

#include "incdecpricesdialog.h"


IncDecPricesDialog::IncDecPricesDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    setupUi(this);
    
    w_value->setText(QLatin1String("0"));
    w_fixed->setText(Currency::symbol());

    m_pos_percent_validator = new QDoubleValidator (0., 1000., 1, this);
    m_neg_percent_validator = new QDoubleValidator (0., 99.,   1, this);
    m_fixed_validator       = new CurrencyValidator(0,  10000, 3, this);

    connect(w_increase, SIGNAL(toggled(bool)), this, SLOT(updateValidators()));
    connect(w_percent, SIGNAL(toggled(bool)), this, SLOT(updateValidators()));
    connect(w_value, SIGNAL(textChanged(const QString &)), this, SLOT(checkValue()));

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    updateValidators();
}

void IncDecPricesDialog::checkValue()
{
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(w_value->hasAcceptableInput());
}

void IncDecPricesDialog::updateValidators()
{
    if (w_percent->isChecked()) {
        w_value->setValidator(w_increase->isChecked() ? m_pos_percent_validator : m_neg_percent_validator);
        w_value->setText(QLatin1String("0"));
    } else {
        w_value->setValidator(m_fixed_validator);
        w_value->setText(Currency(0).toLocal());
    }
    checkValue();
}


Currency IncDecPricesDialog::fixed() const
{
    if (w_value->hasAcceptableInput() && w_fixed->isChecked()) {
        Currency v = Currency::fromLocal(w_value->text());
        return w_increase->isChecked() ? v : -v;
    } else {
        return 0;
    }
}

double IncDecPricesDialog::percent() const
{
    if (w_value->hasAcceptableInput() && w_percent->isChecked()) {
        double v = w_value->text().toDouble();
        return w_increase->isChecked() ? v : -v;
    } else {
        return 0.;
    }
}

bool IncDecPricesDialog::applyToTiers() const
{
    return w_apply_to_tiers->isChecked();
}
