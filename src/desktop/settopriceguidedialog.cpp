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
#include <QDebug>
#include <QStyle>

#include "bricklink/core.h"
#include "common/config.h"
#include "settopriceguidedialog.h"


SetToPriceGuideDialog::SetToPriceGuideDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_advanced_button->setResizeTopLevelOnExpand(true);
    w_advanced_button->setExpandingWidget(w_advanced);
    
    w_type_time->addItem(tr("Last 6 Months Sales"), int(BrickLink::Time::PastSix));
    w_type_time->addItem(tr("Current Inventory"), int(BrickLink::Time::Current));
    
    w_type_price->addItem(tr("Minimum"), int(BrickLink::Price::Lowest));
    w_type_price->addItem(tr("Average"), int(BrickLink::Price::Average));
    w_type_price->addItem(tr("Quantity Average"), int(BrickLink::Price::WAverage));
    w_type_price->addItem(tr("Maximum"), int(BrickLink::Price::Highest));

    w_type_time->setCurrentIndex(Config::inst()->value(u"MainWindow/SetToPriceGuideDialog/Time"_qs,
                                                       int(BrickLink::Time::PastSix)).toInt());

    w_type_price->setCurrentIndex(Config::inst()->value(u"MainWindow/SetToPriceGuideDialog/Price"_qs,
                                                        int(BrickLink::Price::Average)).toInt());

    auto pgCache = BrickLink::core()->priceGuideCache();
    w_vatTypeDescription->setText(pgCache->descriptionForVatType(pgCache->currentVatType()));
    w_vatTypeIcon->setPixmap(pgCache->iconForVatType(pgCache->currentVatType()).pixmap(
                                 style()->pixelMetric(QStyle::PM_SmallIconSize)));
}

SetToPriceGuideDialog::~SetToPriceGuideDialog()
{
    Config::inst()->setValue(u"MainWindow/SetToPriceGuideDialog/Time"_qs, w_type_time->currentIndex());
    Config::inst()->setValue(u"MainWindow/SetToPriceGuideDialog/Price"_qs, w_type_price->currentIndex());
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

#include "moc_settopriceguidedialog.cpp"
