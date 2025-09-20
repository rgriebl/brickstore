# Copyright (C) 2004-2025 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(scanner_module STATIC
    core.h
    core.cpp
    capture.h
    capture.cpp
    qmlapi.h
    qmlapi.cpp
)

qt_add_qml_module(scanner_module
    URI Scanner
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/Scanner
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    NO_PLUGIN
    RESOURCE_PREFIX /
    IMPORTS
        BrickLink
    DEPENDENCIES
        QtQuick
        QtMultimedia
        BrickLink

    QML_FILES
        CameraPreview.qml

    RESOURCES
        service_brickognize.png
        media-playback-start.svg
)

if (BS_DESKTOP)
    target_sources(scanner_module PRIVATE
        camerapreviewwidget.h
        camerapreviewwidget.cpp
    )
    target_link_libraries(scanner_module PRIVATE
        Qt6::Widgets
        Qt6::QuickWidgets
    )
endif()

if (IOS AND (${Qt6_VERSION} VERSION_GREATER_EQUAL "6.8.0"))
    qt_add_ios_ffmpeg_libraries(${PROJECT_NAME})
endif()

target_link_libraries(scanner_module PRIVATE
    Qt6::Network
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Qml
    Qt6::Quick
    QCoro6::Network
    QCoro6::Core
    bricklink_module
)

target_link_libraries(${PROJECT_NAME} PRIVATE scanner_module)
