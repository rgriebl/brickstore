// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QFileOpenEvent>
#include <QCoreApplication>
#include "mobilefileopenhandler.h"

#if defined(Q_OS_IOS)
#  include <QDesktopServices>
#endif
#if defined(Q_OS_ANDROID)
#  include <jni.h>
#  include <QJniObject>

class MobileFileOpenHandlerPrivate
{
public:
    static void deferredOpenUrl(const QUrl &url)
    {
        if (MobileFileOpenHandler::s_inst) {
            QMetaObject::invokeMethod(MobileFileOpenHandler::s_inst, [=]() {
                    MobileFileOpenHandler::s_inst->openUrl(url);
                }, Qt::QueuedConnection);
        } else {
            QMutexLocker locker(&s_mutex);
            s_deferred.append(url);
        }
    }

    static void flushDeferred()
    {
        QMutexLocker locker(&s_mutex);
        for (const auto &url : std::as_const(s_deferred))
            MobileFileOpenHandler::s_inst->openUrl(url);
        s_deferred.clear();
    }

    static QList<QUrl> s_deferred;
    static QMutex s_mutex;
};

QList<QUrl> MobileFileOpenHandlerPrivate::s_deferred;
QMutex MobileFileOpenHandlerPrivate::s_mutex;

extern "C" JNIEXPORT void JNICALL
Java_de_brickforge_brickstore_ExtendedQtActivity_openUrl(JNIEnv *env, jobject, jstring jurl)
{
   jboolean isCopy = false;
   const char *utf8 = env->GetStringUTFChars(jurl, &isCopy);
   MobileFileOpenHandlerPrivate::deferredOpenUrl(QString::fromUtf8(utf8));
   env->ReleaseStringUTFChars(jurl, utf8);
}

#endif


MobileFileOpenHandler *MobileFileOpenHandler::s_inst = nullptr;

MobileFileOpenHandler *MobileFileOpenHandler::create()
{
    Q_ASSERT(!s_inst);
    Q_ASSERT(qApp);
    return s_inst = new MobileFileOpenHandler(qApp);
}

MobileFileOpenHandler::MobileFileOpenHandler(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_IOS)
    QDesktopServices::setUrlHandler(u"file"_qs, this, "openUrl");
#endif

#if defined(Q_OS_ANDROID)
    MobileFileOpenHandlerPrivate::flushDeferred();
#endif
}

MobileFileOpenHandler::~MobileFileOpenHandler()
{
#if defined(Q_OS_IOS)
    QDesktopServices::unsetUrlHandler(u"file"_qs);
#endif
    s_inst = nullptr;
}

void MobileFileOpenHandler::openUrl(const QUrl &url)
{
    Q_ASSERT(qApp);
    QCoreApplication::postEvent(qApp, new QFileOpenEvent(url));
}
