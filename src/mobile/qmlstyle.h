// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSizeF>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtQml/QQmlProperty>
#include <QtQml/QQmlEngine>

#include "common/application.h"


class QmlStylePrivate;

class QmlStyle : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Style)
    QML_SINGLETON

    Q_PROPERTY(QSizeF physicalScreenSize READ physicalScreenSize CONSTANT FINAL) // in mm
    Q_PROPERTY(bool smallSize READ smallSize NOTIFY smallSizeChanged FINAL) // smaller than 8cm x 12cm
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY darkThemeChanged FINAL)
    Q_PROPERTY(QFont monospaceFont READ monospaceFont CONSTANT FINAL)

    Q_PROPERTY(QColor hintTextColor               READ hintTextColor               NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor primaryHighlightedTextColor READ primaryHighlightedTextColor NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor primaryTextColor            READ primaryTextColor            NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor primaryColor                READ primaryColor                NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor textColor                   READ textColor                   NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor backgroundColor             READ backgroundColor             NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor accentTextColor             READ accentTextColor             NOTIFY styleColorChanged FINAL)
    Q_PROPERTY(QColor accentColor                 READ accentColor                 NOTIFY styleColorChanged FINAL)

    Q_PROPERTY(bool isIOS READ isIOS CONSTANT)
    Q_PROPERTY(bool isAndroid READ isAndroid CONSTANT)

    Q_PROPERTY(int topScreenMargin READ topScreenMargin NOTIFY screenMarginsChanged FINAL)
    Q_PROPERTY(int bottomScreenMargin READ bottomScreenMargin NOTIFY screenMarginsChanged FINAL)
    Q_PROPERTY(int leftScreenMargin READ leftScreenMargin NOTIFY screenMarginsChanged FINAL)
    Q_PROPERTY(int rightScreenMargin READ rightScreenMargin NOTIFY screenMarginsChanged FINAL)

    // this is really a write-once only property, but QML does not like these
    Q_PROPERTY(QObject *rootWindow READ rootWindow WRITE setRootWindow NOTIFY rootWindowChanged FINAL)

public:
    QmlStyle(QObject *parent = nullptr);

    QSizeF physicalScreenSize() const;
    bool smallSize() const;
    bool darkTheme() const;
    QFont monospaceFont() const;
    QColor accentColor() const;
    QColor accentTextColor() const;
    QColor backgroundColor() const;
    QColor textColor() const;
    QColor primaryColor() const;
    QColor primaryTextColor() const;
    QColor primaryHighlightedTextColor() const;
    QColor hintTextColor() const;

    bool isIOS() const;
    bool isAndroid() const;
    int topScreenMargin() const;
    int bottomScreenMargin() const;
    int leftScreenMargin() const;
    int rightScreenMargin() const;

    QObject *rootWindow() const;
    void setRootWindow(QObject *root);

signals:
    void smallSizeChanged(bool newSmallSize);
    void darkThemeChanged(bool newDarkTheme);
    void styleColorChanged();
    void screenMarginsChanged();
    void rootWindowChanged(); // dummy, never emitted

private:
    void updateTheme();
    void setScreenMargins(const QMargins &newMargins);
    QColor colorProperty(const QQmlProperty &property, const char *fallbackColor) const;

    Application::Theme m_theme;
    QPointer<QObject> m_root;
    bool m_smallSize = false;

    QQmlProperty m_hintTextColor;
    QQmlProperty m_primaryHighlightedTextColor;
    QQmlProperty m_primaryTextColor;
    QQmlProperty m_primaryColor;
    QQmlProperty m_textColor;
    QQmlProperty m_backgroundColor;
    QQmlProperty m_accentTextColor;
    QQmlProperty m_accentColor;
    QQmlProperty m_materialTheme;

    QMargins m_screenMargins;
    qreal m_screenDpr = qreal(1);

    friend void androidSetScreenMargins(const QMargins &margins);
};
