
configure_file(version.h.in generated/version.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_sources(${PROJECT_NAME} PUBLIC
    main.cpp
    rebuilddatabase.cpp
    rebuilddatabase.h
    ref.cpp
    ref.h
    version.h.in

    ldraw/ldraw.h
    ldraw/ldraw.cpp

    bricklink/bricklink.h
    bricklink/bricklink.cpp
    bricklink/bricklinkfwd.h
    bricklink/bricklink_data.cpp
    bricklink/bricklink_textimport.cpp
    bricklink/bricklink_picture.cpp
    bricklink/bricklink_priceguide.cpp

    script/bricklink_wrapper.cpp
    script/bricklink_wrapper.h
    script/printjob.cpp
    script/printjob.h
    script/script.cpp
    script/script.h
    script/scriptmanager.cpp
    script/scriptmanager.h

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
    utility/stopwatch.h
    utility/systeminfo.cpp
    utility/systeminfo.h
    utility/transfer.cpp
    utility/transfer.h
    utility/utility.cpp
    utility/utility.h
    utility/workspace.cpp
    utility/workspace.h
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

if (NOT BACKEND_ONLY)
    target_sources(${PROJECT_NAME} PUBLIC
        aboutdialog.cpp
        aboutdialog.h
        aboutdialog.ui
        additemdialog.cpp
        additemdialog.h
        additemdialog.ui
        announcements.cpp
        announcements.h
        announcements_p.h
        appearsinwidget.cpp
        appearsinwidget.h
        application.cpp
        application.h
        changecurrencydialog.cpp
        changecurrencydialog.h
        changecurrencydialog.ui
        checkforupdates.cpp
        checkforupdates.h
        config.cpp
        config.h
        consolidateitemsdialog.cpp
        consolidateitemsdialog.h
        consolidateitemsdialog.ui
        document.cpp
        document.h
        document_p.h
        documentdelegate.cpp
        documentdelegate.h
        documentio.cpp
        documentio.h
        framework.cpp
        framework.h
        importcartdialog.cpp
        importcartdialog.h
        importcartdialog.ui
        importinventorydialog.cpp
        importinventorydialog.h
        importinventorydialog.ui
        importorderdialog.cpp
        importorderdialog.h
        importorderdialog.ui
        incdecpricesdialog.cpp
        incdecpricesdialog.h
        lot.cpp
        lot.h
        managecolumnlayoutsdialog.cpp
        managecolumnlayoutsdialog.h
        picturewidget.cpp
        picturewidget.h
        priceguidewidget.cpp
        priceguidewidget.h
        printdialog.cpp
        printdialog.h
        printdialog.ui
        selectcolor.cpp
        selectcolor.h
        selectcolordialog.cpp
        selectcolordialog.h
        selectcolordialog.ui
        selectdocumentdialog.cpp
        selectdocumentdialog.h
        selectitem.cpp
        selectitem.h
        selectitemdialog.cpp
        selectitemdialog.h
        selectitemdialog.ui
        settingsdialog.cpp
        settingsdialog.h
        settingsdialog.ui
        settopriceguidedialog.cpp
        settopriceguidedialog.h
        settopriceguidedialog.ui
        systeminfodialog.cpp
        systeminfodialog.h
        systeminfodialog.ui
        taskwidgets.cpp
        taskwidgets.h
        updatedatabase.cpp
        updatedatabase.h
        view.cpp
        view.h
        view_p.h
        welcomewidget.cpp
        welcomewidget.h

        ldraw/renderwidget.h
        ldraw/renderwidget.cpp
        ldraw/shaders.h

        bricklink/bricklink_model.cpp
        bricklink/bricklink_model.h

        utility/betteritemdelegate.cpp
        utility/betteritemdelegate.h
        utility/currency.cpp
        utility/currency.h
        utility/filter.cpp
        utility/filter.h
        utility/flowlayout.cpp
        utility/flowlayout.h
        utility/headerview.cpp
        utility/headerview.h
        utility/historylineedit.cpp
        utility/historylineedit.h
        utility/humanreadabletimedelta.cpp
        utility/humanreadabletimedelta.h
        utility/messagebox.cpp
        utility/messagebox.h
        utility/progresscircle.cpp
        utility/progresscircle.h
        utility/progressdialog.cpp
        utility/progressdialog.h
        utility/smartvalidator.cpp
        utility/smartvalidator.h
        utility/staticpointermodel.cpp
        utility/staticpointermodel.h
        utility/undo.cpp
        utility/undo.h

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

if(WIN32)
    target_compile_definitions(${PROJECT_NAME}
        BS_HAS_PARALLEL_STL
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::WinExtras
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
