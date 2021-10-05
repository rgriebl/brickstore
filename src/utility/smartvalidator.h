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

#include <QString>
#include <QValidator>

QT_FORWARD_DECLARE_CLASS(QRegularExpressionValidator)


class SmartDoubleValidator : public QDoubleValidator
{
    Q_OBJECT

public:
    SmartDoubleValidator(QObject *parent);
    SmartDoubleValidator(double bottom, double top, int decimals, double empty, QObject *parent);

protected:
    QValidator::State validate(QString &input, int &pos) const override;
    void fixup(QString &str) const override;
    bool event(QEvent *event) override;

private:
    double m_empty;
    QRegularExpressionValidator *m_regexp;
};


class SmartIntValidator : public QIntValidator
{
    Q_OBJECT

public:
    SmartIntValidator(QObject *parent);
    SmartIntValidator(int bottom, int top, int empty, QObject *parent);

protected:
    QValidator::State validate(QString &input, int &pos) const override;
    void fixup(QString &str) const override;
    bool event(QEvent *event) override;

private:
    int m_empty;
    QRegularExpressionValidator *m_regexp;
};


class DotCommaFilter : public QObject
{
public:
    static void install();

private:
    explicit DotCommaFilter(QObject *parent);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
};
