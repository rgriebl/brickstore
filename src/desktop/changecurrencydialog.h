// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "ui_changecurrencydialog.h"


class ChangeCurrencyDialog : public QDialog, private Ui::ChangeCurrencyDialog
{
    Q_OBJECT
public:
    explicit ChangeCurrencyDialog(const QString &from, QWidget *parent = nullptr);

    QString currencyCode() const;
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
    double m_rate;
    QString m_labelProviderFormat;
    QString m_labelCustomFormat;
};

