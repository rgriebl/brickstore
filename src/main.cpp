// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QThreadPool>
#include <QtCore/QCoreApplication>

#if defined(BS_BACKEND)
#  include "backend/backendapplication.h"
using ApplicationType = BackendApplication;
#elif defined(BS_DESKTOP)
#  include "desktop/desktopapplication.h"
using ApplicationType = DesktopApplication;
#elif defined(BS_MOBILE)
#  include "mobile/mobileapplication.h"
using ApplicationType = MobileApplication;
#endif

#if defined(BS_LIBSTDCPP_USES_TBB)
#  include <oneapi/tbb/global_control.h>
#endif


int main(int argc, char **argv)
{
#if defined(SANITIZER_ENABLED)
    QThreadPool::globalInstance()->setExpiryTimeout(0);
#endif
#if defined(BS_LIBSTDCPP_USES_TBB)
    oneapi::tbb::task_scheduler_handle tbb { tbb::attach { } };
#endif
    ApplicationType a(argc, argv);
    a.init();
    a.afterInit();

    int exitCode = a.exec();

    // We are using QThreadStorage in thread-pool threads, so we have to make sure they are
    // all joined before destroying the static QThreadStorage objects.
    // This goes for QThreadPool and also the libstdc++/libtbb thread-pool on Linux.
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();
#if defined(BS_LIBSTDCPP_USES_TBB)
    oneapi::tbb::finalize(tbb);
#endif

    a.checkRestart();
    return exitCode;
}
