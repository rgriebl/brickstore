// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "bricklink/global.h"
#include "ui_settopriceguidedialog.h"

QT_FORWARD_DECLARE_CLASS(QButtonGroup)


class SetToPriceGuideDialog : public QDialog, private Ui::SetToPriceGuideDialog
{
    Q_OBJECT
public:
    SetToPriceGuideDialog(QWidget *parent = nullptr);
    ~SetToPriceGuideDialog() override;

    BrickLink::Time  time() const;
    BrickLink::Price price() const;
    BrickLink::NoPriceGuideOption noPriceGuideOption() const;
    bool forceUpdate() const;

private:
    QButtonGroup *m_noPgOptions;
};
