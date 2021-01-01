/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include "ui_changecurrencydialog.h"


class ChangeCurrencyDialog : public QDialog, private Ui::ChangeCurrencyDialog
{
    Q_OBJECT
public:
    explicit ChangeCurrencyDialog(const QString &from, const QString &to, QWidget *parent = nullptr);

    double exchangeRate() const;

protected:
    bool eventFilter(QObject *, QEvent *) override;
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
    QString m_labelEcbFormat;
    QString m_labelCustomFormat;
};

