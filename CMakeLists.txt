# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.19.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(SENTRY_VERSION 0.6.2)
set(QCORO_VERSION  0.9.0)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(ANDROID_PLATFORM 21) # supress warning by setting it early
endif()

# for QtCreator
set(QML_IMPORT_PATH ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/imports CACHE STRING "" FORCE)

# Supported languages
set(LANGUAGES en de fr es sv)

option(BACKEND_ONLY "Backend only" OFF)
option(FORCE_MOBILE "Force a mobile build on desktop" OFF)
option(SANITIZE     "Build with ASAN" OFF)
option(MODELTEST    "Build with modeltest" OFF)
option(SENTRY       "Build with sentry.io support" OFF)
option(VERBOSE_FETCH "Verbose output for 3rd party FetchContent" OFF)

set(NAME           "BrickStore")
set(DESCRIPTION    "${NAME} - an offline BrickLink inventory management tool.")
set(COPYRIGHT      "2004-2024 Robert Griebl")
set(BRICKSTORE_URL "www.brickstore.dev")
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
find_package(Qt6 CONFIG QUIET COMPONENTS Core REQUIRED)
include(QtPlatformSupport) # load all the platform names

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if (NOT (ANDROID OR APPLE))
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
endif()

# Android is the last supported 32bit platform
if (NOT ANDROID AND NOT WASM AND QT_32BIT)
    message(FATAL_ERROR "32bit builds for this platform are not supported.")
endif()

# important: set up sentry BEFORE enabling strict error reporting
set(BS_SENTRY OFF)
if (SENTRY)
    set(BS_SENTRY ${SENTRY_VERSION})
    include(BuildSentry)
endif()

include(BuildQCoro)

set(copts)
set(cxxopts)
if (MSVC)
    # need to disable C4127 due to 6.4.0 QStringBuilder
    # need to disable C4702 due to 6.4.0 QAnyStringView
    list(APPEND copts /W4 /WX /wd4127 /wd4702)
elseif (CLANG)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17.0)
        list(APPEND cxxopts -fcoroutines-ts)
    endif()
    list(APPEND cxxopts -stdlib=libc++)
    list(APPEND copts -Wall -Wextra -Wpedantic -Werror)
    # clang bug, legal in C++20
    list(APPEND cxxopts -Wno-gnu-zero-variadic-macro-arguments -Wno-nested-anon-types)
elseif (GCC)
    list(APPEND cxxopts -fcoroutines)
    list(APPEND copts -Wall -Wextra -Wpedantic -Werror)
endif()
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${copts}>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${copts}$<SEMICOLON>${cxxopts}>")

qt_add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE)

if (NOT IOS) # somehow this tries to access the wrong build dir on iOS
    include(SeparateDebugInfo)
    enable_separate_debug_info(${PROJECT_NAME})
endif()

# Use LTO, if available
include(CheckIPOSupported)
check_ipo_supported(RESULT HAS_LTO OUTPUT LTO_STATUS)
if (HAS_LTO)
  set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  set(LTO_STATUS ON)
endif()

add_compile_definitions(
    QT_STRICT_ITERATORS
    QT_STRICT_QLIST_ITERATORS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
    QT_MESSAGELOGCONTEXT
    QT_USE_QSTRINGBUILDER
)

find_package(Qt6 CONFIG QUIET
    REQUIRED Core Gui Xml Network Concurrent Sql LinguistTools
    OPTIONAL_COMPONENTS ToolsTools
)

if (${Qt6_VERSION} VERSION_GREATER_EQUAL "6.8.0")
    add_compile_definitions(
        "QT_WARN_DEPRECATED_UP_TO=QT_VERSION_CHECK(6,7,0)"
    )
endif()

set(MIN_QT_VERSION 6.2.4)

if (ANDROID OR IOS OR FORCE_MOBILE)
    set(BS_MOBILE ON)
    set(BS_TYPE "Mobile")
    set(MIN_QT_VERSION 6.5.2)
    add_compile_definitions(BS_MOBILE)
    set(QUICK_CONTROLS "QtQuick.Controls.Material")
    find_package(Qt6 CONFIG QUIET REQUIRED Qml Quick Quick3D QuickControls2Impl QuickDialogs2 QuickTemplates2 QuickDialogs2Utils Svg Multimedia MultimediaQuickPrivate)

elseif (BACKEND_ONLY)
    set(BS_BACKEND ON)
    set(BS_TYPE "Backend")
    add_compile_definitions(BS_BACKEND)

else()
    set(BS_DESKTOP ON)
    set(BS_TYPE "Desktop")
    add_compile_definitions(BS_DESKTOP)
    find_package(Qt6 CONFIG QUIET REQUIRED Widgets PrintSupport Qml Quick Quick3D Multimedia QuickWidgets Svg)

endif()

if (${Qt6_VERSION} VERSION_LESS "${MIN_QT_VERSION}")
    message(FATAL_ERROR "Qt ${MIN_QT_VERSION} or newer is required for building this variant")
endif()

set(PARALLEL_STL OFF)
if (WIN32)
    set(PARALLEL_STL ON)
    add_compile_definitions(
        BS_HAS_PARALLEL_STL
        NOMINMAX
    )
elseif (LINUX)
    find_package(TBB QUIET)
    if (TBB_FOUND)
        set(PARALLEL_STL ON)
        add_compile_definitions(
            BS_HAS_PARALLEL_STL
            TBB_SUPPRESS_DEPRECATED_MESSAGES=1
        )
    endif()
endif()

set(BS_LIBSECRET OFF)
if (LINUX AND (BS_DESKTOP OR BS_MOBILE))
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET QUIET IMPORTED_TARGET GLOBAL libsecret-1)
    if (LIBSECRET_FOUND)
        set(BS_LIBSECRET ON)
        add_compile_definitions(BS_HAS_LIBSECRET)
        target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBSECRET)
    endif()
endif()

if (SENTRY)
    add_compile_definitions(SENTRY_ENABLED)
    target_link_libraries(${PROJECT_NAME} PRIVATE sentry)
endif()

if (MODELTEST)
    find_package(Qt6 REQUIRED Test)
    add_compile_definitions(MODELTEST)
endif()

if (NOT APPLE AND NOT WIN32 AND NOT ANDROID)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "brickstore")
endif()

if (CLANG OR GCC)
    # cmake's RelWithDebInfo is hardcoded to O2, while Release has O3
    string(REGEX REPLACE "([\\/\\-]O)2" "\\13" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

if (SANITIZE)
    add_compile_definitions(SANITIZER_ENABLED)
    if (MSVC)
        set(ECM_ENABLE_SANITIZERS address)
    else()
        set(ECM_ENABLE_SANITIZERS address undefined)
    endif()
    include(${Qt6_DIR}/3rdparty/extra-cmake-modules/modules/ECMEnableSanitizers.cmake)
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
    else()
        set(qts "translations/qtbase_${LANG}.ts")
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${qts}")
            list(APPEND QT_TS_FILES ${qts})
        endif()
    endif()
endforeach()
target_sources(${PROJECT_NAME} PUBLIC ${TS_FILES} ${QT_TS_FILES})
set_source_files_properties(${TS_FILES} ${QT_TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/qm")

qt_add_lrelease(${PROJECT_NAME}
    TS_FILES ${TS_FILES} ${QT_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)
# qt_add_lupdate is not flexible enough to scan all the required sources, so we better
# let lupdate itself scan recursively
add_custom_target(${PROJECT_NAME}_lupdate
    COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate "${CMAKE_SOURCE_DIR}"
            -locations relative -no-ui-lines -ts ${TS_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND_EXPAND_LISTS
    VERBATIM
)
if(NOT TARGET update_translations)
    add_custom_target(update_translations)
endif()
add_dependencies(update_translations ${PROJECT_NAME}_lupdate)
# end of custom qt_add_lupdate replacement

qt_add_resources(${PROJECT_NAME} qt_translations_qrc
    PREFIX "/translations"
    BASE "${qt_qm_base}"
    FILES ${QT_QM_FILES}
)
qt_add_resources(${PROJECT_NAME} bs_translations_qrc
    PREFIX "/translations"
    BASE "${CMAKE_CURRENT_BINARY_DIR}/qm"
    FILES ${QM_FILES}
)
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

include_directories(3rdparty)
add_subdirectory(3rdparty)

include_directories(src)
add_subdirectory(src)

if (WIN32)
    # Windows resources: icons and file-version record
    configure_file(windows/brickstore.rc.in generated/brickstore.rc @ONLY)
    target_sources(${PROJECT_NAME} PUBLIC generated/brickstore.rc)

    # always link against widgets
    find_package(Qt6 REQUIRED Widgets)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

    target_link_libraries(${PROJECT_NAME} PRIVATE user32 advapi32 wininet)
endif()

if (LINUX AND TBB_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
endif()

if (APPLE)
    set(EXECUTABLE ${PROJECT_NAME})
    if (CMAKE_OSX_DEPLOYMENT_TARGET)
        set(MACOSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()
    find_library(SECURITY_LIB Security)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_LIB})
    if (LTO_STATUS)
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-object_path_lto -Wl,lto.o")
    endif()
    if (IOS)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.ios.plist"
            MACOSX_BUNDLE_GUI_IDENTIFIER "de.brickforge.brickstore"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "de.brickforge.brickstore"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
            XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
            XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
            QT_IOS_LAUNCH_SCREEN "${CMAKE_SOURCE_DIR}/macos/LaunchScreen.storyboard"
        )
        set(APPICON_XCASSETS "${CMAKE_SOURCE_DIR}/assets/generated-app-icons/AppIcon.xcassets")
        target_sources(${PROJECT_NAME} PRIVATE ${APPICON_XCASSETS})
        set_source_files_properties(${APPICON_XCASSETS} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )

        # restrict the number of deployed plugins in static builds
        qt_import_plugins(${PROJECT_NAME}
            EXCLUDE_BY_TYPE qmltooling
            INCLUDE_BY_TYPE imageformats
                Qt::QGifPlugin
                Qt::QJpegPlugin
                Qt::QSvgPlugin
                Qt::QWebpPlugin
            INCLUDE_BY_TYPE sqldrivers
                Qt::QSQLiteDriverPlugin
        )
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.macos.plist"
            MACOSX_BUNDLE_GUI_IDENTIFIER "de.brickforge.brickstore"
        )
        set(MACOS_RESOURCES
            assets/generated-app-icons/brickstore.icns
            assets/generated-app-icons/brickstore_doc.icns
            macos/macos.entitlements
        )
        target_sources(${PROJECT_NAME} PUBLIC ${MACOS_RESOURCES})
        set_source_files_properties(${MACOS_RESOURCES} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
    endif()

    foreach (LANG ${LANGUAGES})
        set(locpath "locversions/${LANG}.lproj/locversion.plist")
        configure_file(macos/locversion.plist.in "${locpath}" @ONLY)
        target_sources(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/${locpath}")
        set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${locpath} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources/${LANG}.lproj"
        )
    endforeach()
endif()

if (ANDROID)
    if (BUILD_NUMBER STREQUAL "custom")
        set(VERSION_CODE "0")
    else()
        # the 1000000 offset is necessary because I screwed up the first PlayStore upload
        math(EXPR VERSION_CODE "1000000 + ${BUILD_NUMBER}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_CODE ${VERSION_CODE}
        QT_ANDROID_VERSION_NAME ${VERSION}
    )

    if (QT_ANDROID_SDK_BUILD_TOOLS_REVISION) # this has to be set as a target property to stick
        set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_ANDROID_SDK_BUILD_TOOLS_REVISION "${QT_ANDROID_SDK_BUILD_TOOLS_REVISION}"
        )
    endif()

    if (QT_ANDROID_TARGET_SDK_VERSION) # this has to be set as a target property to stick
        set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_ANDROID_TARGET_SDK_VERSION "${QT_ANDROID_TARGET_SDK_VERSION}"
        )
    endif()

    include(FetchContent)

    FetchContent_Declare(
        android_openssl
        GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
        GIT_TAG master
        GIT_SHALLOW YES
    )
    FetchContent_MakeAvailable(android_openssl)
    FetchContent_GetProperties(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
    add_android_openssl_libraries(${PROJECT_NAME})

    # Mixing pre-NDK23 objects (e.g. Qt) and (post-)NDK23 objects will crash when unwinding:
    # https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md#Unwinding
    if (ANDROID_NDK_MAJOR GREATER 22)
        target_link_libraries(${PROJECT_NAME} PRIVATE -lunwind)
    endif()
endif()

if (TARGET Qt6::qdoc)
    add_custom_target(extensions-doc
        WORKING_DIRECTORY
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}/doc/html/extensions"
        COMMAND Qt6::qdoc "${CMAKE_SOURCE_DIR}/doc/extensions.qdocconf"
                --indexdir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_DOCS}"
                --outputdir "${CMAKE_BINARY_DIR}/doc/html/extensions"
    )
endif()

add_custom_target("Other-Files" SOURCES
    .gitattributes
    .gitignore
    .github/workflows/build_cmake.yml
    .github/scripts/apple-import-apikey.sh
    .github/scripts/apple-import-certificate.sh
    .github/scripts/apple-import-profile.sh
    .github/FUNDING.yml
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
    android/src/de/brickforge/brickstore/ExtendedQtActivity.java
    assets/COPYING-ICONS
    assets/COPYING-FLAGS
    assets/generate-assets.sh
    cmake/SeparateDebugInfo.cmake
    cmake/SeparateDebugInfo.Info.plist.in
    doc/extensions.css
    doc/extensions.qdoc
    doc/extensions.qdocconf
    doc/images/list_arrow.png
    doc/images/list_expand.png
    doc/images/favicon.ico
    docker/Dockerfile
    docker/entrypoint.sh
    docker/rebuild-db.sh
    extensions/README.md
    extensions/classic-print-script.bs.qml
    ldraw/create-bricklink-ldraw-id-mappings.py
    ldraw/mappings/wheels.yaml
    macos/dmg-background.png
    macos/dmg-ds_store
    macos/ExportOptions.ios.plist
    macos/Info.ios.plist
    macos/Info.macos.plist
    macos/LaunchScreen.storyboard
    macos/locversion.plist.in
    scripts/git-release.sh
    translations/translations.json
    unix/brickstore.desktop
    unix/brickstore-mime.xml
    windows/brickstore.iss
    windows/brickstore.rc.in
)

## Installers, Bundles, etc.

if (MACOS)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    set(DMG_DIR "${CMAKE_BINARY_DIR}/dmg")
    set(DMG_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${VERSION}.dmg")

    # deploy sentry dependencies
    set(COPY_SENTRY_COMMAND "")
    if (SENTRY)
        set(COPY_SENTRY_COMMAND COMMAND cp "$<TARGET_FILE:crashpad_handler>" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endif()
    set(MACOS_SIGN_ARGS "")
    if (MACOS_SIGNING_IDENTITY)
        set(MACOS_SIGN_ARGS "-sign-for-notarization=${MACOS_SIGNING_IDENTITY}")
    endif()

    # We cannot use CPack here: macdeployqt signs all binaries, but the installation step when
    # running CPack's DragNDrop generator would try to mess with the rpaths and in turn nullify
    # the code signatures.
    # Also: macdeployqt has to be run on the bundle in the build location. If we copy to the dmg
    # staging directory first, we get weird errors because the tool thinks that all Qt frameworks
    # are "outside of the bundle"

    add_custom_command(
        OUTPUT ${DMG_FILE}
        COMMENT "Creating macOS DMG. Please wait..."
        DEPENDS ${PROJECT_NAME}
        ${COPY_SENTRY_COMMAND}
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/macdeployqt" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../.."
                "-qmldir=${CMAKE_CURRENT_SOURCE_DIR}"
                "-verbose=2"
                ${MACOS_SIGN_ARGS}
        COMMAND rm -rf "${DMG_DIR}"
        COMMAND mkdir -p "${DMG_DIR}/.background"
        COMMAND cp "${CMAKE_SOURCE_DIR}/macos/dmg-ds_store" "${DMG_DIR}/.DS_Store"
        COMMAND cp "${CMAKE_SOURCE_DIR}/macos/dmg-background.png" "${DMG_DIR}/.background/background.png"
        COMMAND ln -s /Applications "${DMG_DIR}"
        COMMAND cp -a "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" "${DMG_DIR}"
        COMMAND "${CMAKE_SOURCE_DIR}/macos/retry.sh" 5 hdiutil create ${DMG_FILE}
                               -volname "${PROJECT_NAME}-${VERSION}" -fs "HFS+" -format ULFO -ov
                               -srcdir ${DMG_DIR}
    )
    add_custom_target(dmg DEPENDS ${DMG_FILE})

elseif (WIN32)
    # deploy sentry dependencies
    set(COPY_SENTRY_COMMAND "")
    if (SENTRY)
        set(COPY_SENTRY_COMMAND COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:crashpad_handler>" "${CMAKE_CURRENT_BINARY_DIR}/bin"
        )
    endif()

    set(INNO_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    if (INNO_ARCH STREQUAL "AMD64")
        set(INNO_ARCH "x64")
    endif()

    if (QT_HOST_PATH)
        set(WINDEPLOYQT_PATH "${QT_HOST_PATH}/${QT6_HOST_INFO_BINDIR}")
        set(WINDEPLOYQT_EXTRA_ARGS "--qtpaths=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/qtpaths.bat")
    else()
        set(WINDEPLOYQT_PATH "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()

    if (${Qt6_VERSION} VERSION_LESS "6.5.0")
        list(APPEND WINDEPLOYQT_EXTRA_ARGS --no-qmltooling)
    elseif (${Qt6_VERSION} VERSION_GREATER "6.5.0")
        list(APPEND WINDEPLOYQT_EXTRA_ARGS --skip-plugin-types qmltooling)
    endif ()

    # deploy Qt dependencies and create an INNO setup installer
    add_custom_target(installer
        COMMAND "${WINDEPLOYQT_PATH}/windeployqt" $<TARGET_FILE:${PROJECT_NAME}>
                ${WINDEPLOYQT_EXTRA_ARGS}
                "--qmldir=${CMAKE_CURRENT_SOURCE_DIR}"
                "--no-opengl-sw"
                "--no-translations"
                "--verbose" "2"
        ${COPY_SENTRY_COMMAND}
        COMMAND "iscc.exe" "/DSOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bin" "/DARCH=${INNO_ARCH}"
                "/O${CMAKE_CURRENT_BINARY_DIR}"
                "/F${PROJECT_NAME}-${VERSION}" "${CMAKE_SOURCE_DIR}/windows/brickstore.iss"
        COMMENT "Creating INNO Setup installer. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )

elseif (ANDROID)
    # we cannot set the apk name directly, so we have to rename it after creation
    add_custom_target(final-apk
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/android-build/${PROJECT_NAME}.apk"
                                           "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${VERSION}.apk"
        COMMENT "Moving apk to final name and location..."
        DEPENDS apk
    )

    string(TOLOWER "${CMAKE_BUILD_TYPE}" lower_build_type)

    add_custom_target(final-aab
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/android-build/build/outputs/bundle/${lower_build_type}/android-build-release.aab"
                                           "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${VERSION}.aab"
        COMMENT "Moving aab to final name and location..."
        DEPENDS aab
    )

elseif (LINUX)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
    install(FILES unix/brickstore.desktop DESTINATION share/applications)
    install(FILES unix/brickstore-mime.xml DESTINATION share/mime/packages)
    install(FILES assets/generated-app-icons/brickstore.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES assets/generated-app-icons/brickstore_doc.png DESTINATION share/icons/hicolor/128x128/mimetypes)

    # Arch has Qt 6.2+ and is handled upstream via AUR
    # Ubuntu finally has a Qt 6.2+ starting with 22.04 LTS (jammy)
    # Debian will get a Qt 6.2+ with bookworm (currently testing, estimated release in summer 2023)

    set(DEBIAN_PACKAGE_DEPENDS
        libqt6sql6-sqlite
        qt6-qpa-plugins   # the TLS and basic image-format plugins are wrongly packaged into here
    )

    if (NOT BS_BACKEND)
        list(APPEND DEBIAN_PACKAGE_DEPENDS
            libqt6svg6
            qt6-gtk-platformtheme
            qt6-image-formats-plugins
            qml6-module-qtqml-workerscript
            qml6-module-qtquick
            qml6-module-qtquick-controls
            qml6-module-qt5compat-graphicaleffects
            qml6-module-quick3d
    )
    endif()
    string(REPLACE ";" "," CPACK_DEBIAN_PACKAGE_DEPENDS "${DEBIAN_PACKAGE_DEPENDS}")
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

    set(SNAP_DIR "${CMAKE_BINARY_DIR}/snap")
    file(COPY "${CMAKE_SOURCE_DIR}/assets/generated-app-icons/brickstore.png" DESTINATION "${SNAP_DIR}/gui/")
    configure_file("${CMAKE_SOURCE_DIR}/unix/snapcraft.yaml.in" "${SNAP_DIR}/snapcraft.yaml" @ONLY)

elseif (IOS)
    # the idea here comes from https://github.com/OlivierLDff/QtIosCMake/blob/master/AddQtIosApp.cmake

    add_custom_target(archive
        DEPENDS ${PROJECT_NAME}  ## we need to build twice! see QTBUG-105006
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND xcodebuild
                -project ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.xcodeproj
                -configuration Release
                -scheme ${PROJECT_NAME}
                -archivePath ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.xcarchive
                -derivedDataPath ${CMAKE_BINARY_DIR}/derivedData
                archive
    )

    if (${CMAKE_OSX_SYSROOT} MATCHES ".*SIMULATOR.*")
        message(STATUS "Skipping IPA target generation for simulator builds")
    elseif (NOT CMAKE_XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER
            OR NOT CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY
            OR NOT CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM)
        message(STATUS "Skipping IPA generation because not all necessary variables are set")
    else()
        get_target_property(IOS_BUNDLE_IDENTIFIER ${PROJECT_NAME} XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER)

        set(EXPORT_OPTIONS_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_ExportOptions.plist)
        configure_file(${CMAKE_SOURCE_DIR}/macos/ExportOptions.ios.plist ${EXPORT_OPTIONS_FILE})

        add_custom_target(ipa
            DEPENDS archive
            COMMAND xcodebuild
                    -exportArchive
                    -archivePath ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.xcarchive
                    -exportOptionsPlist ${EXPORT_OPTIONS_FILE}
                    -exportPath ${CMAKE_BINARY_DIR}/ipa-export
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/ipa-export/${PROJECT_NAME}.ipa"
                    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${VERSION}.ipa"
        )
    endif()
endif()

# we don't want the standard 'package' target
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BundleConfig.cmake" )
include(CPack)

message(STATUS "")
message(STATUS "Configure Summary:")
message(STATUS "  Version ........ ${VERSION}")
message(STATUS "  Build number ... ${BUILD_NUMBER}")
message(STATUS "  Build type ..... ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt version ..... ${Qt6_VERSION}")
message(STATUS "  Qt location .... ${Qt6_DIR}")
message(STATUS "  Build type ..... ${BS_TYPE}")
message(STATUS "  Link-time opt. . ${LTO_STATUS}")
message(STATUS "  Use sentry.io .. ${BS_SENTRY}")
message(STATUS "  ASAN ........... ${SANITIZE}")
message(STATUS "  Qt Modeltest ... ${MODELTEST}")
message(STATUS "  Parallel STL ... ${PARALLEL_STL}")
message(STATUS "  Linux/libsecret  ${BS_LIBSECRET}")
message(STATUS "")
