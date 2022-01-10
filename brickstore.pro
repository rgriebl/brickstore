## Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
##
## This file is part of BrickStore.
##
## This file may be distributed and/or modified under the terms of the GNU
## General Public License version 2 as published by the Free Software Foundation
## and appearing in the file LICENSE.GPL included in the packaging of this file.
##
## This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
## WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
## See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.


android|ios|force-mobile:CONFIG *= bs_mobile
else:backend-only:CONFIG *= bs_backend
else:CONFIG *= bs_desktop

bs_mobile: MIN_QT_VERSION = 6.2.0
else:      MIN_QT_VERSION = 5.15.0

NAME        = "BrickStore"
DESCRIPTION = "$$NAME - an offline BrickLink inventory management tool."
COPYRIGHT   = "2004-2021 Robert Griebl"
BRICKSTORE_URL = "brickforge.de/brickstore"
GITHUB_URL  = "github.com/rgriebl/brickstore"


##NOTE: The VERSION is set in the file "VERSION" and pre-processed in .qmake.conf


requires(linux|macos|win32|ios:!winrt)
!versionAtLeast(QT_VERSION, $$MIN_QT_VERSION) {
    error("$$escape_expand(\\n\\n) *** $$NAME needs to be built against $$MIN_QT_VERSION or higher ***$$escape_expand(\\n\\n)")
}

TEMPLATE = app

TARGET = $$NAME
unix:!macos:!android:!ios:TARGET = $$lower($$TARGET)

CONFIG *= no_private_qt_headers_warning no_include_pwd c++2a
CONFIG *= lrelease embed_translations

bs_mobile:DEFINES *= BS_MOBILE
bs_backend:DEFINES *= BS_BACKEND
bs_desktop:DEFINES *= BS_DESKTOP

sanitize:debug:unix {
  CONFIG *= sanitizer sanitize_address sanitize_undefined
  # CONFIG *= sanitizer sanitize_thread
  DEFINES += SANITIZER_ENABLED
}

modeltest:debug {
  QT *= testlib
  DEFINES += MODELTEST
}

DEFINES *= QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
DEFINES *= QT_MESSAGELOGCONTEXT

!android:DESTDIR = bin

# cmake compatible substitutions
defineTest(substitute) {
  SIN = $$cat($$1, lines)
  for(line, SIN) {
    varname = $$section(line, "@", 1, 1)
    !isEmpty(varname):line = $$replace(line, "@$${varname}@", "$$eval($${varname})")
    SOUT += $$line
  }
  write_file($$2, SOUT)
  return(true)
}

BrickStore_VERSION_MAJOR = $$VERSION_MAJOR
BrickStore_VERSION_MINOR = $$VERSION_MINOR
BrickStore_VERSION_PATCH = $$VERSION_PATCH
substitute($$PWD/src/version.h.in, $$OUT_PWD/src/version.h)

INCLUDEPATH *= $$OUT_PWD/src  # for version.h

OTHER_FILES += \
  .gitignore \
  .gitattributes \
  .tag \
  .github/workflows/*.yml \
  .qmake.conf \
  VERSION \
  README.md \
  CHANGELOG.md \
  LICENSE.GPL \
  BrickStoreXML.rnc \
  configure \
  translations/translations.json \
  scripts/generate-assets.sh \
  extensions/README.md \
  extensions/*.bs.qml \
  debian/* \
  unix/brickstore.desktop \
  unix/brickstore-mime.xml \
  windows/brickstore.iss \

LANGUAGES = en de fr cz pt es

for(l, LANGUAGES) {
  TRANSLATIONS += translations/brickstore_$${l}.ts
  qt_qm = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${l}.qm
  exists($$qt_qm):qt_translations.files += $$qt_qm
}

qt_translations.base = $$[QT_INSTALL_TRANSLATIONS]
qt_translations.prefix = translations
QM_FILES_RESOURCE_PREFIX = translations

android:QT *= svg
RESOURCES = \
  qt_translations \
  translations/translations.json \
  assets/generated-app-icons/brickstore.png \
  assets/generated-app-icons/brickstore_doc.png \
  assets/icons \
  assets/flags \
  extensions/classic-print-script.bs.qml \

!bs_backend:include(3rdparty/minizip.pri)
include(3rdparty/lzma.pri)
include(3rdparty/qcoro.pri)
include(src/src.pri)
include(doc/doc.pri)

INCLUDEPATH *= 3rdparty/

qtPrepareTool(LUPDATE, lupdate)

lupdate.commands = $$QMAKE_CD $$system_quote($$system_path($$PWD)) && $$LUPDATE $$_PRO_FILE_
QMAKE_EXTRA_TARGETS += lupdate-all

sentry {
  !build_pass {
    isEmpty(VCPKG_PATH):error("You tried to enable crashpad, but didn't set VCPKG_PATH")
    else:message("Building with crashpad/sentry.io support from $$VCPKG_PATH")
  }

  INCLUDEPATH *= $$VCPKG_PATH/include
  CONFIG(debug, debug|release):LIBS += "-L$$VCPKG_PATH/debug/lib"
  else:LIBS += "-L$$VCPKG_PATH/lib"
  LIBS *= -lsentry
  macos:LIBS *= -lcurl -lcrashpad_client -lmini_chromium -lcrashpad_util -lbsm -framework Security
  CONFIG *= force_debug_info

  DEFINES *= SENTRY_ENABLED
}


#
# Windows specific
#

win32 {
  RC_ICONS = \
    assets/generated-app-icons/brickstore.ico \
    assets/generated-app-icons/brickstore_doc.ico \

  # qmake uses these to generate a FILEVERSION record
  QMAKE_TARGET_COPYRIGHT   = "Copyright (c) $$COPYRIGHT"
  QMAKE_TARGET_COMPANY     = "https://$$BRICKSTORE_URL"
  QMAKE_TARGET_DESCRIPTION = "$$DESCRIPTION"

  DEFINES *= NOMINMAX

  LIBS += -luser32 -ladvapi32 -lwininet

  build_pass:CONFIG(release, debug|release) {
    ISCC="iscc.exe"
    !system(where /Q $$ISCC) {
      INNO_PATH=$$(INNO_SETUP_PATH)
      !exists("$$INNO_PATH\\$$ISCC") {
        INNO_PATH="$$getenv(ProgramFiles(x86))\\Inno Setup 6"
        !exists("$$INNO_PATH\\$$ISCC"):error("Please set %INNO_SETUP_PATH% to point to the directory containing the '$$ISCC' binary.")
      }
      ISCC="$$INNO_PATH\\$$ISCC"
    }

    !isEmpty(VCPKG_PATH):OPENSSL_PATHS *= $$VCPKG_PATH/bin

    contains(QMAKE_TARGET.arch, x86_64) {
      OPENSSL_PATHS *= "$$getenv(ProgramFiles)/OpenSSL-Win64/bin"
      OPENSSL_PATHS *= "$$getenv(ProgramFiles)/OpenSSL/bin"

      OPENSSL_SSL_LIB="libssl-1_1-x64.dll"
      OPENSSL_CRYPTO_LIB="libcrypto-1_1-x64.dll"
    } else {
      OPENSSL_PATHS *= "$$getenv(ProgramFiles)/OpenSSL-Win32/bin"
      OPENSSL_PATHS *= "$$getenv(ProgramFiles(x86))/OpenSSL/bin"

      OPENSSL_SSL_LIB="libssl-1_1.dll"
      OPENSSL_CRYPTO_LIB="libcrypto-1_1.dll"
    }

    OPENSSL_PATH=""
    for (osslp, OPENSSL_PATHS) {
      exists("$$osslp/$$OPENSSL_SSL_LIB"):isEmpty(OPENSSL_PATH):OPENSSL_PATH=$$osslp
    }
    isEmpty(OPENSSL_PATH) {
      error("Please install OpenSSL via VCPKG or from https://slproweb.com/products/Win32OpenSSL.html.")
    } else {
      OPENSSL_PATH=$$clean_path($$OPENSSL_PATH)
      message("Found OpenSSL at $$OPENSSL_PATH")
    }

    # The OpenSSL libs from the Qt installer require an ancient MSVC2010 C runtimes, but the
    # installer doesn't install them by default, plus there's no package for the x86 version anyway.
    # The build from slwebpro.com on the other hand are built against a recent v14 runtime, which
    # we are installing anyway.
    deploy.depends += $(DESTDIR_TARGET)
    deploy.commands += $$shell_path($$[QT_HOST_BINS]/windeployqt.exe) --qmldir $$shell_quote($$shell_path($$PWD/extensions)) $(DESTDIR_TARGET)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/$$OPENSSL_SSL_LIB)) $(DESTDIR)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/$$OPENSSL_CRYPTO_LIB)) $(DESTDIR)
    sentry {
      deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$VCPKG_PATH/bin/sentry.dll)) $(DESTDIR)
      deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$VCPKG_PATH/bin/zlib1.dll)) $(DESTDIR)
      deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$VCPKG_PATH/tools/sentry-native/crashpad_handler.exe)) $(DESTDIR)
    }

    installer.depends += deploy
    installer.commands += $$shell_path($$ISCC) \
                            /DSOURCE_DIR=$$shell_quote($$shell_path($$OUT_PWD/$$DESTDIR)) \
                            /O$$shell_quote($$shell_path($$OUT_PWD)) \
                            /F$$shell_quote($${TARGET}-$${VERSION}) \
                            $$shell_quote($$shell_path($$PWD/windows/brickstore.iss))
  } else {
    deploy.CONFIG += recursive
    installer.CONFIG += recursive
  }

  QMAKE_EXTRA_TARGETS += deploy installer
}


#
# Unix specific
#

unix:!android {
  debug:OBJECTS_DIR   = $$OUT_PWD/.obj/debug
  release:OBJECTS_DIR = $$OUT_PWD/.obj/release
  debug:MOC_DIR       = $$OUT_PWD/.moc/debug
  release:MOC_DIR     = $$OUT_PWD/.moc/release
  UI_DIR              = $$OUT_PWD/.uic
  RCC_DIR             = $$OUT_PWD/.rcc

  QMAKE_CXXFLAGS *= -Wno-deprecated-declarations
}


#
# Unix/X11 specific
#

unix:!macos:!android {
  isEmpty(PREFIX):PREFIX = /usr/local
  DEFINES += INSTALL_PREFIX=\"$$PREFIX\"
  target.path = $$PREFIX/bin
  INSTALLS += target

  linux* {
    share_desktop.path   = $$PREFIX/share/applications
    share_desktop.files  = unix/brickstore.desktop
    share_mime.path      = $$PREFIX/share/mime/packages
    share_mime.files     = unix/brickstore-mime.xml
    share_appicon.path   = $$PREFIX/share/icons/hicolor/256x256/apps
    share_appicon.files  = assets/generated-app-icons/brickstore.png
    share_mimeicon.path  = $$PREFIX/share/icons/hicolor/128x128/mimetypes
    share_mimeicon.files = assets/generated-app-icons/brickstore_doc.png

    INSTALLS += share_desktop share_mime share_appicon share_mimeicon

    AppDir = $$BUILD_DIR/AppDir

    appimage.depends   = $(TARGET)
    appimage.commands  = mkdir -p $$AppDir
    appimage.commands += && install -D $$PWD/assets/generated-app-icons/brickstore_doc.png $$AppDir/share/icons/hicolor/128x128/mimetypes
    appimage.commands += && install -D $$PWD/unix/brickstore-mime.xml $$AppDir/share/mime/packages
    appimage.commands += && mkdir -p .linuxdeploy
    appimage.commands += && ( cd .linuxdeploy && wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && chmod +x ./linuxdeploy-x86_64.AppImage )
    appimage.commands += && ( cd .linuxdeploy && wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage && chmod +x ./linuxdeploy-plugin-qt-x86_64.AppImage )
    appimage.commands += && VERSION=$$VERSION QML_SOURCES_PATHS=\"$$PWD/extensions\" QMAKE=\"$$QMAKE_QMAKE\" EXTRA_QT_PLUGINS=\"svg\" \
                              ./.linuxdeploy/linuxdeploy-x86_64.AppImage --appdir=AppDir \
                              -e $$OUT_PWD/bin/brickstore \
                              -i $$PWD/assets/generated-app-icons/brickstore.png \
                              -d $$PWD/unix/brickstore.desktop \
                              --plugin qt --output appimage

    QMAKE_EXTRA_TARGETS += appimage

    package.depends = $(TARGET)
    package.commands = scripts/create-debian-changelog.sh $$VERSION > debian/changelog
    package.commands += && export QMAKE_BIN=\"$$QMAKE_QMAKE\"
    bs_backend:package.commands += && export QMAKE_EXTRA_CONFIG=\"CONFIG+=backend-only\"
    package.commands += && dpkg-buildpackage --build=binary --check-builddeps --jobs=auto --root-command=fakeroot \
                                             --unsigned-source --unsigned-changes --compression=xz
    package.commands += && mv ../brickstore*.deb .

    QMAKE_EXTRA_TARGETS += package
  }
}


#
# macOS and iOS specific
#

macos|ios {
  macos:bundle_icons.path = Contents/Resources
  macos:bundle_locversions.path = Contents/Resources

  bundle_icons.files = $$files("assets/generated-app-icons/*.icns")
  for(LANG, LANGUAGES) {
    outpath = $$OUT_PWD/.locversions/$${LANG}.lproj
    mkpath($$outpath)
    substitute("$$PWD/macos/locversion.plist.in", "$$outpath/locversion.plist")
    bundle_locversions.files += $$outpath
  }

  QMAKE_BUNDLE_DATA += bundle_icons bundle_locversions
}

ios {
  EXECUTABLE = $$TARGET
  substitute($$PWD/macos/Info.ios.plist, $$OUT_PWD/ios/Info.plist)
  QMAKE_INFO_PLIST = $$OUT_PWD/ios/Info.plist

  CONFIG(release, debug|release) {
    package.depends = $$TARGET
    package.commands = cp -a $$OUT_PWD/Release-iphoneos/$${TARGET}.app $$OUT_PWD/$${TARGET}-$${VERSION}.app
  } else {
    package.CONFIG += recursive
  }
  QMAKE_EXTRA_TARGETS += package
}

macos {
  EXECUTABLE = $$TARGET
  substitute($$PWD/macos/Info.macos.plist, $$OUT_PWD/macos/Info.plist)
  QMAKE_INFO_PLIST = $$OUT_PWD/macos/Info.plist

  LIBS += -framework SystemConfiguration

  sentry {
    bundle_sentry.path = Contents/MacOS
    bundle_sentry.files = $$VCPKG_PATH/tools/sentry-native/crashpad_handler
    QMAKE_BUNDLE_DATA += bundle_sentry
  }

  CONFIG(release, debug|release) {
    deploy.depends += $(DESTDIR_TARGET)
    deploy.commands += $$shell_path($$[QT_HOST_BINS]/macdeployqt) $$OUT_PWD/$$DESTDIR/$${TARGET}.app -qmldir=$$PWD/extensions

    installer.depends += deploy
    installer.commands += rm -rf $$OUT_PWD/dmg
    installer.commands += && mkdir $$OUT_PWD/dmg
    installer.commands += && mkdir $$OUT_PWD/dmg/.background
    installer.commands += && cp $$PWD/macos/dmg-background.png $$OUT_PWD/dmg/.background/background.png
    installer.commands += && cp $$PWD/macos/dmg-ds_store $$OUT_PWD/dmg/.DS_Store
    installer.commands += && cp -a $$OUT_PWD/$$DESTDIR/$${TARGET}.app $$OUT_PWD/dmg/
    installer.commands += && ln -s /Applications "$$OUT_PWD/dmg/"
    installer.commands += && hdiutil create \"$$OUT_PWD/$${TARGET}-$${VERSION}.dmg\" -volname \"$$TARGET $$VERSION\" -fs \"HFS+\" -srcdir \"$$OUT_PWD/dmg\" -quiet -format UDBZ -ov
  } else {
    deploy.CONFIG += recursive
    installer.CONFIG += recursive
  }

  QMAKE_EXTRA_TARGETS += deploy installer
}

DISTFILES += \
    android/AndroidManifest.xml \
    android/build.gradle \
    android/gradle.properties \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew \
    android/gradlew.bat \
    android/res/values/libs.xml

android {
  ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
  ANDROID_VERSION_CODE=$$num_add("$${VERSION_MAJOR}000000", "$${VERSION_MINOR}000", $$VERSION_PATCH)
  ANDROID_VERSION_NAME=$$VERSION

  # We expect KDAB's OpenSSL libs in $ANDROID_SDK_ROOT/android_openssl
  # cd $ANDROID_SDK_ROOT && git clone https://github.com/KDAB/android_openssl.git
  OPENSSL_PRI=$$(ANDROID_SDK_ROOT)/android_openssl/openssl.pri
  !exists($$OPENSSL_PRI):error("$$OPENSSL_PRI is missing - please clone KDAB's android_openssl into $$(ANDROID_SDK_ROOT)")
  include($$OPENSSL_PRI)

  # Mixing pre-NDK23 objects (e.g. Qt) and (post-)NDK23 objects will crash when unwinding:
  # https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md#Unwinding
  LIBS = -lunwind $$LIBS

  package.depends = apk
  package.commands = cp $$OUT_PWD/android-build/build/outputs/apk/debug/android-build-debug.apk $$OUT_PWD/$${TARGET}-$${VERSION}.apk
  QMAKE_EXTRA_TARGETS += package
}
