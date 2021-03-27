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


backend-only:MIN_QT_VERSION = 5.12.0
else:MIN_QT_VERSION = 5.12.0

NAME        = "BrickStore"
DESCRIPTION = "$$NAME - an offline BrickLink inventory management tool."
COPYRIGHT   = "2004-2021 Robert Griebl"
BRICKSTORE_URL = "brickforge.de/brickstore"
GITHUB_URL  = "github.com/rgriebl/brickstore"


##NOTE: The VERSION is set in the file "VERSION" and pre-processed in .qmake.conf


requires(linux|macos|win32:!winrt:!android)
!versionAtLeast(QT_VERSION, $$MIN_QT_VERSION) {
    error("$$escape_expand(\\n\\n) *** $$NAME needs to be built against $$MIN_QT_VERSION or higher ***$$escape_expand(\\n\\n)")
}

TEMPLATE = app

TARGET = $$NAME
unix:!macos:TARGET = $$lower($$TARGET)

CONFIG *= no_private_qt_headers_warning c++17
CONFIG *= lrelease embed_translations


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

DESTDIR = bin

version_subst.input  = src/version.h.in
version_subst.output = src/version.h
QMAKE_SUBSTITUTES    = version_subst

INCLUDEPATH += $$OUT_PWD/src  # for version.h

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
  translations/translations.xml \
  scripts/generate-assets.sh \
  extensions/README.md \
  extensions/*.bs.qml \
  debian/* \
  unix/brickstore.desktop \
  unix/brickstore-mime.xml \
  windows/brickstore.iss \

LANGUAGES = en de fr

for(l, LANGUAGES) {
  TRANSLATIONS += translations/brickstore_$${l}.ts
  qt_qm = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${l}.qm
  exists($$qt_qm):qt_translations.files += $$qt_qm
}
qt_translations.base = $$[QT_INSTALL_TRANSLATIONS]
qt_translations.prefix = i18n

RESOURCES = brickstore.qrc qt_translations assets/icons assets/flags

backend-only:DEFINES *= BRICKSTORE_BACKEND_ONLY

include(src/src.pri)
include(src/utility/utility.pri)
include(src/bricklink/bricklink.pri)
include(src/ldraw/ldraw.pri)
include(src/script/script.pri)
include(src/lzma/lzma.pri)
include(src/minizip/minizip.pri)

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

  win32-msvc* {
    LIBS += user32.lib advapi32.lib wininet.lib
  }

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

    contains(QMAKE_TARGET.arch, x86_64) {
      TARCH=x64
      OPENSSL_ARCH="-x64"
    } else {
      TARCH=x86
      OPENSSL_ARCH=""
    }

    OPENSSL_LIB="libssl-1_1$${OPENSSL_ARCH}.dll"
    equals(TARCH, x64) {
      OPENSSL_PATH="$$getenv(ProgramFiles)/OpenSSL-Win64/bin"
      !exists("$$OPENSSL_PATH/$$OPENSSL_LIB"):OPENSSL_PATH="$$getenv(ProgramFiles)/OpenSSL/bin"
    }
    equals(TARCH, x86) {
      OPENSSL_PATH="$$getenv(ProgramFiles(x86))/OpenSSL-Win32/bin"
      !exists("$$OPENSSL_PATH/$$OPENSSL_LIB"):OPENSSL_PATH="$$getenv(ProgramFiles(x86))/OpenSSL/bin"
    }
    message("Trying to use OpenSSL from $$OPENSSL_PATH/$$OPENSSL_LIB")

    !exists("$$OPENSSL_PATH/$$OPENSSL_LIB"):error("Please install the matching OpenSSL version from https://slproweb.com/products/Win32OpenSSL.html.")

    OPENSSL_PATH=$$clean_path($$OPENSSL_PATH)

    # The OpenSSL libs from the Qt installer require an ancient MSVC2010 C runtimes, but the
    # installer doesn't install them by default, plus there's no package for the x86 version anyway.
    # The build from slwebpro.com on the other hand are built against a recent v14 runtime, which
    # we are installing anyway.
    deploy.depends += $(DESTDIR_TARGET)
    deploy.commands += $$shell_path($$[QT_HOST_BINS]/windeployqt.exe) --qmldir $$shell_quote($$shell_path($$PWD/extensions)) $(DESTDIR_TARGET)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/libcrypto-1_1$${OPENSSL_ARCH}.dll)) $(DESTDIR)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/libssl-1_1$${OPENSSL_ARCH}.dll)) $(DESTDIR)

    installer.depends += deploy
    installer.commands += $$shell_path($$ISCC) \
                            /DSOURCE_DIR=$$shell_quote($$shell_path($$OUT_PWD/$$DESTDIR)) \
                            /DVCPKG_PATH=$$shell_quote($$shell_path($$VCPKG_PATH)) \
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

unix {
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

unix:!macos {
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
    backend-only:package.commands += && export QMAKE_EXTRA_CONFIG=\"CONFIG+=backend-only\"
    package.commands += && dpkg-buildpackage --build=binary --check-builddeps --jobs=auto --root-command=fakeroot \
                                             --unsigned-source --unsigned-changes --compression=xz
    package.commands += && mv ../brickstore*.deb .

    QMAKE_EXTRA_TARGETS += package
  }
}


#
# Mac OS X specific
#

macos {
  LIBS += -framework SystemConfiguration

  QMAKE_FULL_VERSION = $$VERSION
  QMAKE_INFO_PLIST = macos/Info.plist
  bundle_icons.path = Contents/Resources
  bundle_icons.files = $$files("assets/generated-app-icons/*.icns")
  bundle_locversions.path = Contents/Resources
  for(l, LANGUAGES) {
    outpath = $$OUT_PWD/.locversions/$${l}.lproj
    mkpath($$outpath)
    system(sed -e "s,@LANG@,$$l," < "$$PWD/macos/locversion.plist.in" > "$$outpath/locversion.plist")
    bundle_locversions.files += $$outpath
  }

  QMAKE_BUNDLE_DATA += bundle_icons bundle_locversions

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
