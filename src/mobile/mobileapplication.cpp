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
#include <QtQml/QQmlExtensionPlugin>
#include <QtSvg>  // because deployment sometimes just forgets to include this lib otherwise

#include "common/actionmanager.h"
#include "common/config.h"
#include "common/undo.h"
#include "ldraw/library.h"
#include "mobileapplication.h"
#include "mobileuihelpers.h"


// add all QML plugins here for the iOS port (static linking only)
Q_IMPORT_QML_PLUGIN(MobilePlugin)
Q_IMPORT_QML_PLUGIN(LDrawPlugin)
Q_IMPORT_QML_PLUGIN(BrickLinkPlugin)
Q_IMPORT_QML_PLUGIN(BrickStorePlugin)


MobileApplication::MobileApplication(int &argc, char **argv)
    : Application(argc, argv)
{
    m_app = new QGuiApplication(argc, argv);

    qputenv("QT_QUICK_CONTROLS_CONF", ":/Mobile/qtquickcontrols2.conf");
}

void MobileApplication::init()
{
    Application::init();

    if (qEnvironmentVariableIntValue("SHOW_TRACER") == 1)
        m_engine->rootContext()->setContextProperty(u"showTracer"_qs, true);

    MobileUIHelpers::create(m_engine);

    ActionManager::inst()->createAll([this](const ActionManager::Action *aa) {
        if (aa->isUndo())
            return Application::inst()->undoGroup()->createUndoAction(this);
        else if (aa->isRedo())
            return Application::inst()->undoGroup()->createRedoAction(this);
        else
            return new QAction(this);
    });

    setIconTheme(LightTheme);

    m_engine->load(QUrl(u"Mobile/Main.qml"_qs));

    if (m_engine->rootObjects().isEmpty()) {
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }

    setUILoggingHandler([](QtMsgType, const QMessageLogContext &, const QString &) {
        // just ignore for now, but we need to set one
    });
}

QCoro::Task<bool> MobileApplication::closeAllViews()
{
    return Application::closeAllViews();
}

void MobileApplication::setupQml()
{
    Application::setupQml();
}

MobileApplication::~MobileApplication()
{
    delete m_engine;
}
