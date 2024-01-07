# Copyright (C) 2004-2023 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(scanner_module STATIC
    itemscanner.h
    itemscanner.cpp
)

qt_add_qml_module(scanner_module
    URI Scanner
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/Scanner
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    ENABLE_TYPE_COMPILER
    NO_PLUGIN
    RESOURCE_PREFIX /
    DEPENDENCIES
        QtQuick
        QtMultimedia

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
    )
endif()


target_link_libraries(scanner_module PRIVATE
    Qt6::Network
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaQuickPrivate
    Qt6::Qml
    Qt6::QmlPrivate
    Qt6::Quick
    Qt6::QuickPrivate
    QCoro6::Network
    QCoro6::Core
    bricklink_module
)

target_link_libraries(${PROJECT_NAME} PRIVATE scanner_module)
