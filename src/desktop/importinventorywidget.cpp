// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>

#include "bricklink/core.h"
#include "importinventorywidget.h"


namespace {


} // namspace

ImportInventoryWidget::ImportInventoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    languageChange();
}

void ImportInventoryWidget::setItem(const BrickLink::Item *item)
{
    BrickLink::PartOutTraits traits;
    if (item)
        traits = item->partOutTraits();

    w_extra->setEnabled(traits & BrickLink::PartOutTrait::Extras);
    w_instructions->setEnabled(traits & BrickLink::PartOutTrait::Instructions);
    w_originalBox->setEnabled(traits & BrickLink::PartOutTrait::OriginalBox);
    w_alternates->setEnabled(traits & BrickLink::PartOutTrait::Alternates);
    w_counterParts->setEnabled(traits & BrickLink::PartOutTrait::CounterParts);
    w_setsInSet->setEnabled(traits & BrickLink::PartOutTrait::SetsInSet);
    w_minifigs->setEnabled(traits & BrickLink::PartOutTrait::Minifigs);
}

int ImportInventoryWidget::quantity() const
{
    return w_qty->value();
}

void ImportInventoryWidget::setQuantity(int qty)
{
    if (qty)
        w_qty->setValue(qty);
}

BrickLink::Condition ImportInventoryWidget::condition() const
{
    return w_condition_used->isChecked() ? BrickLink::Condition::Used : BrickLink::Condition::New;
}

void ImportInventoryWidget::setCondition(BrickLink::Condition condition)
{
    w_condition_new->setChecked(condition == BrickLink::Condition::New);
    w_condition_used->setChecked(condition != BrickLink::Condition::New);
}

BrickLink::Status ImportInventoryWidget::extraParts() const
{
    return static_cast<BrickLink::Status>(w_extra->currentIndex());
}

BrickLink::PartOutTraits ImportInventoryWidget::partOutTraits() const
{
    BrickLink::PartOutTraits traits = { };
    if (w_instructions->isEnabled() && w_instructions->isChecked())
        traits |= BrickLink::PartOutTrait::Instructions;
    if (w_originalBox->isEnabled() && w_originalBox->isChecked())
        traits |= BrickLink::PartOutTrait::OriginalBox;
    if (w_alternates->isEnabled() && w_alternates->isChecked())
        traits |= BrickLink::PartOutTrait::Alternates;
    if (w_counterParts->isEnabled() && w_counterParts->isChecked())
        traits |= BrickLink::PartOutTrait::CounterParts;
    if (w_setsInSet->isEnabled() && w_setsInSet->isChecked())
        traits |= BrickLink::PartOutTrait::SetsInSet;
    if (w_minifigs->isEnabled() && w_minifigs->isChecked())
        traits |= BrickLink::PartOutTrait::Minifigs;
    return traits;
}


void ImportInventoryWidget::languageChange()
{
    retranslateUi(this);
}

void ImportInventoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}


QByteArray ImportInventoryWidget::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("II") << qint32(4)
       << w_condition_new->isChecked()
       << qint32(w_qty->value())
       << qint32(w_extra->currentIndex())
       << w_instructions->isChecked()
       << w_alternates->isChecked()
       << w_counterParts->isChecked()
       << w_originalBox->isChecked()
       << w_setsInSet->isChecked()
       << w_minifigs->isChecked();
    return ba;
}

bool ImportInventoryWidget::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version < 1) || (version > 4))
        return false;

    bool isNew;
    qint32 qty;

    ds >> isNew >> qty;

    if (ds.status() != QDataStream::Ok)
        return false;

    w_condition_new->setChecked(isNew);
    w_condition_used->setChecked(!isNew);
    w_qty->setValue(qty);

    if (version >= 2) {
        qint32 extras;
        bool instructions;

        ds >> extras >> instructions;

        if (ds.status() != QDataStream::Ok)
            return false;

        w_instructions->setChecked(instructions);
        w_extra->setCurrentIndex(extras);
    }
    if (version >= 3) {
        bool alternates;
        bool counterParts;

        ds >> alternates >> counterParts;

        if (ds.status() != QDataStream::Ok)
            return false;

        w_alternates->setChecked(alternates);
        w_counterParts->setChecked(counterParts);
    }
    if (version >= 4) {
        bool originalBox;
        bool setsInSet;
        bool minifigs;

        ds >> originalBox >> setsInSet >> minifigs;

        if (ds.status() != QDataStream::Ok)
            return false;

        w_originalBox->setChecked(originalBox);
        w_setsInSet->setChecked(setsInSet);
        w_minifigs->setChecked(minifigs);
    }

    return true;
}


#include "moc_importinventorywidget.cpp"
