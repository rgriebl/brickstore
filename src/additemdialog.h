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

#include <QWidget>
#include <QPointer>

#include "ui_additemdialog.h"
#include "currency.h"
#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QValidator)
QT_FORWARD_DECLARE_CLASS(QPushButton)
class Window;


class AddItemDialog : public QWidget, private Ui::AddItemDialog
{
    Q_OBJECT
public:
    AddItemDialog(QWidget *parent = nullptr);
    ~AddItemDialog() override;

    void attach(Window *window);

signals:
    void closed();

protected slots:
    void languageChange();

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateCaption();
    void updateCurrencyCode();
    void updateItemAndColor();
    void showTotal();
    bool checkAddPossible();
    void addClicked();
    void checkTieredPrices();
    void setTierType(int type);
    void setSimpleMode(bool);

private:
    void showItemInColor(const BrickLink::Item *it, const BrickLink::Color *col);
    double tierPriceValue(int i);

private:
    QPointer<Window> m_window;

    QPushButton *w_add;
    QSpinBox *w_tier_qty [3];
    QLineEdit *w_tier_price [3];

    QButtonGroup *m_tier_type;
    QButtonGroup *m_condition;

    QValidator *m_money_validator;
    QValidator *m_percent_validator;

    QString m_caption_fmt;
    QString m_price_label_fmt;

    QString m_currency_code;

    QCheckBox *w_toggles[3];
};
