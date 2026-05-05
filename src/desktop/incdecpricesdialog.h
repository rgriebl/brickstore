// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QCheckBox)


class IncDecPricesDialog : public QDialog
{
    Q_OBJECT
public:
    IncDecPricesDialog(const QString &text, bool showTiers, const QString &currencyCode,
                       QWidget *parent = nullptr);

    bool isFixed() const;
    double value() const;
    bool applyToTiers() const;

private slots:
    void switchType(int type);
    
private:
    QDoubleSpinBox *m_value;
    QButtonGroup *m_percentOrFixed;
    QCheckBox *m_applyToTiers = nullptr;
    QString m_currencyCode;
};
