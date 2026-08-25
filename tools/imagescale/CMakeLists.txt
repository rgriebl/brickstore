# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Buildable standalone on purpose: the cross builds (iOS, Android) have to build
# this against QT_HOST_PATH, because a target binary cannot run on the machine
# that is generating the icons.
#
#   cmake -S tools/imagescale -B <builddir> -DCMAKE_PREFIX_PATH=<host-qt>
#   cmake --build <builddir>

cmake_minimum_required(VERSION 3.19.0)
project(imagescale LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)

add_executable(imagescale imagescale.cpp)
target_link_libraries(imagescale PRIVATE Qt6::Core Qt6::Gui)
target_compile_definitions(imagescale PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

# there is no Q_OBJECT in here, and as a subdirectory of the app this would
# otherwise inherit the app's CMAKE_AUTOMOC
set_target_properties(imagescale PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
