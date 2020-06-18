/* Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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
#include "config.h"
#include "settopriceguidedialog.h"


SetToPriceGuideDialog::SetToPriceGuideDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    
    w_type_time->addItem(tr("Last 6 Months Sales"), BrickLink::PastSix);
    w_type_time->addItem(tr("Current Inventory"), BrickLink::Current);
    
    w_type_price->addItem(tr("Minimum"), BrickLink::Lowest);
    w_type_price->addItem(tr("Average"), BrickLink::Average);
    w_type_price->addItem(tr("Quantity Average"), BrickLink::WAverage);
    w_type_price->addItem(tr("Maximum"), BrickLink::Highest);

    BrickLink::Time timedef = static_cast<BrickLink::Time>(Config::inst()->value(QLatin1String("/Defaults/SetToPG/Time"), BrickLink::PastSix).toInt());
    BrickLink::Price pricedef = static_cast<BrickLink::Price>(Config::inst()->value(QLatin1String("/Defaults/SetToPG/Price"), BrickLink::Average).toInt());

    w_type_time->setCurrentIndex(w_type_time->findData(timedef));
    w_type_price->setCurrentIndex(w_type_price->findData(pricedef));
}

BrickLink::Time SetToPriceGuideDialog::time() const
{
    return static_cast<BrickLink::Time>(w_type_time->itemData(w_type_time->currentIndex()).toInt());
}

BrickLink::Price SetToPriceGuideDialog::price() const
{
    return static_cast<BrickLink::Price>(w_type_price->itemData(w_type_price->currentIndex()).toInt());
}

bool SetToPriceGuideDialog::forceUpdate() const
{
    return w_force_update->isChecked();
}

