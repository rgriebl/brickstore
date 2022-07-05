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
#include <QGuiApplication>
#include <QScreen>
#include <QFontDatabase>
#include <QQmlProperty>
#include <QQmlEngine>
#include "common/config.h"
#include "common/application.h"
#include "qmlstyle.h"


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
    // 0: light, 1: dark, 2: system (see qquickmaterialstyle_p.h)
    return m_theme.isValid() ? (m_theme.read().toInt() == 1) : false;
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
    m_theme = QQmlProperty(root, u"Material.theme"_qs, qmlContext(root));

    emit styleColorChanged();

    m_theme.connectNotifySignal(this, SLOT(updateTheme()));
    connect(Config::inst(), &Config::uiThemeChanged,
            this, &QmlStyle::updateTheme);
    updateTheme();
}

void QmlStyle::updateTheme()
{
    // 0: light, 1: dark, 2: system (see qquickmaterialstyle_p.h)
    int materialTheme;

    switch (Config::inst()->uiTheme()) {
    case Config::UITheme::Light:         materialTheme = 0; break;
    case Config::UITheme::Dark:          materialTheme = 1; break;
    default:
    case Config::UITheme::SystemDefault: materialTheme = 2; break;
    }

    if (m_theme.read().toInt() != materialTheme)
        m_theme.write(materialTheme);

    Application::inst()->setIconTheme(darkTheme() ? Application::DarkTheme : Application::LightTheme);
}

QColor QmlStyle::colorProperty(const QQmlProperty &property, const char *fallbackColor) const
{
    return property.isValid() ? property.read().value<QColor>() : QColor { fallbackColor };
}
