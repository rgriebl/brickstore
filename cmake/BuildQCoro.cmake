# Copyright (C) 2004-2024 Robert Griebl
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
    GIT_TAG        v${QCORO_VERSION}
    SOURCE_SUBDIR  "NeedManualAddSubDir" # make it possible to add_subdirectory below
)

FetchContent_MakeAvailable(qcoro)

set(mll ${CMAKE_MESSAGE_LOG_LEVEL})
if (NOT VERBOSE_FETCH)
  set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
endif()

# we need EXCLUDE_FROM_ALL to suppress the installation of qcoro into the macOS bundle
# and Linux packages
add_subdirectory(${qcoro_SOURCE_DIR} ${qcoro_BINARY_DIR} EXCLUDE_FROM_ALL)

set(CMAKE_MESSAGE_LOG_LEVEL ${mll})
