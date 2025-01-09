// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include "bricklink/global.h"

#include "ui_importinventorywidget.h"


class ImportInventoryWidget : public QWidget, private Ui::ImportInventoryWidget
{
    Q_OBJECT
public:
    ImportInventoryWidget(QWidget *parent = nullptr);

    void setItem(const BrickLink::Item *item);
    int quantity() const;
    void setQuantity(int qty);
    BrickLink::Condition condition() const;
    void setCondition(BrickLink::Condition condition);
    BrickLink::Status extraParts() const;
    BrickLink::PartOutTraits partOutTraits() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

protected:
    void changeEvent(QEvent *e) override;

    void languageChange();
};
