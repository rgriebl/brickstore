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
#pragma once

#include <QDialog>
#include "bricklink/global.h"

#include "ui_importinventorywidget.h"


class ImportInventoryWidget : public QWidget, private Ui::ImportInventoryWidget
{
    Q_OBJECT
public:
    ImportInventoryWidget(QWidget *parent = nullptr);
    ~ImportInventoryWidget() override;

    void setItem(const BrickLink::Item *item);
    int quantity() const;
    void setQuantity(int qty);
    BrickLink::Condition condition() const;
    void setCondition(BrickLink::Condition condition);
    BrickLink::Status extraParts() const;
    bool includeInstructions() const;
    bool includeAlternates() const;
    bool includeCounterParts() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

protected:
    void changeEvent(QEvent *e) override;

    void languageChange();
};
