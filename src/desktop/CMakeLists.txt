# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(desktop_module STATIC
    aboutdialog.cpp
    aboutdialog.h
    aboutdialog.ui
    additemdialog.cpp
    additemdialog.h
    additemwidget.ui
    announcementsdialog.h
    announcementsdialog.cpp
    bettercommandbutton.cpp
    bettercommandbutton.h
    betteritemdelegate.cpp
    betteritemdelegate.h
    brickstoreproxystyle.cpp
    brickstoreproxystyle.h
    changecurrencydialog.cpp
    changecurrencydialog.h
    changecurrencydialog.ui
    checkforupdatesdialog.cpp
    checkforupdatesdialog.h
    consolidatedialog.cpp
    consolidatedialog.h
    consolidatedialog.ui
    desktopapplication.cpp
    desktopapplication.h
    desktopuihelpers.cpp
    desktopuihelpers_p.h
    desktopuihelpers.h
    developerconsole.cpp
    developerconsole.h
    documentdelegate.cpp
    documentdelegate.h
    expandbutton.cpp
    expandbutton.h
    filtertermwidget.cpp
    filtertermwidget.h
    flowlayout.cpp
    flowlayout.h
    headerview.cpp
    headerview.h
    historylineedit.cpp
    historylineedit.h
    importcartdialog.cpp
    importcartdialog.h
    importcartdialog.ui
    importinventorydialog.cpp
    importinventorydialog.h
    importinventorywidget.cpp
    importinventorywidget.h
    importinventorywidget.ui
    importorderdialog.cpp
    importorderdialog.h
    importorderdialog.ui
    importwantedlistdialog.cpp
    importwantedlistdialog.h
    importwantedlistdialog.ui
    incdecpricesdialog.cpp
    incdecpricesdialog.h
    inventorywidget.cpp
    inventorywidget.h
    itemscannerdialog.h
    itemscannerdialog.cpp
    mainwindow.cpp
    mainwindow.h
    managecolumnlayoutsdialog.cpp
    managecolumnlayoutsdialog.h
    menucombobox.cpp
    menucombobox.h
    orderinformationdialog.cpp
    orderinformationdialog.h
    orderinformationdialog.ui
    picturewidget.cpp
    picturewidget.h
    priceguidewidget.cpp
    priceguidewidget.h
    printdialog.cpp
    printdialog.h
    printdialog.ui
    progresscircle.cpp
    progresscircle.h
    rendersettingsdialog.cpp
    rendersettingsdialog.h
    rendersettingsdialog.ui
    selectcolor.cpp
    selectcolor.h
    selectcolordialog.cpp
    selectcolordialog.h
    selectdocumentdialog.cpp
    selectdocumentdialog.h
    selectitem.cpp
    selectitem.h
    selectitemdialog.cpp
    selectitemdialog.h
    selectmergemode.cpp
    selectmergemode.h
    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    settopriceguidedialog.cpp
    settopriceguidedialog.h
    settopriceguidedialog.ui
    smartvalidator.cpp
    smartvalidator.h
    systeminfodialog.cpp
    systeminfodialog.h
    systeminfodialog.ui
    taskwidgets.cpp
    taskwidgets.h
    tierpricesdialog.cpp
    tierpricesdialog.h
    view.cpp
    view.h
    view_p.h
    viewpane.cpp
    viewpane.h
    welcomewidget.cpp
    welcomewidget.h
)

target_link_libraries(desktop_module PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WidgetsPrivate
    Qt6::PrintSupport
    Qt6::Qml
    Qt6::Quick
    Qt6::Multimedia
    QCoro6::Network
    QCoro6::Core
    common_module
    bricklink_module
    ldraw_module
)

if (MACOS)
    target_sources(desktop_module PRIVATE
        desktopapplication_mac.mm
    )
    target_link_libraries(desktop_module PRIVATE
        "-framework AppKit"
    )
endif()

if (MODELTEST)
    target_link_libraries(desktop_module PRIVATE Qt6::Test)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE desktop_module)
