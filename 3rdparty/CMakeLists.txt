
target_sources(${PROJECT_NAME} PUBLIC
    lzma/bs_lzma.h
    lzma/bs_lzma.cpp
    lzma/lzmadec.h
    lzma/lzmadec.c
)


if (NOT BS_BACKEND)
    target_sources(${PROJECT_NAME} PUBLIC
        minizip/crypt.h
        minizip/ioapi.c
        minizip/ioapi.h
        minizip/minizip.cpp
        minizip/minizip.h
        minizip/unzip.c
        minizip/unzip.h
        minizip/zlib_p.h
    )

    if(WIN32)
        target_sources(${PROJECT_NAME} PUBLIC
            minizip/iowin32.c
            minizip/iowin32.h
        )
    else()
        find_package(ZLIB REQUIRED)

        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${ZLIB_LIBRARIES}
        )
    endif()


    target_sources(${PROJECT_NAME} PUBLIC
        qtdiag/qtdiag.h
        qtdiag/qtdiag.cpp
    )
    if (VULKAN_FOUND)
        set_source_files_properties(qtdiag/qtdiag.cpp
            TARGET_DIRECTORY ${PROJECT_NAME}
            PROPERTIES COMPILE_DEFINITIONS HAS_VULKAN)
    endif()
endif()


target_sources(${PROJECT_NAME} PUBLIC
    qcoro/concepts_p.h
    qcoro/qcorocore.h
    qcoro/qcorofuture.h
    qcoro/qcoroiodevice.cpp
    qcoro/qcoroiodevice.h
    qcoro/qcoroiodevice_p.cpp
    qcoro/qcoroiodevice_p.h
    qcoro/qcorosignal.h
    qcoro/qcorothread.cpp
    qcoro/qcorothread.h
    qcoro/qcorotimer.cpp
    qcoro/qcorotimer.h
    qcoro/coroutine.h
    qcoro/macros_p.h
    qcoro/qcoroabstractsocket.cpp
    qcoro/qcoroabstractsocket.h
    qcoro/qcorolocalsocket.cpp
    qcoro/qcorolocalsocket.h
    qcoro/qcoronetwork.h
    qcoro/qcoronetworkreply.cpp
    qcoro/qcoronetworkreply.h
    qcoro/qcorotcpserver.cpp
    qcoro/qcorotcpserver.h
    qcoro/qcoro.h
    qcoro/task.h
    qcoro/waitoperationbase_p.h
)

if (NOT IOS)
    target_sources(${PROJECT_NAME} PUBLIC
        qcoro/qcoroprocess.cpp
        qcoro/qcoroprocess.h
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qcoro
)

if (WIN32)
    target_sources(${PROJECT_NAME} PUBLIC
        qtwinextras/qwinevent.cpp
        qtwinextras/qwinevent.h
        qtwinextras/qwineventfilter.cpp
        qtwinextras/qwineventfilter_p.h
        qtwinextras/qwinfunctions.cpp
        qtwinextras/qwinfunctions.h
        qtwinextras/qwinfunctions_p.h
        qtwinextras/qwintaskbarbutton.cpp
        qtwinextras/qwintaskbarbutton.h
        qtwinextras/qwintaskbarbutton_p.h
        qtwinextras/qwintaskbarprogress.cpp
        qtwinextras/qwintaskbarprogress.h
        qtwinextras/windowsguidsdefs.cpp
        qtwinextras/windowsguidsdefs_p.h
        qtwinextras/winshobjidl_p.h
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/qtwinextras
    )
endif()
