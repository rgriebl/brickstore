// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
