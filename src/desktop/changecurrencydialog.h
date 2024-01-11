// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "ui_changecurrencydialog.h"


class ChangeCurrencyDialog : public QDialog, private Ui::ChangeCurrencyDialog
{
    Q_OBJECT
public:
    explicit ChangeCurrencyDialog(const QString &from, const QString &to, bool wasLegacy = false,
                                  QWidget *parent = nullptr);

    double exchangeRate() const;

protected:
    void changeEvent(QEvent *e) override;

private slots:
    void currencyChanged(const QString &);
    void ratesUpdated();

private:
    void languageChange();

private:
    QString m_from;
    QString m_to;
    bool m_wasLegacy;
    double m_rate;
    QString m_labelEcbFormat;
    QString m_labelCustomFormat;
};

