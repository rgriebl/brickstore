// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QProxyStyle>

class BrickStoreProxyStyle : public QProxyStyle
{
public:
    BrickStoreProxyStyle(QStyle *baseStyle = nullptr);

    void polish(QWidget *w) override;
    void unpolish(QWidget *w) override;

    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;

    void drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled,
                      const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const override;

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement elem, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override;
    void drawControl(ControlElement element, const QStyleOption *opt, QPainter *p,
                     const QWidget *widget) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget) const override;

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool m_isWindowsVistaStyle = false;
};
