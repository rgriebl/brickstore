# Copyright (C) 2004-2023 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

include(FetchContent)

set(QCORO_BUILD_EXAMPLES  OFF)
set(QCORO_ENABLE_ASAN ${SANITIZE})
set(QCORO_DISABLE_DEPRECATED_TASK_H ON)
set(BUILD_TESTING OFF)
set(QCORO_WITH_QTWEBSOCKETS OFF)
set(QCORO_WITH_QTQUICK OFF)
set(QCORO_WITH_QTDBUS OFF)
if (BACKEND_ONLY)
    set(QCORO_WITH_QML OFF)
endif()

FetchContent_Declare(
    qcoro
    GIT_REPOSITORY https://github.com/danvratil/qcoro.git
    GIT_TAG        34d7f2f2d2f85804beb2b7e0da8faa5987c280c3 # needed for the iOS fix
    #GIT_TAG        v${QCORO_VERSION}
)
FetchContent_MakeAvailable(qcoro)
