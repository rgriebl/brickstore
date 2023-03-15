# Copyright (C) 2004-2023 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(bricklink_module STATIC
    category.h
    category.cpp
    changelogentry.h
    color.h
    color.cpp
    core.h
    core.cpp
    database.h
    database.cpp
    global.h
    item.h
    item.cpp
    itemtype.h
    itemtype.cpp
    lot.h
    lot.cpp
    partcolorcode.h
    partcolorcode.cpp
    picture.h
    picture_p.h
    picture.cpp
    priceguide.h
    priceguide_p.h
    priceguide.cpp
    relationship.h
    relationship.cpp
)

if (NOT BS_BACKEND)
    qt_add_qml_module(bricklink_module
        URI BrickLink
        VERSION 1.0
        OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/BrickLink
        IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
        NO_PLUGIN
        RESOURCE_PREFIX /
        DEPENDENCIES
            QtQml
    )

    target_link_libraries(bricklink_module PRIVATE
        Qt6::Qml
    )
endif()

target_link_libraries(bricklink_module PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    Qt6::Concurrent
    Qt6::Sql
    QCoro6::Network
    QCoro6::Core
)

if (BS_BACKEND)
    target_sources(${PROJECT_NAME} PRIVATE
        textimport.h
        textimport.cpp
    )
endif()

if (BS_DESKTOP OR BS_MOBILE)
    target_sources(bricklink_module PRIVATE
        cart.h
        cart.cpp
        io.h
        io.cpp
        model.h
        model_p.h
        model.cpp
        order.h
        order.cpp
        qmlapi.h
        qmlapi.cpp
        staticpointermodel.h
        staticpointermodel.cpp
        store.h
        store.cpp
        wantedlist.h
        wantedlist.cpp
    )
endif()

if (BS_DESKTOP)
    target_sources(bricklink_module PRIVATE
        delegate.h
        delegate.cpp
    )
    target_link_libraries(bricklink_module PRIVATE
        Qt6::Widgets
    )
endif()

if (BRICKLINK_AFFILIATE_API_KEY)
    set_source_files_properties(priceguide.cpp PROPERTIES COMPILE_DEFINITIONS
        BS_BRICKLINK_AFFILIATE_API_KEY=${BRICKLINK_AFFILIATE_API_KEY}
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE bricklink_module)
