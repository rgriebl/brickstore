
configure_file(version.h.in generated/version.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_sources(${PROJECT_NAME} PUBLIC
    version.h.in

    bricklink/cart.cpp
    bricklink/cart.h
    bricklink/category.cpp
    bricklink/category.h
    bricklink/changelogentry.cpp
    bricklink/changelogentry.h
    bricklink/color.cpp
    bricklink/color.h
    bricklink/core.cpp
    bricklink/core.h
    bricklink/global.h
    bricklink/io.cpp
    bricklink/io.h
    bricklink/item.cpp
    bricklink/item.h
    bricklink/itemtype.cpp
    bricklink/itemtype.h
    bricklink/lot.cpp
    bricklink/lot.h
    bricklink/order.cpp
    bricklink/order.h
    bricklink/partcolorcode.cpp
    bricklink/partcolorcode.h
    bricklink/picture.cpp
    bricklink/picture.h
    bricklink/priceguide.cpp
    bricklink/priceguide.h
    bricklink/store.cpp
    bricklink/store.h
    bricklink/textimport.cpp
    bricklink/textimport.h
    bricklink/updatedatabase.cpp
    bricklink/updatedatabase.h

    common/main.cpp
    common/onlinestate.cpp
    common/onlinestate.h

    ldraw/ldraw.h
    ldraw/ldraw.cpp

    utility/chunkreader.cpp
    utility/chunkreader.h
    utility/chunkwriter.h
    utility/exception.cpp
    utility/exception.h
    utility/q3cache.h
    utility/q3cache6.h
    utility/q5hashfunctions.cpp
    utility/q5hashfunctions.h
    utility/qparallelsort.h
    utility/ref.cpp
    utility/ref.h
    utility/stopwatch.h
    utility/systeminfo.cpp
    utility/systeminfo.h
    utility/transfer.cpp
    utility/transfer.h
    utility/utility.cpp
    utility/utility.h
    utility/xmlhelpers.cpp
    utility/xmlhelpers.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    Qt6::Network
    Qt6::CorePrivate
    Qt6::GuiPrivate
    Qt6::Concurrent
)

if (BS_BACKEND)
    target_sources(${PROJECT_NAME} PUBLIC
        backend/rebuilddatabase.cpp
        backend/rebuilddatabase.h
    )
endif()

if (BS_DESKTOP OR BS_MOBILE)
    target_sources(${PROJECT_NAME} PUBLIC
        bricklink/model.cpp
        bricklink/model.h

        common/actionmanager.cpp
        common/actionmanager.h
        common/announcements.cpp
        common/announcements.h
        common/application.cpp
        common/application.h
        common/config.cpp
        common/config.h
        common/document.cpp
        common/document.h
        common/document_p.h
        common/documentio.cpp
        common/documentio.h

        qmlapi/bricklink_wrapper.cpp
        qmlapi/bricklink_wrapper.h
        qmlapi/brickstore_wrapper.cpp
        qmlapi/brickstore_wrapper.h
        qmlapi/common.cpp
        qmlapi/common.h

        utility/currency.cpp
        utility/currency.h
        utility/filter.cpp
        utility/filter.h
        utility/humanreadabletimedelta.cpp
        utility/humanreadabletimedelta.h
        utility/staticpointermodel.cpp
        utility/staticpointermodel.h
        utility/undo.cpp
        utility/undo.h
    )
endif()

if (BS_DESKTOP)
    target_sources(${PROJECT_NAME} PUBLIC
        bricklink/delegate.cpp
        bricklink/delegate.h

        desktop/aboutdialog.cpp
        desktop/aboutdialog.h
        desktop/aboutdialog.ui
        desktop/additemdialog.cpp
        desktop/additemdialog.h
        desktop/additemdialog.ui
        desktop/announcementsdialog.h
        desktop/announcementsdialog.cpp
        desktop/appearsinwidget.cpp
        desktop/appearsinwidget.h
        desktop/changecurrencydialog.cpp
        desktop/changecurrencydialog.h
        desktop/changecurrencydialog.ui
        desktop/checkforupdates.cpp
        desktop/checkforupdates.h
        desktop/consolidateitemsdialog.cpp
        desktop/consolidateitemsdialog.h
        desktop/consolidateitemsdialog.ui
        desktop/desktopapplication.cpp
        desktop/desktopapplication.h
        desktop/documentdelegate.cpp
        desktop/documentdelegate.h
        desktop/framework.cpp
        desktop/framework.h
        desktop/importcartdialog.cpp
        desktop/importcartdialog.h
        desktop/importcartdialog.ui
        desktop/importinventorydialog.cpp
        desktop/importinventorydialog.h
        desktop/importinventorydialog.ui
        desktop/importorderdialog.cpp
        desktop/importorderdialog.h
        desktop/importorderdialog.ui
        desktop/incdecpricesdialog.cpp
        desktop/incdecpricesdialog.h
        desktop/managecolumnlayoutsdialog.cpp
        desktop/managecolumnlayoutsdialog.h
        desktop/picturewidget.cpp
        desktop/picturewidget.h
        desktop/priceguidewidget.cpp
        desktop/priceguidewidget.h
        desktop/printdialog.cpp
        desktop/printdialog.h
        desktop/printdialog.ui
        desktop/printjob.cpp
        desktop/printjob.h
        desktop/script.cpp
        desktop/script.h
        desktop/scriptmanager.cpp
        desktop/scriptmanager.h
        desktop/selectcolor.cpp
        desktop/selectcolor.h
        desktop/selectcolordialog.cpp
        desktop/selectcolordialog.h
        desktop/selectcolordialog.ui
        desktop/selectdocumentdialog.cpp
        desktop/selectdocumentdialog.h
        desktop/selectitem.cpp
        desktop/selectitem.h
        desktop/selectitemdialog.cpp
        desktop/selectitemdialog.h
        desktop/selectitemdialog.ui
        desktop/settingsdialog.cpp
        desktop/settingsdialog.h
        desktop/settingsdialog.ui
        desktop/settopriceguidedialog.cpp
        desktop/settopriceguidedialog.h
        desktop/settopriceguidedialog.ui
        desktop/systeminfodialog.cpp
        desktop/systeminfodialog.h
        desktop/systeminfodialog.ui
        desktop/taskwidgets.cpp
        desktop/taskwidgets.h
        desktop/view.cpp
        desktop/view.h
        desktop/view_p.h
        desktop/welcomewidget.cpp
        desktop/welcomewidget.h
        desktop/workspace.cpp
        desktop/workspace.h

        ldraw/renderwidget.h
        ldraw/renderwidget.cpp
        ldraw/shaders.h

        utility/betteritemdelegate.cpp
        utility/betteritemdelegate.h
        utility/flowlayout.cpp
        utility/flowlayout.h
        utility/headerview.cpp
        utility/headerview.h
        utility/historylineedit.cpp
        utility/historylineedit.h
        utility/messagebox.cpp
        utility/messagebox.h
        utility/progresscircle.cpp
        utility/progresscircle.h
        utility/progressdialog.cpp
        utility/progressdialog.h
        utility/smartvalidator.cpp
        utility/smartvalidator.h
        )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Qml
        Qt6::Quick
        Qt6::Concurrent
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        )
endif()


if (BS_MOBILE)
    target_sources(${PROJECT_NAME} PUBLIC
        mobile/mobileapplication.cpp
        mobile/mobileapplication.h c
        )

    set(QML_FILES
        mobile/qtquickcontrols2.conf
        mobile/main.qml
        mobile/View.qml
        mobile/MessageBox.qml
        mobile/SettingsDialog.qml
        mobile/GridCell.qml
        )
    qt_add_resources(${PROJECT_NAME} mobile_qrc
        PREFIX "mobile"
        BASE   "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES  "${QML_FILES}"
        )
endif()

if(WIN32)
    add_compile_definitions(BS_HAS_PARALLEL_STL)
    target_link_libraries(${PROJECT_NAME} PRIVATE
#        Qt6::WinExtras
        Qt6::Widgets
    )
elseif(APPLE)
    target_sources(${PROJECT_NAME} PUBLIC
        application_mac.mm
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework SystemConfiguration"
        "-framework AppKit"
    )
elseif(UNIX)
    # TBB stuff
endif()
