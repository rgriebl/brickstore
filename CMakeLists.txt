
cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Supported languages
set(LANGUAGES en de fr es)

option(BACKEND_ONLY "Backend only" OFF)
option(FORCE_MOBILE "Force a mobile build on desktop" OFF)
option(SANITIZE     "Build with ASAN" OFF)
option(MODELTEST    "Build with modeltest" OFF)
option(SENTRY       "Build with sentry.io support" OFF)
option(VCPKG_PATH   "VCPKG_PATH" "")

set(NAME           "BrickStore")
set(DESCRIPTION    "${NAME} - an offline BrickLink inventory management tool.")
set(COPYRIGHT      "2004-2022 Robert Griebl")
set(BRICKSTORE_URL "brickforge.de/brickstore")
set(GITHUB_URL     "github.com/rgriebl/brickstore")
set(BUILD_NUMBER   "$ENV{BUILD_NUMBER}")
if (NOT BUILD_NUMBER)
    set(BUILD_NUMBER   "custom")
endif()
if (WIN32)
    set(BUILD_USER     "$ENV{USERNAME}")
else()
    set(BUILD_USER     "$ENV{USER}")
endif()
cmake_host_system_information(RESULT BUILD_HOST QUERY HOSTNAME)

file(STRINGS ${CMAKE_SOURCE_DIR}/.tag GIT_VERSION)
if (GIT_VERSION STREQUAL "\$Format:%H\$")
    set(GIT_VERSION "unknown")
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.git)
        execute_process(
            COMMAND git describe --tags --always --dirty
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_DESCRIBE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if (GIT_DESCRIBE)
            set(GIT_VERSION "${GIT_DESCRIBE}")
        endif()
    endif()
endif()

file(STRINGS "VERSION_NUMBER" VERSION)
string(REPLACE "." ";" VERSION_LIST ${VERSION})
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_PATCH)

project(BrickStore
    VERSION ${VERSION}
    DESCRIPTION ${DESCRIPTION}
    HOMEPAGE_URL "https://${BRICKSTORE_URL}"
    LANGUAGES CXX C
)

find_package(Qt6)
include(QtPlatformSupport) # load all the platform names

if (NOT ANDROID)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
endif()

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE)

add_compile_definitions(
    QT_STRICT_ITERATORS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
    QT_MESSAGELOGCONTEXT
)

if (ANDROID)
    # Mixing pre-NDK23 objects (e.g. Qt) and (post-)NDK23 objects will crash when unwinding:
    # https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md#Unwinding
    target_link_libraries(${PROJECT_NAME} PRIVATE -lunwind)
endif()

find_package(Qt6 COMPONENTS Core Gui Xml Network Concurrent LinguistTools REQUIRED)

if (ANDROID OR IOS OR FORCE_MOBILE)
    set(BS_MOBILE ON)
    add_compile_definitions(BS_MOBILE)
    find_package(Qt6 COMPONENTS Qml Quick OpenGL REQUIRED)

elseif (BACKEND_ONLY)
    set(BS_BACKEND ON)
    add_compile_definitions(BS_BACKEND)

else()
    set(BS_DESKTOP ON)
    add_compile_definitions(BS_DESKTOP)
    find_package(Qt6 COMPONENTS Widgets PrintSupport Qml Quick OpenGL OpenGLWidgets REQUIRED)

endif()

if (MODELTEST)
    find_package(Qt6 COMPONENTS TestLib REQUIRED)
    target_compile_definitions(${PROJECT_NAME} MODELTEST)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::TestLib)
endif()

if (NOT APPLE AND NOT WIN32 AND NOT ANDROID)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "brickstore")
endif()

if (CLANG)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines-ts -stdlib=libc++>)
elseif (GCC)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
endif()

if (SANITIZE)
    if (NOT (LINUX AND (GCC OR CLANG)))
        message(FATAL_ERROR "Sanitizer builds are only supported on Linux, using GCC or Clang.")
        return()
    endif()
    ##CMAKE by default this takes ECM_ENABLE_SANITIZERS from the Qt build
    include(ECMEnableSanitizers)
    target_compile_definitions(${PROJECT_NAME} SANITIZER_ENABLED)
endif()

file(GLOB_RECURSE ICONS_QRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} assets/icons/*)
file(GLOB_RECURSE FLAGS_QRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} assets/flags/*)

set(qt_qm_base "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
foreach (LANG ${LANGUAGES})
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

if (WIN32)
    # Windows resources: icons and file-version record
    configure_file(windows/brickstore.rc.in generated/brickstore.rc @ONLY)
    target_sources(${PROJECT_NAME} PUBLIC generated/brickstore.rc)

    target_compile_definitions(${PROJECT_NAME} PRIVATE BS_HAS_PARALLEL_STL)

    # always link against widgets
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

    target_link_libraries(${PROJECT_NAME} PRIVATE user32 advapi32 wininet)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-declarations)
endif()

if (APPLE)
    set(EXECUTABLE ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.macos.plist"
    )
    set(MACOS_RESOURCES
        assets/generated-app-icons/brickstore.icns
        assets/generated-app-icons/brickstore_doc.icns
    )
    target_sources(${PROJECT_NAME} PUBLIC ${MACOS_RESOURCES})

    set_source_files_properties(${MACOS_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    foreach (LANG ${LANGUAGES})
        set(locpath "locversions/${LANG}.lproj/locversion.plist")
        configure_file(macos/locversion.plist.in "${locpath}" @ONLY)
        target_sources(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/${locpath}")
        set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${locpath} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/${LANG}.lproj"
        )
    endforeach()
endif()

if (LINUX AND NOT ANDROID)
    find_package(TBB)
    if (TBB_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE BS_HAS_PARALLEL_STL)
        target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
    else()
        message(STATUS "No libtbb found: parallel STL algorithms will not be used.")
    endif()
endif()

if (ANDROID)
    # force QtSvg deployment
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Svg)
endif()

add_custom_target(OtherFiles SOURCES
    .gitattributes
    .gitignore
    .github/workflows/*.yml
    .qmake.conf
    .tag
    CHANGELOG.md
    LICENSE.GPL
    README.md
    VERSION_NUMBER
    BrickStoreXML.rnc
    configure
    android/AndroidManifest.xml
    android/build.gradle
    android/gradle.properties
    android/gradle/wrapper/gradle-wrapper.jar
    android/gradle/wrapper/gradle-wrapper.properties
    android/gradlew
    android/gradlew.bat
    android/res/values/libs.xml
    assets/COPYING-ICONS
    assets/COPYING-FLAGS
    debian/*
    docker/Dockerfile
    docker/entrypoint.sh
    extensions/README.md
    extensions/*.bs.qml
    macos/*
    scripts/*.sh
    translations/translations.json
    unix/brickstore.desktop
    unix/brickstore-mime.xml
    windows/brickstore.iss
    windows/brickstore.rc.in
)

## Installers, Bundles, etc.

if (APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    add_custom_target(bundle
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/macdeployqt" $<TARGET_FILE:${PROJECT_NAME}>/../../.. -qmldir=${CMAKE_CURRENT_SOURCE_DIR}/extensions
        COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "$<CONFIGURATION>"
                "--config" "${CMAKE_BINARY_DIR}/BundleConfig.cmake"
        COMMENT "Running CPack. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )
    set(CPACK_DMG_DS_STORE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-ds_store")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-background.png")
    set(CPACK_GENERATOR "DragNDrop")

elseif (WIN32)
    # deploy sentry dependencies
    if (SENTRY)
        set(COPY_SENTRY_COMMAND "${CMAKE_COMMAND}" -E copy
            "${VCPKG_PATH}/bin/sentry.dll"
            "${VCPKG_PATH}/bin/zlib1.dll"
            "${VCPKG_PATH}/tools/sentry-native/crashpad_handler.exe" "${CMAKE_CURRENT_BINARY_DIR}/bin")
    endif()

    # deploy Qt dependencies and create an INNO setup installer
    add_custom_target(installer
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/windeployqt" $<TARGET_FILE:${PROJECT_NAME}>
                "--qmldir=${CMAKE_CURRENT_SOURCE_DIR}/extensions"
                "--no-opengl-sw"
                "--no-virtualkeyboard"
                "--no-translations"
                "--verbose" "2"
        COMMAND "${COPY_SENTRY_COMMAND}"
        COMMAND "iscc.exe" "/DSOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bin" "/O${CMAKE_CURRENT_BINARY_DIR}"
                "/F${PROJECT_NAME}-${VERSION}" "${CMAKE_SOURCE_DIR}/windows/brickstore.iss"
        COMMENT "Creating INNO Setup installer. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )

elseif (LINUX)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
    install(FILES unix/brickstore.desktop DESTINATION share/applications)
    install(FILES unix/brickstore-mime.xml DESTINATION share/mime/packages)
    install(FILES assets/generated-app-icons/brickstore.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES assets/generated-app-icons/brickstore_doc.png DESTINATION share/icons/hicolor/128x128/mimetypes)

    # Arch has Qt 6.2+ and is handled upstream via AUR
    # Ubuntu finally has a Qt 6.2+ starting with 22.04 LTS (jammy)
    # Debian will get a Qt 6.2+ with bookworm (currently testing, estimated release in summer 2023)

    if (NOT BS_BACKEND)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick (>= 6.2), qml6-module-qtquick-controls (>= 6.2)")
    endif()
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_PACKAGE_CONTACT "Robert Griebl <robert@griebl.org>")
    set(CPACK_DEBIAN_PACKAGE_SECTION x11)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    add_custom_target(deb-package
        COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "$<CONFIGURATION>" -G DEB
                "--config" "${CMAKE_BINARY_DIR}/BundleConfig.cmake"
        COMMENT "Running CPack. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )

    set(APPIMAGE_DIR "${CMAKE_BINARY_DIR}/AppDir")
    set(LINUXDEPLOY_DIR "${CMAKE_BINARY_DIR}/.linuxdeploy")

    add_custom_target(appimage
        COMMAND mkdir -p "${APPIMAGE_DIR}"
        COMMAND install -D "${CMAKE_SOURCE_DIR}/unix/brickstore-mime.xml"
                           "${APPIMAGE_DIR}/share/mime/packages"
        COMMAND install -D "${CMAKE_SOURCE_DIR}/assets/generated-app-icons/brickstore_doc.png"
                           "${APPIMAGE_DIR}/share/icons/hicolor/128x128/mimetypes"
        COMMAND mkdir -p "${LINUXDEPLOY_DIR}"
        COMMAND wget  -P "${LINUXDEPLOY_DIR}" -N "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        COMMAND chmod +x "${LINUXDEPLOY_DIR}/linuxdeploy-x86_64.AppImage"
        COMMAND wget  -P "${LINUXDEPLOY_DIR}" -N "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
        COMMAND chmod +x "${LINUXDEPLOY_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
        COMMAND "VERSION=${VERSION}"
                "QML_SOURCES_PATHS=\"${CMAKE_SOURCE_DIR}/extensions\""
                "EXTRA_QT_PLUGINS=\"svg\""
                ${LINUXDEPLOY_DIR}/linuxdeploy-x86_64.AppImage "--appdir=${APPIMAGE_DIR}"
                    -e "${CMAKE_BINARY_DIR}/bin/brickstore"
                    -i "${CMAKE_SOURCE_DIR}/assets/generated-app-icons/brickstore.png"
                    -d "${CMAKE_SOURCE_DIR}/unix/brickstore.desktop"
                    --plugin qt --output appimage
        DEPENDS ${PROJECT_NAME}
    )
endif()

# we don't want the standard 'package' target
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BundleConfig.cmake" )
include(CPack)
