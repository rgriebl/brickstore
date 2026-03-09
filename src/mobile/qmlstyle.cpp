// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QScreen>
#include <QStyleHints>
#include <QQmlApplicationEngine>
#include "common/config.h"
#include "common/application.h"
#include "qmlstyle.h"


QmlStyle *QmlStyle::s_inst = nullptr;

QmlStyle::QmlStyle(QObject *parent)
    : QObject(parent)
{
    auto calculateSmallSize = [this](Config::UISize uiSize) {
        const auto scrSize = physicalScreenSize();
        return ((scrSize.width() < 80) || (scrSize.height() < 120) || (uiSize == Config::UISize::Small))
                && (uiSize != Config::UISize::Large);
    };

    m_smallSize = calculateSmallSize(Config::inst()->mobileUISize());

    connect(Config::inst(), &Config::mobileUISizeChanged,
            this, [this, &calculateSmallSize](Config::UISize uiSize) {
        auto newSmallSize = calculateSmallSize(uiSize);
        if (newSmallSize != m_smallSize) {
            m_smallSize = newSmallSize;
            emit smallSizeChanged(newSmallSize);
        }
    });

    s_inst = this;
}

QmlStyle::~QmlStyle()
{
    s_inst = nullptr;
}

QSizeF QmlStyle::physicalScreenSize() const
{
    auto s = QGuiApplication::primaryScreen()->physicalSize();
    if (s.width() > s.height())
        s.transpose();
    return s;
}

bool QmlStyle::smallSize() const
{
    return m_smallSize;
}

bool QmlStyle::darkTheme() const
{
    return m_theme == Application::DarkTheme;
}

QFont QmlStyle::monospaceFont() const
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

QColor QmlStyle::accentColor() const
{
    return colorProperty(m_accentColor, "darkred");
}

QColor QmlStyle::accentTextColor() const
{
    return colorProperty(m_accentTextColor, "white");
}

QColor QmlStyle::backgroundColor() const
{
    return colorProperty(m_backgroundColor, "white");
}

QColor QmlStyle::textColor() const
{
    return colorProperty(m_textColor, "black");
}

QColor QmlStyle::primaryColor() const
{
    return colorProperty(m_primaryColor, "darkgreen");
}

QColor QmlStyle::primaryTextColor() const
{
    return colorProperty(m_primaryTextColor, "black");
}

QColor QmlStyle::primaryHighlightedTextColor() const
{
    return colorProperty(m_primaryHighlightedTextColor, "white");
}

QColor QmlStyle::hintTextColor() const
{
    return colorProperty(m_hintTextColor, "gray");
}

bool QmlStyle::isIOS() const
{
#if defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

bool QmlStyle::isAndroid() const
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

QObject *QmlStyle::rootWindow() const
{
    return m_root;
}

void QmlStyle::setRootWindow(QObject *root)
{
    Q_ASSERT(!m_root);

    m_root = root;

    m_hintTextColor = QQmlProperty(root, u"Material.hintTextColor"_qs, qmlContext(root));
    m_primaryHighlightedTextColor = QQmlProperty(root, u"Material.primaryHighlightedTextColor"_qs, qmlContext(root));
    m_primaryTextColor = QQmlProperty(root, u"Material.primaryTextColor"_qs, qmlContext(root));
    m_primaryColor = QQmlProperty(root, u"Material.primaryColor"_qs, qmlContext(root));
    m_textColor = QQmlProperty(root, u"Material.foreground"_qs, qmlContext(root));
    m_backgroundColor = QQmlProperty(root, u"Material.backgroundColor"_qs, qmlContext(root));
    m_accentTextColor = QQmlProperty(root, u"Material.primaryHighlightedTextColor"_qs, qmlContext(root));
    m_accentColor = QQmlProperty(root, u"Material.accentColor"_qs, qmlContext(root));
    m_materialTheme = QQmlProperty(root, u"Material.theme"_qs, qmlContext(root));

    emit styleColorChanged();

    //m_materialTheme.connectNotifySignal(this, SLOT(updateTheme()));
    connect(Config::inst(), &Config::uiThemeChanged,
            this, &QmlStyle::updateTheme);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &QmlStyle::updateTheme);
    updateTheme();
}

void QmlStyle::updateTheme()
{
    Application::Theme theme;
    bool systemIsDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

    switch (Config::inst()->uiTheme()) {
    case Config::UITheme::Light:         theme = Application::LightTheme; break;
    case Config::UITheme::Dark:          theme = Application::DarkTheme; break;
    default:
    case Config::UITheme::SystemDefault: theme = systemIsDark ? Application::DarkTheme
                                                              : Application::LightTheme; break;
    }

    // 0: light, 1: dark, 2: system (see qquickmaterialstyle_p.h)
    int materialThemeEnum = (theme == Application::DarkTheme) ? 1 : 0;
    if (m_materialTheme.read().toInt() != materialThemeEnum)
        m_materialTheme.write(materialThemeEnum);

    Application::inst()->setIconTheme(theme);

    if (theme != m_theme) {
        m_theme = theme;
        emit darkThemeChanged(theme == Application::DarkTheme);
    }
}

QColor QmlStyle::colorProperty(const QQmlProperty &property, const char *fallbackColor) const
{
    return property.isValid() ? property.read().value<QColor>() : QColor { fallbackColor };
}
