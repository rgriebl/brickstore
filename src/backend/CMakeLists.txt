# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(backend_module STATIC
    backendapplication.h
    backendapplication.cpp
)

target_link_libraries(backend_module PRIVATE
    Qt6::Core
    Qt6::Gui
    QCoro6::Core
    bricklink_module
)

target_link_libraries(${PROJECT_NAME} PRIVATE backend_module)
