# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

include(FetchContent)

set(QCORO_BUILD_EXAMPLES OFF)
set(QCORO_ENABLE_ASAN ${SANITIZE})
set(QCORO_DISABLE_DEPRECATED_TASK_H  ON)
set(BUILD_TESTING OFF)

FetchContent_Declare(
    sentry
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG        ${SENTRY_VERSION}
    SOURCE_SUBDIR  "NeedManualAddSubDir"
)

# we need EXCLUDE_FROM_ALL to suppress the installation of sentry into the macOS bundle
# and Linux packages

FetchContent_MakeAvailable(sentry)
FetchContent_GetProperties(sentry)

set(mll ${CMAKE_MESSAGE_LOG_LEVEL})
if (NOT VERBOSE_FETCH)
  set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
endif()

add_subdirectory(${sentry_SOURCE_DIR} ${sentry_BINARY_DIR} EXCLUDE_FROM_ALL)

set(CMAKE_MESSAGE_LOG_LEVEL ${mll})
