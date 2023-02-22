# Copyright (C) 2004-2023 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(mobile_module STATIC
    mobileapplication.h
    mobileapplication.cpp
    mobileuihelpers.h
    mobileuihelpers_p.h
    mobileuihelpers.cpp
    qmlimageitem.h
    qmlimageitem.cpp
    qmlstyle.h
    qmlstyle.cpp
)

target_link_libraries(mobile_module PRIVATE
    Qt6::QuickPrivate
    Qt6::QuickDialogs2Private
    Qt6::QuickControls2ImplPrivate
    Qt6::QuickTemplates2Private
    Qt6::Svg # force deployment
    QCoro6::Core
    bricklink_module
    common_module
)

if (${Qt6_VERSION} VERSION_GREATER_EQUAL "6.3.0")
    target_link_libraries(mobile_module PRIVATE Qt6::QuickDialogs2UtilsPrivate)
endif()

set_source_files_properties(Style.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(mobile_module
    URI Mobile
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/imports/Mobile
    IMPORT_PATH ${CMAKE_BINARY_DIR}/imports
    NO_PLUGIN
    RESOURCE_PREFIX /
    #ENABLE_TYPE_COMPILER
    DEPENDENCIES
        QtQuick.Dialogs
        BrickStore
        BrickLink
        LDraw
    IMPORTS
        QtQml
        QtQuick
        QtQuick.Layouts
        ${QUICK_CONTROLS}

    QML_FILES
        AboutDialog.qml
        ActionDelegate.qml
        ActionMenuItem.qml
        ActionMenuSeparator.qml
        AnnouncementsDialog.qml
        AppearsInWidget.qml
        AutoSizingMenu.qml
        AutoSizingDialog.qml
        CheckButton.qml
        CornerTag.qml
        DialogLoader.qml
        DeveloperConsole.qml
        GridCell.qml
        GridHeader.qml
        IconComboBox.qml
        ImportCartDialog.qml
        ImportInventoryDialog.qml
        ImportInventoryWidget.qml
        ImportOrderDialog.qml
        ImportWantedListDialog.qml
        IncDecPricesDialog.qml
        InfoWidget.qml
        Main.qml
        MainWindow.qml
        PriceGuideWidget.qml
        ScrollableLayout.qml
        SelectColor.qml
        SelectionInfoDialog.qml
        SettingsDialog.qml
        SetToPriceGuideDialog.qml
        SystemInfoDialog.qml
        Tracer.qml
        View.qml
        ViewEditMenu.qml
        ViewHeaderMenu.qml
        InputDialog.qml
        ProgressDialog.qml
        ToastMessage.qml
        utils.js

    RESOURCES
        qtquickcontrols2.conf
)

if (CLANG)
    set_source_files_properties(
        ${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/mobile_module_SettingsDialog_qml.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/mobile_module_View_qml.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-shorten-64-to-32
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE mobile_module)
