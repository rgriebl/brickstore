# Copyright (C) 2004-2025 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

include(FetchContent)

FetchContent_Declare(
    sentry
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG        ${SENTRY_VERSION}
    SOURCE_SUBDIR  "NeedManualAddSubDir" # make it possible to add_subdirectory below
)

FetchContent_MakeAvailable(sentry)

set(mll ${CMAKE_MESSAGE_LOG_LEVEL})
if (NOT VERBOSE_FETCH)
  set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
endif()

# we need EXCLUDE_FROM_ALL to suppress the installation of sentry into the macOS bundle
# and Linux packages
add_subdirectory(${sentry_SOURCE_DIR} ${sentry_BINARY_DIR} EXCLUDE_FROM_ALL)

set(CMAKE_MESSAGE_LOG_LEVEL ${mll})
