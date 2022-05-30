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
#include <QtCore/QLoggingCategory>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlProperty>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuickTemplates2/private/qquickaction_p.h>
#include <QtQuickControls2Impl/private/qquickiconimage_p.h>
#ifdef Q_OS_ANDROID
#  include <QtSvg>  // because deployment sometimes just forgets to include this lib otherwise
#endif
#include <QDirIterator>

#include "common/actionmanager.h"
#include "common/config.h"
#include "ldraw/library.h"
#include "utility/undo.h"
#include "utility/utility.h"
#include "mobileapplication.h"
#include "mobileuihelpers.h"
#include "qmlimageitem.h"


MobileApplication::MobileApplication(int &argc, char **argv)
    : Application(argc, argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    (void) new QGuiApplication(argc, argv);

    const char *style =
    #if defined(Q_OS_IOS) && (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))
            "iOS";
    #elif defined(Q_OS_IOS)
            "Basic";
    #else
            "Material";
    #endif
    qputenv("QT_QUICK_CONTROLS_STYLE", style);
}

void MobileApplication::init()
{
    Application::init();

    m_engine->setBaseUrl(QUrl("qrc:/mobile/"_l1));

    MobileUIHelpers::create(m_engine);

    ActionManager::inst()->createAll([this](const ActionManager::Action *aa) {
        if (aa->isUndo())
            return Application::inst()->undoGroup()->createUndoAction(this);
        else if (aa->isRedo())
            return Application::inst()->undoGroup()->createRedoAction(this);
        else
            return new QAction(this);
    });

    setIconTheme(DarkTheme);

    m_engine->load(m_engine->baseUrl().resolved(QUrl("Main.qml"_l1)));

    connect(Config::inst(), &Config::uiThemeChanged, this, &MobileApplication::updateIconTheme);
    updateIconTheme();

    if (m_engine->rootObjects().isEmpty()) {
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }

    setUILoggingHandler([](QtMsgType, const QMessageLogContext &, const QString &) {
        // just ignore for now, but we need to set one
    });
}

void MobileApplication::updateIconTheme()
{
    auto roots = m_engine->rootObjects();
    if (roots.isEmpty())
        return;
    QObject *root = roots.constFirst();
    QQmlProperty theme(root, "Material.theme"_l1, qmlContext(root));

    setIconTheme(theme.read().toInt() == 1 ? DarkTheme : LightTheme);

    const auto icons = root->findChildren<QQuickIconImage *>();
    for (const auto &icon : icons) {
        QString name = icon->name();
        icon->setName("foo"_l1);
        icon->setName(name);
    }

}

QCoro::Task<bool> MobileApplication::closeAllViews()
{
    //TODO implement
    co_return true;
}

void MobileApplication::setupQml()
{
    Application::setupQml();

    qmlRegisterType<QmlImageItem>("BrickStore", 1, 0, "QImageItem");
}

MobileApplication::~MobileApplication()
{
    delete m_engine;
}

void MobileApplication::showToastMessage(const QString &message, int timeout)
{
    //TODO implement
}
