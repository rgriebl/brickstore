
cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BACKEND_ONLY "Backend only" OFF)
option(FORCE_MOBILE "Force a mobile build on desktop" OFF)
option(SANITIZE "Build with ASAN" OFF)
option(MODELTEST "Build with modeltest" OFF)

set(NAME "BrickStore")
set(DESCRIPTION    "${NAME} - an offline BrickLink inventory management tool.")
set(COPYRIGHT      "2004-2021 Robert Griebl")
set(BRICKSTORE_URL "brickforge.de/brickstore")
set(GITHUB_URL     "github.com/rgriebl/brickstore")
file(STRINGS "VERSION" VERSION)

set(LANGUAGES en de fr)

project(BrickStore
    VERSION ${VERSION}
    DESCRIPTION ${DESCRIPTION}
    LANGUAGES CXX C
)

if (ANDROID OR IOS OR FORCE_MOBILE)
    set(BS_MOBILE ON)
    add_compile_definitions(BS_MOBILE)
elseif (BACKEND_ONLY)
    set(BS_BACKEND ON)
    add_compile_definitions(BS_BACKEND)
else()
    set(BS_DESKTOP ON)
    add_compile_definitions(BS_DESKTOP)
endif()

find_package(Qt6 COMPONENTS Core Gui Xml Network Concurrent LinguistTools REQUIRED)

if (BS_DESKTOP)
    find_package(Qt6 COMPONENTS Widgets PrintSupport Qml Quick OpenGL OpenGLWidgets REQUIRED)
elseif (BS_MOBILE)
    find_package(Qt6 COMPONENTS Qml Quick OpenGL REQUIRED)
endif()
if(MODELTEST)
    find_package(Qt6 COMPONENTS TestLib REQUIRED)
endif()
#if(WIN32)
#    find_package(Qt6 COMPONENTS WinExtras Widgets REQUIRED)
#endif()

add_compile_definitions(
    QT_STRICT_ITERATORS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
)

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE)

if (NOT APPLE AND NOT WIN32 AND NOT ANDROID)
    set_target_properties(brickstore PROPERTIES OUTPUT_NAME "brickstore")
endif()

if(SANITIZE)
    if (NOT (LINUX AND (GCC OR CLANG)))
        message(FATAL_ERROR "Sanitizer builds are only supported on Linux, using GCC or Clang.")
        return()
    endif()
    ##CMAKE by default this takes ECM_ENABLE_SANITIZERS from the Qt build
    include(ECMEnableSanitizers)
    target_compile_definitions(${PROJECT_NAME} SANITIZER_ENABLED)
endif()

if(MODELTEST)
    target_compile_definitions(${PROJECT_NAME} MODELTEST)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::TestLib)
endif()


file(GLOB_RECURSE ICONS_QRC CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} assets/icons/*)
file(GLOB_RECURSE FLAGS_QRC CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} assets/flags/*)

set(qt_qm_base "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
foreach(LANG ${LANGUAGES})
    set(lts "translations/brickstore_${LANG}.ts")
    list(APPEND TS_FILES ${lts})
    set(qqm "${qt_qm_base}/qtbase_${LANG}.qm")
    if (EXISTS ${qqm})
        list(APPEND QT_QM_FILES ${qqm})
    endif()
endforeach()
target_sources(${PROJECT_NAME} PUBLIC ${TS_FILES})
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/qm")
qt6_add_translation(QM_FILES ${TS_FILES})

qt_add_resources(${PROJECT_NAME} qt_translations_qrc PREFIX /translations BASE "${qt_qm_base}" FILES ${QT_QM_FILES})
qt_add_resources(${PROJECT_NAME} bs_translations_qrc PREFIX /translations BASE "${CMAKE_CURRENT_BINARY_DIR}/qm" FILES ${QM_FILES})
qt_add_resources(${PROJECT_NAME} brickstore_qrc PREFIX / FILES
    translations/translations.json
    assets/generated-app-icons/brickstore.png
    assets/generated-app-icons/brickstore_doc.png
    ${ICONS_QRC}
    ${FLAGS_QRC}
    extensions/classic-print-script.bs.qml
)

#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

include_directories(src)
add_subdirectory(src)

include_directories(3rdparty)
add_subdirectory(3rdparty)

if(WIN32)
    #target_sources(${PROJECT_NAME} PUBLIC windows/brickstore.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 advapi32 wininet)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-declarations)
endif()
if(APPLE)
    set(EXECUTABLE ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist"
    )
    set(MACOS_RESOURCES
        assets/generated-app-icons/brickstore.icns
        assets/generated-app-icons/brickstore_doc.icns
    )
    target_sources(${PROJECT_NAME} PUBLIC ${MACOS_RESOURCES})

    set_source_files_properties(${MACOS_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    foreach(LANG ${LANGUAGES})
        set(locpath "locversions/${LANG}.lproj/locversion.plist")
        configure_file(macos/locversion.plist.in "${locpath}" @ONLY)
        target_sources(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/${locpath}")
        set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${locpath} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/${LANG}.lproj"
        )
    endforeach()
endif()

add_custom_target(OtherFiles SOURCES
    .gitignore
    .gitattributes
    .tag
    .github/workflows/*.yml
    .qmake.conf
    VERSION
    README.md
    CHANGELOG.md
    LICENSE.GPL
    BrickStoreXML.rnc
    configure
    translations/translations.json
    scripts/generate-assets.sh
    extensions/README.md
    extensions/*.bs.qml
    debian/*
    unix/brickstore.desktop
    unix/brickstore-mime.xml
    windows/brickstore.iss
)

if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    add_custom_target(bundle
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/macdeployqt" $<TARGET_FILE:${PROJECT_NAME}>/../../.. -verbose=2 -qmldir=${CMAKE_CURRENT_SOURCE_DIR}/extensions
        COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "$<CONFIGURATION>"
                "--config" "${CMAKE_BINARY_DIR}/BundleConfig.cmake"
        COMMENT "Running CPack. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )
    set(CPACK_DMG_DS_STORE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-ds_store")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-background.png")
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    # windeployqt
elseif(LINUX)
    # appimage + flatpak
endif()

# we don't want the standard 'package' target
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BundleConfig.cmake" )
include(CPack)
