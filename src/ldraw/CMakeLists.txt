# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(ldraw_module STATIC
    library.h
    library.cpp
    part.h
    part.cpp
    rendercontroller.h
    rendercontroller.cpp
    rendergeometry.h
    rendergeometry.cpp
    rendersettings.h
    rendersettings.cpp
)

target_link_libraries(ldraw_module PRIVATE
    Qt6::Concurrent
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    QCoro6::Network
    QCoro6::Core
    bricklink_module
)

if (BS_DESKTOP)
    target_sources(ldraw_module PRIVATE
        renderwidget.h
        renderwidget.cpp
    )
    target_link_libraries(ldraw_module PRIVATE
        Qt6::Widgets
        Qt6::QuickWidgets
    )
endif()

# This doesn't work on CI runs, because the converter (balsam) needs a working QPA
# We need to convert and include the ktx-es manually (see RESOURCES below)
#qt6_add_lightprobe_images(ldraw_module "lightprobe"
#    PREFIX
#        "/LDraw"
#    FILES
#        "textures/lightbox.hdr"
#)


qt_add_qml_module(ldraw_module
    URI LDraw
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/LDraw
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    #ENABLE_TYPE_COMPILER
    NO_PLUGIN
    RESOURCE_PREFIX /
    DEPENDENCIES
        QtQuick
        QtQuick3D

    QML_FILES
	PartRenderer.qml

    RESOURCES
        textures/lightbox.ktx
        shaders/custom-line.vert
        shaders/custom-line.frag
)

target_link_libraries(${PROJECT_NAME} PRIVATE ldraw_module)
