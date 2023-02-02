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

#if defined(Q_OS_ANDROID)
#  include <jni.h>
#  include <QJniObject>

// WindowManager.LayoutParams
#  define FLAG_TRANSLUCENT_STATUS 0x04000000
#  define FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS 0x80000000
// View
#  define SYSTEM_UI_FLAG_LIGHT_STATUS_BAR 0x00002000

static bool darkThemeOS_value;

extern "C" JNIEXPORT void JNICALL
Java_de_brickforge_brickstore_ExtendedQtActivity_changeUiTheme(JNIEnv *, jobject, jboolean jisDark)
{
    darkThemeOS_value = jisDark;
}

static bool darkThemeOS()
{
    return darkThemeOS_value;
}

static void setStatusBarColor(const QColor &color)
{
    if (QNativeInterface::QAndroidApplication::sdkVersion() < 23)
        return;
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        window.callMethod<void>("addFlags", "(I)V", FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.callMethod<void>("clearFlags", "(I)V", FLAG_TRANSLUCENT_STATUS);
        QJniObject view = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        int visibility = view.callMethod<int>("getSystemUiVisibility", "()I");
        visibility &= ~SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        view.callMethod<void>("setSystemUiVisibility", "(I)V", visibility);

        window.callMethod<void>("setStatusBarColor", "(I)V", color.rgba());
    });
}

#else
#  if defined(Q_CC_MSVC)
#    pragma warning(push)
#    pragma warning(disable: 4458)
#    pragma warning(disable: 4201)
#  endif
#  include <QtGui/private/qguiapplication_p.h>
#  if defined(Q_CC_MSVC)
#    pragma warning(pop)
#  endif
#  include <QtGui/qpa/qplatformtheme.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
namespace Qt { typedef QPlatformTheme::Appearance Appearance; }
#endif

static bool darkThemeOS()
{
    if (const QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme())
        return (theme->appearance() == Qt::Appearance::Dark);
    return false;
}

static void setStatusBarColor(const QColor &color)
{
    Q_UNUSED(color)
}

#endif


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

    connect(QGuiApplication::primaryScreen(), &QScreen::geometryChanged,
            this, &QmlStyle::screenMarginsChanged);
    connect(QGuiApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, &QmlStyle::screenMarginsChanged);
    connect(QGuiApplication::primaryScreen(), &QScreen::orientationChanged,
            this, &QmlStyle::screenMarginsChanged);
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

int QmlStyle::topScreenMargin() const
{
    if (isIOS()) {
        const auto scr = QGuiApplication::primaryScreen();
        return scr->availableGeometry().y();
    } else {
        return 0;
    }
}

int QmlStyle::bottomScreenMargin() const
{
    if (isIOS()) {
        const auto scr = QGuiApplication::primaryScreen();
        return scr->geometry().bottom() - scr->availableGeometry().bottom();
    } else {
        return 0;
    }
}

int QmlStyle::leftScreenMargin() const
{
    int lsm = 0;

    if (isIOS()) {
        const auto scr = QGuiApplication::primaryScreen();
        lsm = (scr->orientation() == Qt::InvertedLandscapeOrientation)
                ? 0 : scr->availableGeometry().x();
    }
    //qWarning() << "Left screen margin:" << lsm;
    return lsm;
}

int QmlStyle::rightScreenMargin() const
{
    int rsm = 0;

    if (isIOS()) {
        const auto scr = QGuiApplication::primaryScreen();
        rsm = (scr->orientation() == Qt::LandscapeOrientation)
                ? 0 : (scr->geometry().right() - scr->availableGeometry().right());
    }
    //qWarning() << "Right screen margin:" << rsm;
    return rsm;
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
    case Config::UITheme::SystemDefault: materialTheme = (darkThemeOS() ? 1 : 0); break;
    }

    setStatusBarColor(m_primaryColor.read().value<QColor>());

    if (m_theme.read().toInt() != materialTheme)
        m_theme.write(materialTheme);

    Application::inst()->setIconTheme(darkTheme() ? Application::DarkTheme : Application::LightTheme);
}

QColor QmlStyle::colorProperty(const QQmlProperty &property, const char *fallbackColor) const
{
    return property.isValid() ? property.read().value<QColor>() : QColor { fallbackColor };
}
