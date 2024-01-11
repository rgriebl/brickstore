# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

configure_file(version.h.in generated/version.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_sources(${PROJECT_NAME} PUBLIC
    main.cpp
    version.h.in

    utility/appstatistics.cpp
    utility/appstatistics.h
    utility/chunkreader.cpp
    utility/chunkreader.h
    utility/chunkwriter.h
    utility/credentialsmanager.cpp
    utility/credentialsmanager.h
    utility/exception.cpp
    utility/exception.h
    utility/memoryresource.cpp
    utility/memoryresource.h
    utility/persistentcookiejar.cpp
    utility/persistentcookiejar.h
    utility/pooledarray.h
    utility/q3cache.h
    utility/q5hash.cpp
    utility/q5hash.h
    utility/q5hashfunctions.h
    utility/qparallelsort.h
    utility/ref.cpp
    utility/ref.h
    utility/stopwatch.h
    utility/transfer.cpp
    utility/transfer.h
    utility/utility.cpp
    utility/utility.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Xml
    Qt6::Network
    Qt6::CorePrivate
    Qt6::Concurrent
    QCoro6::Core
)

add_subdirectory(bricklink)

if (BS_DESKTOP OR BS_MOBILE)
    add_subdirectory(common)
    add_subdirectory(ldraw)
    add_subdirectory(scanner)
endif()

if (BS_BACKEND)
    add_subdirectory(backend)
elseif (BS_DESKTOP)
    add_subdirectory(desktop)
elseif (BS_MOBILE)
    add_subdirectory(mobile)
endif()
