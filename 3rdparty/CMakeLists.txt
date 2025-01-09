# Copyright (C) 2004-2025 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

target_sources(${PROJECT_NAME} PUBLIC
    lzma/bs_lzma.h
    lzma/bs_lzma.cpp
    lzma/lzmadec.h
    lzma/lzmadec.c

    minizip/crypt.h
    minizip/ioapi.c
    minizip/ioapi.h
    minizip/minizip.cpp
    minizip/minizip.h
    minizip/unzip.c
    minizip/unzip.h
    minizip/zip.c
    minizip/zip.h
    minizip/zlib_p.h
)

if(WIN32)
    target_sources(${PROJECT_NAME} PUBLIC
        minizip/iowin32.c
        minizip/iowin32.h
    )
elseif(NOT WASM)
    find_package(ZLIB REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${ZLIB_LIBRARIES}
    )
endif()

if (NOT BS_BACKEND)
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

if (BS_DESKTOP)
    target_sources(${PROJECT_NAME} PUBLIC
        qtsingleapplication/qtlockedfile.h
        qtsingleapplication/qtlockedfile.cpp
        qtsingleapplication/qtlocalpeer.h
        qtsingleapplication/qtlocalpeer.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/qtsingleapplication
    )
    if (WIN32)
        target_sources(${PROJECT_NAME} PUBLIC
            qtsingleapplication/qtlockedfile_win.cpp
        )
    else()
        target_sources(${PROJECT_NAME} PUBLIC
            qtsingleapplication/qtlockedfile_unix.cpp
        )
    endif()
endif()
