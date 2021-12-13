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
