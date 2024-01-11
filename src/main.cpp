// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdio>

#include <QtCore/QThreadPool>
#include <QtCore/QCoreApplication>
#if defined(Q_OS_UNIX)
#  include <sys/time.h>
#  include <sys/resource.h>
#endif

#if defined(BS_BACKEND)
#  include "backend/backendapplication.h"
#elif defined(BS_DESKTOP)
#  include "desktop/desktopapplication.h"
#elif defined(BS_MOBILE)
#  include "mobile/mobileapplication.h"
#endif


static void fixNumberOfOpenFiles()
{
#if defined(Q_OS_UNIX)
    static constexpr rlim_t noFile = 30000;
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {

        if (rlim.rlim_cur < noFile) {
            rlim_t soft = (rlim.rlim_max == RLIM_INFINITY) ? noFile
                                                           : std::min(rlim.rlim_max, noFile);
            if (soft > rlim.rlim_cur) {
                rlim.rlim_cur = soft;
                setrlimit(RLIMIT_NOFILE, &rlim);
            }
        }
    }
    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        fprintf(stderr, "\nCould not retrieve the number of files that are allowed to be opened at "
                        "the same time. This may lead to image loading problems when working on "
                        "large lists.\n\n");
    } else if (rlim.rlim_cur < noFile) {
        fprintf(stderr, "\nYour system only allows for %ld files to be opened at the same time. "
                        "This may lead to image loading problems when working on large lists.\n\n",
                static_cast<long>(rlim.rlim_cur));
    }
#endif
}

int main(int argc, char **argv)
{
#if defined(SANITIZER_ENABLED)
    QThreadPool::globalInstance()->setExpiryTimeout(0);
#endif
    int exitCode = 0;
    fixNumberOfOpenFiles();

#if defined(BS_BACKEND)
    BackendApplication a(argc, argv);
#elif defined(BS_DESKTOP)
    DesktopApplication a(argc, argv);
#elif defined(BS_MOBILE)
    MobileApplication a(argc, argv);
#endif
    a.init();
    a.afterInit();

    exitCode = qApp->exec();

    // we are using QThreadStorage in thread-pool threads, so we have to make sure they are
    // all joined before destroying the static QThreadStorage objects.
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();

    a.checkRestart();
    return exitCode;
}
