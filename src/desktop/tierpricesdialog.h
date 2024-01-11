// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)


class TierPricesDialog : public QDialog
{
    Q_OBJECT
public:
    TierPricesDialog(QWidget *parent = nullptr);

    std::array<double, 3> percentagesOff() const;

private:
    void checkValues();

    std::array<QSpinBox *, 3> m_spinBoxes;
    QDialogButtonBox *m_buttons;
};
