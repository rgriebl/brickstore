// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QLoggingCategory>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlProperty>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>

#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "common/undo.h"
#include "ldraw/library.h"
#include "mobileapplication.h"
#include "mobileuihelpers.h"
#include "common/brickstore_wrapper.h"

#if defined(Q_OS_ANDROID)
#  include <jni.h>
#  include <QJniObject>
#  include <QFileOpenEvent>

extern "C" JNIEXPORT void JNICALL
Java_de_brickforge_brickstore_ExtendedQtActivity_openUrl(JNIEnv *env, jobject, jstring jurl)
{
    jboolean isCopy = false;
    const char *utf8 = env->GetStringUTFChars(jurl, &isCopy);
    qWarning() << "opening" << utf8;
    QCoreApplication::postEvent(qApp, new QFileOpenEvent(QString::fromUtf8(utf8)));
    env->ReleaseStringUTFChars(jurl, utf8);
}

#endif


MobileApplication::MobileApplication(int &argc, char **argv)
    : Application(argc, argv)
{
    m_app = new QGuiApplication(argc, argv);

    qputenv("QT_QUICK_CONTROLS_CONF", ":/Mobile/qtquickcontrols2.conf");
}

void MobileApplication::init()
{
    Application::init();

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
        if (co_await UIHelpers::question(s) == UIHelpers::Yes) {
            const auto modes = DocumentModel::createFieldMergeModes(DocumentModel::MergeMode::MergeAverage);

            for (auto &consolidate : list) {
                consolidate.destinationIndex = 0;
                consolidate.doNotDeleteEmpty = false;
                consolidate.fieldMergeModes = modes;
            }
            co_return true;
        }
        co_return false;
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
