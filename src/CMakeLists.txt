
configure_file(version.h.in generated/version.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_sources(${PROJECT_NAME} PUBLIC
    main.cpp
    version.h.in

    bricklink/category.h
    bricklink/changelogentry.cpp
    bricklink/changelogentry.h
    bricklink/color.cpp
    bricklink/color.h
    bricklink/core.cpp
    bricklink/core.h
    bricklink/database.cpp
    bricklink/database.h
    bricklink/global.h
    bricklink/item.cpp
    bricklink/item.h
    bricklink/itemtype.cpp
    bricklink/itemtype.h
    bricklink/lot.cpp
    bricklink/lot.h
    bricklink/partcolorcode.cpp
    bricklink/partcolorcode.h
    bricklink/picture.cpp
    bricklink/picture.h
    bricklink/priceguide.cpp
    bricklink/priceguide.h
    bricklink/textimport.cpp
    bricklink/textimport.h

    utility/chunkreader.cpp
    utility/chunkreader.h
    utility/chunkwriter.h
    utility/eventfilter.h
    utility/exception.cpp
    utility/exception.h
    utility/q3cache.h
    utility/q3cache6.h
    utility/q5hashfunctions.cpp
    utility/q5hashfunctions.h
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
    Qt6::Concurrent
)

if (IOS)
    # really only useful for iOS: restrict the number of deployed plugins in static builds
    qt_import_plugins(${PROJECT_NAME}
        EXCLUDE_BY_TYPE qmltooling
        INCLUDE_BY_TYPE imageformats
            Qt::QGifPlugin
            Qt::QJpegPlugin
            Qt::QSvgPlugin
            Qt::QJpegPlugin
            Qt::QJpegPlugin
    )
endif()

if (BS_BACKEND)
    target_sources(${PROJECT_NAME} PUBLIC
        backend/backendapplication.cpp
        backend/backendapplication.h
        backend/rebuilddatabase.cpp
        backend/rebuilddatabase.h
    )
endif()

if (BS_DESKTOP OR BS_MOBILE)
    target_sources(${PROJECT_NAME} PUBLIC
        bricklink/cart.cpp
        bricklink/cart.h
        bricklink/io.cpp
        bricklink/io.h
        bricklink/model.cpp
        bricklink/model.h
        bricklink/order.cpp
        bricklink/order.h
        bricklink/qmlapi.cpp
        bricklink/qmlapi.h
        bricklink/store.cpp
        bricklink/store.h
        bricklink/wantedlist.cpp
        bricklink/wantedlist.h

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
        common/documentio.cpp
        common/documentio.h
        common/documentlist.cpp
        common/documentlist.h
        common/documentmodel.cpp
        common/documentmodel.h
        common/documentmodel_p.h
        common/onlinestate.cpp
        common/onlinestate.h
        common/recentfiles.cpp
        common/recentfiles.h
        common/uihelpers.cpp
        common/uihelpers.h

        ldraw/library.cpp
        ldraw/library.h
        ldraw/part.cpp
        ldraw/part.h
        ldraw/rendercontroller.cpp
        ldraw/rendercontroller.h
        ldraw/rendergeometry.cpp
        ldraw/rendergeometry.h
        ldraw/rendersettings.cpp
        ldraw/rendersettings.h

        qmlapi/brickstore_wrapper.cpp
        qmlapi/brickstore_wrapper.h

        utility/currency.cpp
        utility/currency.h
        utility/filter.cpp
        utility/filter.h
        utility/humanreadabletimedelta.cpp
        utility/humanreadabletimedelta.h
        utility/qparallelsort.h
        utility/staticpointermodel.cpp
        utility/staticpointermodel.h
        utility/undo.cpp
        utility/undo.h
    )

    qt_add_resources(${PROJECT_NAME} ldraw_qrc
        PREFIX "/"
        FILES
            ldraw/PartRenderer.qml
            ldraw/lightbox.hdr
            ldraw/shaders/custom-line.vert
            ldraw/shaders/custom-line.frag
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Qml
        Qt6::Quick
        Qt6::Quick3D
    )
    if (MACOS)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework SystemConfiguration"
        )
    endif()

    # not useable due to QTBUG-XXXX
    #qt_add_qml_module(${PROJECT_NAME}
    #    URI BrickStore
    #    VERSION 1.0
    #)

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
        desktop/betteritemdelegate.cpp
        desktop/betteritemdelegate.h
        desktop/brickstoreproxystyle.cpp
        desktop/brickstoreproxystyle.h
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
        desktop/desktopuihelpers.cpp
        desktop/desktopuihelpers_p.h
        desktop/desktopuihelpers.h
        desktop/developerconsole.cpp
        desktop/developerconsole.h
        desktop/documentdelegate.cpp
        desktop/documentdelegate.h
        desktop/filtertermwidget.cpp
        desktop/filtertermwidget.h
        desktop/flowlayout.cpp
        desktop/flowlayout.h
        desktop/headerview.cpp
        desktop/headerview.h
        desktop/historylineedit.cpp
        desktop/historylineedit.h
        desktop/importcartdialog.cpp
        desktop/importcartdialog.h
        desktop/importcartdialog.ui
        desktop/importinventorydialog.cpp
        desktop/importinventorydialog.h
        desktop/importinventorywidget.cpp
        desktop/importinventorywidget.h
        desktop/importinventorywidget.ui
        desktop/importorderdialog.cpp
        desktop/importorderdialog.h
        desktop/importorderdialog.ui
        desktop/importwantedlistdialog.cpp
        desktop/importwantedlistdialog.h
        desktop/importwantedlistdialog.ui
        desktop/incdecpricesdialog.cpp
        desktop/incdecpricesdialog.h
        desktop/mainwindow.cpp
        desktop/mainwindow.h
        desktop/managecolumnlayoutsdialog.cpp
        desktop/managecolumnlayoutsdialog.h
        desktop/menucombobox.cpp
        desktop/menucombobox.h
        desktop/orderinformationdialog.cpp
        desktop/orderinformationdialog.h
        desktop/orderinformationdialog.ui
        desktop/picturewidget.cpp
        desktop/picturewidget.h
        desktop/priceguidewidget.cpp
        desktop/priceguidewidget.h
        desktop/printdialog.cpp
        desktop/printdialog.h
        desktop/printdialog.ui
        desktop/printjob.cpp
        desktop/printjob.h
        desktop/progresscircle.cpp
        desktop/progresscircle.h
        desktop/progressdialog.cpp
        desktop/progressdialog.h
        desktop/rendersettingsdialog.cpp
        desktop/rendersettingsdialog.h
        desktop/rendersettingsdialog.ui
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
        desktop/smartvalidator.cpp
        desktop/smartvalidator.h
        desktop/systeminfodialog.cpp
        desktop/systeminfodialog.h
        desktop/systeminfodialog.ui
        desktop/taskwidgets.cpp
        desktop/taskwidgets.h
        desktop/view.cpp
        desktop/view.h
        desktop/view_p.h
        desktop/viewpane.cpp
        desktop/viewpane.h
        desktop/welcomewidget.cpp
        desktop/welcomewidget.h

        ldraw/renderwidget.h
        ldraw/renderwidget.cpp
        )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets
        Qt6::WidgetsPrivate
        Qt6::PrintSupport
    )

    if (APPLE)
        target_sources(${PROJECT_NAME} PUBLIC
            desktop/desktopapplication_mac.mm
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework AppKit"
        )
    endif()
endif()


if (BS_MOBILE)
    target_sources(${PROJECT_NAME} PUBLIC
        mobile/mobileapplication.cpp
        mobile/mobileapplication.h
        mobile/mobileuihelpers.cpp
        mobile/mobileuihelpers.h
        mobile/qmlimageitem.cpp
        mobile/qmlimageitem.h
    )

    set(QML_FILES
        mobile/qmldir
        mobile/qtquickcontrols2.conf
        mobile/+ios/qtquickcontrols2.conf
        mobile/AboutDialog.qml
        mobile/ActionDelegate.qml
        mobile/AnnouncementsDialog.qml
        mobile/AppearsInWidget.qml
        mobile/AutoSizingMenu.qml
        mobile/BrickStoreDialog.qml
        mobile/CornerTag.qml
        mobile/DialogLoader.qml
        mobile/DocumentProxyModelJS.qml
        mobile/GridCell.qml
        mobile/GridHeader.qml
        mobile/ImportCartDialog.qml
        mobile/ImportInventoryDialog.qml
        mobile/ImportOrderDialog.qml
        mobile/ImportWantedListDialog.qml
        mobile/InfoWidget.qml
        mobile/Main.qml
        mobile/PriceGuideWidget.qml
        mobile/ScrollableLayout.qml
        mobile/SelectionInfoDialog.qml
        mobile/SettingsDialog.qml
        mobile/SetToPriceGuideDialog.qml
        mobile/Style.qml
        mobile/SystemInfoDialog.qml
        mobile/View.qml
        mobile/ViewEditMenu.qml
        mobile/ViewHeaderMenu.qml
        mobile/uihelpers/InputDialog.qml
        mobile/uihelpers/ProgressDialog.qml
        mobile/uihelpers/ToastMessage.qml
        mobile/utils.js
    )
    qt_add_resources(${PROJECT_NAME} mobile_qrc
        PREFIX "/"
        BASE   "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES  "${QML_FILES}"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::QuickPrivate
        Qt6::QuickDialogs2Private
        Qt6::QuickControls2ImplPrivate
        Qt6::QuickTemplates2Private
    )
    if (${Qt6_VERSION} VERSION_GREATER_EQUAL "6.3.0")
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::QuickDialogs2UtilsPrivate)
    endif()
endif()
