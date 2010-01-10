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
#include "config.h"
#include "settopriceguidedialog.h"

Q_DECLARE_METATYPE(BrickLink::Time)
Q_DECLARE_METATYPE(BrickLink::Price)


SetToPriceGuideDialog::SetToPriceGuideDialog(QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    setupUi(this);
    
    w_type_time->addItem(tr("Last 6 Months Sales"), BrickLink::PastSix);
    w_type_time->addItem(tr("Current Inventory"), BrickLink::Current);
    
    w_type_price->addItem(tr("Minimum"), BrickLink::Lowest);
    w_type_price->addItem(tr("Average"), BrickLink::Average);
    w_type_price->addItem(tr("Quantity Average"), BrickLink::WAverage);
    w_type_price->addItem(tr("Maximum"), BrickLink::Highest);

    int timedef = Config::inst()->value(QLatin1String("/Defaults/SetToPG/Time"), 1 /*BrickLink::PastSix*/).toInt();
    int pricedef = Config::inst()->value(QLatin1String("/Defaults/SetToPG/Price"), 1 /*BrickLink::Average*/).toInt();

    if ((timedef >= 0) && (timedef < w_type_time->count()))
        w_type_time->setCurrentIndex(timedef);
    if ((pricedef >= 0) && (pricedef < w_type_price->count()))
        w_type_price->setCurrentIndex(pricedef);
}

BrickLink::Time SetToPriceGuideDialog::time() const
{
    return w_type_time->itemData(w_type_time->currentIndex()).value<BrickLink::Time>();
}

BrickLink::Price SetToPriceGuideDialog::price() const
{
    return w_type_price->itemData(w_type_price->currentIndex()).value<BrickLink::Price>();
}

bool SetToPriceGuideDialog::forceUpdate() const
{
    return w_force_update->isChecked();
}

