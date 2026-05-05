# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

include(FetchContent)

set(USE_QT6 ON)

FetchContent_Declare(
    qadwaitadecorations
    GIT_REPOSITORY https://github.com/FedoraQt/QAdwaitaDecorations.git
    GIT_TAG        f40f31dadc074bd989db6dd90e52eecf33c4567b  # 0.1.5 + border fix
    SOURCE_SUBDIR  "NeedManualAddSubDir" # make it possible to add_subdirectory below
)

FetchContent_MakeAvailable(qadwaitadecorations)

set(mll ${CMAKE_MESSAGE_LOG_LEVEL})
if (NOT VERBOSE_FETCH)
  set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
endif()

add_subdirectory(${qadwaitadecorations_SOURCE_DIR} ${qadwaitadecorations_BINARY_DIR})

set(CMAKE_MESSAGE_LOG_LEVEL ${mll})
