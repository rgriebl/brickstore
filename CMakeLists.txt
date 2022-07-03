
cmake_minimum_required(VERSION 3.19.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# for QtCreator
set(QML_IMPORT_PATH ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/imports CACHE STRING "" FORCE)

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

find_package(Qt6 COMPONENTS Core REQUIRED)
include(QtPlatformSupport) # load all the platform names

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if (NOT (ANDROID OR APPLE))
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
endif()

# Android is the last supported 32bit platform
if (NOT ANDROID AND QT_32BIT)
    message(FATAL_ERROR "32bit builds for this platform are not supported.")
endif()

# this needs to happen before
if (CLANG)
    set(coroargs -fcoroutines-ts -stdlib=libc++)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:>${coroargs}")
elseif (GCC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>")
endif()

qt_add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE)

if (NOT IOS) # somehow this tries to access the wrong build dir on iOS
    include(SeparateDebugInfo)
    enable_separate_debug_info(${PROJECT_NAME})
endif()

add_compile_definitions(
    QT_STRICT_ITERATORS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
    QT_MESSAGELOGCONTEXT
)

find_package(Qt6
    REQUIRED Core Gui Xml Network Concurrent LinguistTools
    OPTIONAL_COMPONENTS ToolsTools
)

set(MIN_QT_VERSION 6.2.0)

if (ANDROID OR IOS OR FORCE_MOBILE)
    set(BS_MOBILE ON)
    set(MIN_QT_VERSION 6.3.0)
    add_compile_definitions(BS_MOBILE)
    set(QUICK_CONTROLS "QtQuick.Controls.Material")
    find_package(Qt6 REQUIRED Qml Quick Quick3D QuickControls2Impl QuickDialogs2 QuickTemplates2 QuickDialogs2Utils Svg)

elseif (BACKEND_ONLY)
    set(BS_BACKEND ON)
    add_compile_definitions(BS_BACKEND)

else()
    set(BS_DESKTOP ON)
    add_compile_definitions(BS_DESKTOP)
    find_package(Qt6 REQUIRED Widgets PrintSupport Qml Quick Quick3D QuickControls2Impl)

endif()

if (${Qt6_VERSION} VERSION_LESS "${MIN_QT_VERSION}")
    message(FATAL_ERROR "Qt ${MIN_QT_VERSION} or newer is required for building this variant")
endif()

if (WIN32)
    add_compile_definitions(BS_HAS_PARALLEL_STL)
    add_compile_definitions(NOMINMAX)
endif()

if (MODELTEST)
    find_package(Qt6 REQUIRED TestLib)
    add_compile_definitions(MODELTEST)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::TestLib)
endif()

if (NOT APPLE AND NOT WIN32 AND NOT ANDROID)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "brickstore")
endif()

if (CLANG OR GCC)
    # cmake's RelWithDebInfo is hardcoded to O2, while Release has O3
    string(REGEX REPLACE "([\\/\\-]O)2" "\\13" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

if (SANITIZE)
    if (NOT (LINUX AND (GCC OR CLANG)))
        message(FATAL_ERROR "Sanitizer builds are only supported on Linux, using GCC or Clang.")
        return()
    endif()
    set(ECM_ENABLE_SANITIZERS address leak undefined)
    include(${Qt6_DIR}/3rdparty/extra-cmake-modules/modules/ECMEnableSanitizers.cmake)
    add_compile_definitions(SANITIZER_ENABLED)
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

qt_add_lrelease(${PROJECT_NAME}
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)
# qt_add_lupdate is not flexible enough to scan all the required sources, so we better
# let lupdate itself scan recursively
add_custom_target(${PROJECT_NAME}_lupdate
    COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate "${CMAKE_SOURCE_DIR}" -ts ${TS_FILES}
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
    PREFIX /translations
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

if (APPLE)
    set(EXECUTABLE ${PROJECT_NAME})
    if (CMAKE_OSX_DEPLOYMENT_TARGET)
        set(MACOSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()
    if (IOS)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.ios.plist"
            MACOSX_BUNDLE_GUI_IDENTIFIER "de.brickforge.brickstore"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "de.brickforge.brickstore"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
            XCODE_ATTRIBUTE_SKIP_INSTALL "No"
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
        )
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.macos.plist"
        )
        set(MACOS_RESOURCES
            assets/generated-app-icons/brickstore.icns
            assets/generated-app-icons/brickstore_doc.icns
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

if (LINUX AND NOT ANDROID)
    find_package(TBB)
    if (TBB_FOUND)
        add_compile_definitions(
            BS_HAS_PARALLEL_STL
            TBB_SUPPRESS_DEPRECATED_MESSAGES=1
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
    else()
        message(STATUS "No libtbb found: parallel STL algorithms will not be used.")
    endif()
endif()

if (ANDROID)
    if (BUILD_NUMBER STREQUAL "custom")
        math(EXPR VERSION_CODE "((${VERSION_MAJOR} - 2000) * 10000) + (${VERSION_MINOR} * 100) + ${VERSION_PATCH}")
    else()
        set(VERSION_CODE "${BUILD_NUMBER}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_CODE ${VERSION_CODE}
        QT_ANDROID_VERSION_NAME ${VERSION}
    )

    if (QT_ANDROID_SDK_BUILD_TOOLS_REVISION)
        # Qt bug: this has to be set as a target property to stick
        set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_ANDROID_SDK_BUILD_TOOLS_REVISION "${QT_ANDROID_SDK_BUILD_TOOLS_REVISION}"
        )
    endif()

    # We expect KDAB's OpenSSL libs in $ANDROID_SDK_ROOT/android_openssl
    # cd $ANDROID_SDK_ROOT && git clone https://github.com/KDAB/android_openssl.git
    set(OPENSSL_CMAKE "${ANDROID_SDK_ROOT}/android_openssl/CMakeLists.txt")
    if (NOT EXISTS ${OPENSSL_CMAKE})
        message(FATAL_ERROR "${OPENSSL_CMAKE} is missing - please clone KDAB's android_openssl into ${ANDROID_SDK_ROOT}")
    endif()
    include(${OPENSSL_CMAKE})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}"
    )

    # Mixing pre-NDK23 objects (e.g. Qt) and (post-)NDK23 objects will crash when unwinding:
    # https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md#Unwinding
    if (ANDROID_NDK_MAJOR GREATER 22)
        target_link_libraries(${PROJECT_NAME} PRIVATE -lunwind)
    endif()
endif()

if (TARGET Qt6::qdoc)
    add_custom_target(extensions-doc
        WORKING_DIRECTORY
        COMMAND Qt6::qdoc "${CMAKE_SOURCE_DIR}/doc/extensions.qdocconf"
                --indexdir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_DOCS}"
                --outputdir "${CMAKE_BINARY_DIR}/doc/html/extensions"
    )
endif()

add_custom_target("Other-Files" SOURCES
    .gitattributes
    .gitignore
    .github/workflows/build_cmake.yml
    .github/scripts/ios-import-certificate.sh
    .github/scripts/ios-import-profile.sh
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
    doc/extensions.qdoc
    doc/extensions.qdocconf
    doc/extensions.css
    doc/images/list_arrow.png
    doc/images/list_expand.png
    doc/images/favicon.ico
    docker/Dockerfile
    docker/entrypoint.sh
    extensions/README.md
    extensions/classic-print-script.bs.qml
    macos/Info.macos.plist
    macos/Info.ios.plist
    macos/ExportOptions.ios.plist
    macos/locversion.plist.in
    macos/dmg-ds_store
    macos/dmg-background.png
    scripts/generate-assets.sh
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

    add_custom_target(dmg
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/macdeployqt" $<TARGET_FILE_DIR:${PROJECT_NAME}>/../..
                "-qmldir=${CMAKE_CURRENT_SOURCE_DIR}"
                "-verbose=2"
        COMMAND "${CMAKE_CPACK_COMMAND}" "-C" "$<CONFIGURATION>"
                "--config" "${CMAKE_BINARY_DIR}/BundleConfig.cmake"
        COMMENT "Creating macOS DMG. Please wait..."
        DEPENDS ${PROJECT_NAME}
    )
    set(CPACK_DMG_DS_STORE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-ds_store")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/macos/dmg-background.png")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}-${VERSION}")

elseif (WIN32)
    # deploy sentry dependencies
    set(COPY_SENTRY_COMMAND "")
    if (SENTRY)
        set(COPY_SENTRY_COMMAND COMMAND "${CMAKE_COMMAND}" -E copy
            "${VCPKG_PATH}/bin/sentry.dll"
            "${VCPKG_PATH}/bin/zlib1.dll"
            "${VCPKG_PATH}/tools/sentry-native/crashpad_handler.exe" "${CMAKE_CURRENT_BINARY_DIR}/bin")
    endif()

    set(INNO_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    if (INNO_ARCH STREQUAL "AMD64")
        set(INNO_ARCH "x64")
    endif()

    if (QT_HOST_PATH)
        set(WINDEPLOYQT_PATH "${QT_HOST_PATH}/${QT6_HOST_INFO_BINDIR}")
        set(WINDEPLOYQT_EXTRA_ARGS "--qmake=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/qmake.bat")
    else()
        set(WINDEPLOYQT_PATH "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()

    # deploy Qt dependencies and create an INNO setup installer
    add_custom_target(installer
        COMMAND "${WINDEPLOYQT_PATH}/windeployqt" $<TARGET_FILE:${PROJECT_NAME}>
                ${WINDEPLOYQT_EXTRA_ARGS}
                "--qmldir=${CMAKE_CURRENT_SOURCE_DIR}"
                "--no-opengl-sw"
                "--no-virtualkeyboard"
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

elseif (LINUX)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
    install(FILES unix/brickstore.desktop DESTINATION share/applications)
    install(FILES unix/brickstore-mime.xml DESTINATION share/mime/packages)
    install(FILES assets/generated-app-icons/brickstore.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES assets/generated-app-icons/brickstore_doc.png DESTINATION share/icons/hicolor/128x128/mimetypes)

    # Arch has Qt 6.2+ and is handled upstream via AUR (TODO: update to Qt6)
    # Ubuntu finally has a Qt 6.2+ starting with 22.04 LTS (jammy)
    # Debian will get a Qt 6.2+ with bookworm (currently testing, estimated release in summer 2023)

    # remove this, once the TLS plugin is packaged alongside QtNetwork again
    set(DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")

    if (NOT BS_BACKEND)
        list(APPEND DEBIAN_PACKAGE_DEPENDS
            "qml6-module-qtquick"
            "qml6-module-qtquick-controls"
            "qml6-module-qt5compat-graphicaleffects"
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
                "QML_SOURCES_PATHS=\"${CMAKE_SOURCE_DIR}\""
                "EXTRA_QT_PLUGINS=\"svg\""
                ${LINUXDEPLOY_DIR}/linuxdeploy-x86_64.AppImage "--appdir=${APPIMAGE_DIR}"
                    -e "${CMAKE_BINARY_DIR}/bin/brickstore"
                    -i "${CMAKE_SOURCE_DIR}/assets/generated-app-icons/brickstore.png"
                    -d "${CMAKE_SOURCE_DIR}/unix/brickstore.desktop"
                    --plugin qt --output appimage
        DEPENDS ${PROJECT_NAME}
    )

elseif (IOS)
    # the idea here comes from https://github.com/OlivierLDff/QtIosCMake/blob/master/AddQtIosApp.cmake

    # Workaround for https://stackoverflow.com/questions/30318941/cmake-generated-xcode-project-release-build-works-but-archive-fails-on-linker
    if (FIX_IOS_ARCHIVE_BUILD)
        # for the object libs, we have to link all .o to the correct folder
        add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
            VERBATIM
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/fix-ios-archive-build.sh
        )
        # for the static libs, we can force a common folder
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/out/library)
    endif()

    add_custom_target(archive
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
message(STATUS "  Git describe.... ${GIT_VERSION}")
message(STATUS "  Build type ..... ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt version ..... ${Qt6_VERSION}")
message(STATUS "  Qt location .... ${Qt6_DIR}")
message(STATUS "  BACKEND_ONLY ... ${BACKEND_ONLY}")
message(STATUS "  FORCE_MOBILE ... ${FORCE_MOBILE}")
message(STATUS "  SANITIZE ....... ${SANITIZE}")
message(STATUS "  MODELTEST ...... ${MODELTEST}")
message(STATUS "  SENTRY ......... ${SENTRY}")
message(STATUS "  VCPKG_PATH ..... ${VCPKG_PATH}")
message(STATUS "")
