
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
)

if (BS_DESKTOP)
    target_sources(ldraw_module PRIVATE
        renderwidget.h
        renderwidget.cpp
    )
    target_link_libraries(ldraw_module PRIVATE
        Qt6::Widgets
    )
endif()

qt_add_qml_module(ldraw_module
    URI LDraw
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/LDraw
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    #ENABLE_TYPE_COMPILER
    DEPENDENCIES
        QtQuick
        QtQuick3D

    QML_FILES
	PartRenderer.qml

    RESOURCES
        lightbox.hdr
        shaders/custom-line.vert
        shaders/custom-line.frag
)

target_link_libraries(${PROJECT_NAME} PRIVATE ldraw_moduleplugin)
