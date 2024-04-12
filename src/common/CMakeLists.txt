# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(common_module STATIC
    actionmanager.cpp
    actionmanager.h
    announcements.cpp
    announcements.h
    application.cpp
    application.h
    config.cpp
    config.h
    document.cpp
    document.h
    documentio.cpp
    documentio.h
    documentlist.cpp
    documentlist.h
    documentmodel.cpp
    documentmodel.h
    documentmodel_p.h
    onlinestate.cpp
    onlinestate.h
    recentfiles.cpp
    recentfiles.h
    sentryinterface.h
    sentryinterface.cpp
    systeminfo.cpp
    systeminfo.h

    uihelpers.cpp
    uihelpers.h

    brickstore_wrapper.cpp
    brickstore_wrapper_p.h
    brickstore_wrapper.h
    printjob.cpp
    printjob.h
    script.cpp
    script.h
    scriptmanager.cpp
    scriptmanager.h

    currency.cpp
    currency.h
    eventfilter.cpp
    eventfilter.h
    filter.cpp
    filter.h
    humanreadabletimedelta.cpp
    humanreadabletimedelta.h
    undo.cpp
    undo.h
)

target_link_libraries(common_module PRIVATE
    Qt6::Qml
    Qt6::Concurrent
    Qt6::Xml
    Qt6::Quick
    Qt6::QuickPrivate
    Qt6::Svg # force deployment
    QCoro6::Network
    QCoro6::Core
    QCoro6::Qml
    bricklink_module
)

if (BS_DESKTOP)
    target_link_libraries(common_module PRIVATE
        Qt6::Widgets
        Qt6::PrintSupport
    )
endif()

if (BS_MOBILE)
    target_link_libraries(common_module PRIVATE
        Qt6::QuickControls2ImplPrivate
        Qt6::QuickTemplates2Private
    )
endif()

if (MACOS)
    target_link_libraries(common_module PRIVATE "-framework SystemConfiguration")
    target_sources(common_module PRIVATE
        application_mac.mm
    )
endif()

if (SENTRY AND NOT ANDROID)
    target_link_libraries(common_module PRIVATE sentry::sentry)
endif()

if (MODELTEST)
    target_link_libraries(common_module PRIVATE Qt6::Test)
endif()

qt_add_qml_module(common_module
    URI BrickStore
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/BrickStore
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    NO_PLUGIN
    RESOURCE_PREFIX /
    IMPORTS
        BrickLink
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
        BrickLink
)

target_link_libraries(${PROJECT_NAME} PRIVATE common_module)
