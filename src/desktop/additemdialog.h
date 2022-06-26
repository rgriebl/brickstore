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

#include <QWidget>
#include <QPointer>
#include <QDateTime>

#include "ui_additemdialog.h"
#include "common/currency.h"
#include "bricklink/core.h"

QT_FORWARD_DECLARE_CLASS(QValidator)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTimer)

class View;


class AddItemDialog : public QWidget, private Ui::AddItemDialog
{
    Q_OBJECT
public:
    AddItemDialog(QWidget *parent = nullptr);
    ~AddItemDialog() override;

    void attach(View *view);

signals:
    void closed();

protected slots:
    void languageChange();

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void updateCaption();
    void updateCurrencyCode();
    void updateItemAndColor();
    bool checkAddPossible();
    void addClicked();
    void checkTieredPrices();
    void setTierType(int type);
    void setSellerMode(bool);

private:
    double tierPriceValue(int i);
    void updateHistoryText();
    static QString historyTextFor(const QDateTime &when, const Lot &lot);

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

private:
    QPointer<View> m_view;

    QPushButton *w_add;
    QSpinBox *w_tier_qty[3];
    QDoubleSpinBox *w_tier_price[3];

    QButtonGroup *m_tier_type;
    QButtonGroup *m_condition;

    QString m_caption_fmt;
    QString m_price_label_fmt;

    QString m_currency_code;

    QToolButton *w_toggles[3];

    QTimer *m_historyTimer;
    std::list<QPair<QDateTime, const Lot>> m_addHistory;
};
