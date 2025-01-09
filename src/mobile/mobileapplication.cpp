// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QLoggingCategory>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlProperty>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/private/qquickflickable_p.h>

#include "common/actionmanager.h"
#include "common/document.h"
#include "common/undo.h"
#include "common/eventfilter.h"
#include "mobileapplication.h"
#include "mobileuihelpers.h"
#include "mobilefileopenhandler.h"
#include "common/brickstore_wrapper.h"

#if defined(Q_OS_ANDROID)
#  include <android/api-level.h>
#  if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
#    include <qpa/qwindowsysteminterface.h>
#  endif

static struct DisableA11YOnAndroid  // clazy:exclude=non-pod-global-static
{
    // QTBUG-xxxxxx: Accessibility crashes in TableView
    DisableA11YOnAndroid()
    {
        qputenv("QT_ANDROID_DISABLE_ACCESSIBILITY", "1");
    }
} disableA11YOnAndroid;

#elif defined(Q_OS_IOS)
#  include <QtGui/QAccessible>
#endif


MobileApplication::MobileApplication(int &argc, char **argv)
    : Application(argc, argv)
{
    m_app = new QGuiApplication(argc, argv);

    qputenv("QT_QUICK_CONTROLS_CONF", ":/Mobile/qtquickcontrols2.conf");

#if defined(Q_OS_ANDROID) && (QT_VERSION < QT_VERSION_CHECK(6, 7, 0)) // QTBUG-118421
    const auto inputDevices = QInputDevice::devices();
    bool vkbdFound = false;
    for (const auto *ip : inputDevices) {
        if ((ip->type() == QInputDevice::DeviceType::Keyboard)
            && (ip->name() == u"Virtual keyboard"_qs)
            && (ip->seatName().isEmpty())) {
            vkbdFound = true;
            break;
        }
    }
    if (!vkbdFound) {
        qInfo() << "Qt is missing the virtual keyboard input device, registering it manually.";
        QWindowSystemInterface::registerInputDevice(
            new QInputDevice(u"Virtual keyboard"_qs, 0, QInputDevice::DeviceType::Keyboard,
                             {}, qApp));
    }
#endif
#if defined(Q_OS_IOS)
    // QML's TableView crashes when empty, if a11y features are enabled
    // (e.g. "Full Keyboard Access" in the simulator)
    QAccessible::installUpdateHandler([](QAccessibleEvent *) { });
    QAccessible::installRootObjectHandler([](QObject *) { });
#endif
}

void MobileApplication::init()
{
    Application::init();

    MobileFileOpenHandler::create();

#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)) && (QT_VERSION <= QT_VERSION_CHECK(6, 7, 0))
    // the scrolling speed is broken in Qt 6.6+
    auto dpr = qApp->primaryScreen()->devicePixelRatio();
    qInfo() << "Fixing QML Flickable speed with a factor of" << dpr;
    new EventFilter(qApp, { QEvent::ChildAdded }, [dpr](QObject *o, QEvent *) {
        static QSet<QObject *> seen;
        if (auto flickable = qobject_cast<QQuickFlickable *>(o)) {
            if (!seen.contains(o)) {
                seen.insert(o);
                connect(o, &QObject::destroyed, qApp, [](QObject *dead) { seen.remove(dead); });
                flickable->setMaximumFlickVelocity(flickable->maximumFlickVelocity() * dpr);
            }
        }
        return EventFilter::ContinueEventProcessing;
    });
#endif

    // add all relevant QML modules here
    extern void qml_register_types_Mobile(); qml_register_types_Mobile();

    MobileUIHelpers::create(m_engine);

    setIconTheme(LightTheme);

    ActionManager::inst()->createAll([this](const ActionManager::Action *aa) {
        if (aa->isUndo())
            return Application::inst()->undoGroup()->createUndoAction(this);
        else if (aa->isRedo())
            return Application::inst()->undoGroup()->createRedoAction(this);
        else
            return new QAction(this);
    });

    DocumentModel::setConsolidateFunction([](DocumentModel *model, QVector<DocumentModel::Consolidate> &list, bool addItems) -> QCoro::Task<bool> {
        auto *doc = DocumentList::inst()->documentForModel(model);
        Q_UNUSED(addItems)

        if (!doc || !MobileApplication::inst())
            co_return false;

        emit doc->requestActivation();

        QString s = tr("Would you like to consolidate %L1 lots?").arg(list.count());
        bool doConsolidate = (co_await UIHelpers::question(s) == UIHelpers::Yes);
        const auto modes = DocumentModel::createFieldMergeModes(DocumentModel::MergeMode::MergeAverage);

        for (auto &consolidate : list) {
            consolidate.destinationIndex = doConsolidate ? 0 : -1;
            consolidate.doNotDeleteEmpty = false;
            consolidate.fieldMergeModes = modes;
        }
        co_return true;
    });

    m_engine->load(QUrl(u"Mobile/Main.qml"_qs));

    if (m_engine->rootObjects().isEmpty()) {
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }
    setMainWindow(qobject_cast<QWindow *>(m_engine->rootObjects().constFirst()));
}

void MobileApplication::setupLogging()
{
    Application::setupLogging();

    setUILoggingHandler([](const UILogMessage &lm) {
        QmlDebugLogModel::inst()->append(std::get<0>(lm), std::get<1>(lm), std::get<2>(lm),
                                         std::get<3>(lm), std::get<4>(lm));
    });
}

QCoro::Task<bool> MobileApplication::closeAllDocuments()
{
    return Application::closeAllDocuments();
}

void MobileApplication::setupQml()
{
    Application::setupQml();
}

MobileApplication::~MobileApplication()
{
    delete m_engine;
}
