
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
endif()

target_sources(${PROJECT_NAME} PUBLIC
    qcoro/concepts_p.h
    qcoro/core/qcorocore.h
    qcoro/core/qcorofuture.h
    qcoro/core/qcoroiodevice.cpp
    qcoro/core/qcoroiodevice.h
    qcoro/core/qcorosignal.h
    qcoro/core/qcorotimer.cpp
    qcoro/core/qcorotimer.h
    qcoro/coroutine.h
    qcoro/macros_p.h
    qcoro/network/qcoroabstractsocket.cpp
    qcoro/network/qcoroabstractsocket.h
    qcoro/network/qcorolocalsocket.cpp
    qcoro/network/qcorolocalsocket.h
    qcoro/network/qcoronetwork.h
    qcoro/network/qcoronetworkreply.cpp
    qcoro/network/qcoronetworkreply.h
    qcoro/network/qcorotcpserver.cpp
    qcoro/network/qcorotcpserver.h
    qcoro/qcoro.h
    qcoro/task.h
    qcoro/waitoperationbase_p.h
)

if (NOT IOS)
    target_sources(${PROJECT_NAME} PUBLIC
        qcoro/core/qcoroprocess.cpp
        qcoro/core/qcoroprocess.h
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qcoro
    ${CMAKE_CURRENT_SOURCE_DIR}/qcoro/core
    ${CMAKE_CURRENT_SOURCE_DIR}/qcoro/network
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
