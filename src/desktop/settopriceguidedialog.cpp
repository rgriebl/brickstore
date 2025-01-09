// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDebug>
#include <QStyle>
#include <QButtonGroup>

#include "bricklink/core.h"
#include "common/config.h"
#include "settopriceguidedialog.h"


SetToPriceGuideDialog::SetToPriceGuideDialog(QWidget *parent)
    : QDialog(parent)
    , m_noPgOptions(new QButtonGroup(this))
{
    setupUi(this);

    m_noPgOptions->addButton(w_noChange, int(BrickLink::NoPriceGuideOption::NoChange));
    m_noPgOptions->addButton(w_zero, int(BrickLink::NoPriceGuideOption::Zero));
    m_noPgOptions->addButton(w_marker, int(BrickLink::NoPriceGuideOption::Marker));

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

    auto *option = m_noPgOptions->button(Config::inst()->value(u"MainWindow/SetToPriceGuideDialog/NoPriceOption"_qs,
                                                               0).toInt());
    if (option)
        option->setChecked(true);

    auto pgCache = BrickLink::core()->priceGuideCache();
    w_vatTypeDescription->setText(pgCache->descriptionForVatType(pgCache->currentVatType()));
    w_vatTypeIcon->setPixmap(pgCache->iconForVatType(pgCache->currentVatType()).pixmap(
                                 style()->pixelMetric(QStyle::PM_SmallIconSize)));
}

SetToPriceGuideDialog::~SetToPriceGuideDialog()
{
    Config::inst()->setValue(u"MainWindow/SetToPriceGuideDialog/Time"_qs, w_type_time->currentIndex());
    Config::inst()->setValue(u"MainWindow/SetToPriceGuideDialog/Price"_qs, w_type_price->currentIndex());
    Config::inst()->setValue(u"MainWindow/SetToPriceGuideDialog/NoPriceOption"_qs, m_noPgOptions->checkedId());
}

BrickLink::Time SetToPriceGuideDialog::time() const
{
    return static_cast<BrickLink::Time>(w_type_time->itemData(w_type_time->currentIndex()).toInt());
}

BrickLink::Price SetToPriceGuideDialog::price() const
{
    return static_cast<BrickLink::Price>(w_type_price->itemData(w_type_price->currentIndex()).toInt());
}

BrickLink::NoPriceGuideOption SetToPriceGuideDialog::noPriceGuideOption() const
{
    return static_cast<BrickLink::NoPriceGuideOption>(m_noPgOptions->checkedId());
}

bool SetToPriceGuideDialog::forceUpdate() const
{
    return w_force_update->isChecked();
}

#include "moc_settopriceguidedialog.cpp"
