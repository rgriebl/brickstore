
target_sources(${PROJECT_NAME} PUBLIC
    lzma/bs_lzma.h
    lzma/bs_lzma.cpp
    lzma/lzmadec.h
    lzma/lzmadec.c
)
    
if (NOT BACKEND_ONLY)
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
