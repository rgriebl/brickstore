// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "bricklink/order.h"
#include "ui_orderinformationdialog.h"


class OrderInformationDialog : public QDialog, private Ui::OrderInformationDialog
{
    Q_OBJECT

public:
    explicit OrderInformationDialog(const BrickLink::Order *order, QWidget *parent = nullptr);

private:
};
