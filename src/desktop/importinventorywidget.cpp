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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>

#include "bricklink/core.h"
#include "importinventorywidget.h"


ImportInventoryWidget::ImportInventoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    languageChange();
}

ImportInventoryWidget::~ImportInventoryWidget()
{ }

void ImportInventoryWidget::setItem(const BrickLink::Item *item)
{
    bool hasInstructions = false;
    bool hasCounterParts = false;
    bool hasAlternates = false;
    bool hasExtras = false;

    if (item) {
        if (item->itemTypeId() == 'S')
            hasInstructions = (BrickLink::core()->item('I', item->id()));

        const auto &inv = item->consistsOf();
        for (const auto &co : inv) {
            if (co.isExtra())
                hasExtras = true;
            if (co.isCounterPart())
                hasCounterParts = true;
            if (co.isAlternate())
                hasAlternates = true;
            if (hasAlternates && hasCounterParts && hasExtras)
                break;
        }
    }
    w_extra->setEnabled(hasExtras);
    w_instructions->setEnabled(hasInstructions);
    w_alternates->setEnabled(hasAlternates);
    w_counterParts->setEnabled(hasCounterParts);
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
    w_condition_used->setChecked(condition != BrickLink::Condition::New);
}

BrickLink::Status ImportInventoryWidget::extraParts() const
{
    return static_cast<BrickLink::Status>(w_extra->currentIndex());
}

bool ImportInventoryWidget::includeInstructions() const
{
    return w_instructions->isEnabled() && w_instructions->isChecked();
}

bool ImportInventoryWidget::includeAlternates() const
{
    return w_alternates->isEnabled() && w_alternates->isChecked();
}

bool ImportInventoryWidget::includeCounterParts() const
{
    return w_counterParts->isEnabled() && w_counterParts->isChecked();
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
    ds << QByteArray("II") << qint32(3)
       << w_condition_new->isChecked()
       << qint32(w_qty->value())
       << qint32(w_extra->currentIndex())
       << w_instructions->isChecked()
       << w_alternates->isChecked()
       << w_counterParts->isChecked();
    return ba;
}

bool ImportInventoryWidget::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version < 1) || (version > 3))
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

    return true;
}


#include "moc_importinventorywidget.cpp"
