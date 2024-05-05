# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

include(FetchContent)

set(USE_QT6 ON)

FetchContent_Declare(
    qadwaitadecorations
    GIT_REPOSITORY https://github.com/FedoraQt/QAdwaitaDecorations.git
    GIT_TAG        0.1.5
)

FetchContent_GetProperties(qadwaitadecorations)
if (NOT qadwaitadecorations_POPULATED)
  FetchContent_Populate(qadwaitadecorations)

  set(mll ${CMAKE_MESSAGE_LOG_LEVEL})
  if (NOT VERBOSE_FETCH)
    set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
  endif()

  add_subdirectory(${qadwaitadecorations_SOURCE_DIR} ${qadwaitadecorations_BINARY_DIR})

  set(CMAKE_MESSAGE_LOG_LEVEL ${mll})
endif()
