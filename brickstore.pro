## Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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


MIN_QT_VERSION = 5.11.0

NAME        = "BrickStore"
DESCRIPTION = "An offline BrickLink inventory management tool."
COPYRIGHT   = "2004-2020 Robert Griebl"
GITHUB_URL  = "github.com/rgriebl/brickstore"


##NOTE: The VERSION is set in the file "VERSION" and pre-processed in .qmake.conf


requires(linux|macos|win32:!winrt:!android)
!versionAtLeast(QT_VERSION, $$MIN_QT_VERSION) {
    error("$$escape_expand(\\n\\n) *** BrickStore needs to be built against $$MIN_QT_VERSION or higher ***$$escape_expand(\\n\\n)")
}

TEMPLATE = app

TARGET = $$NAME
unix:!macos:TARGET = $$lower($$TARGET)

CONFIG *= no_private_qt_headers_warning
CONFIG *= lrelease embed_translations
# CONFIG *= modeltest


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
  LICENSE.GPL \
  configure \
  translations/translations.xml \
  scripts/generate-assets.sh \
  print-templates/*.qs \
  debian/* \
  unix/brickstore.desktop \
  unix/brickstore-mime.xml \
  windows/brickstore.iss \

RESOURCES += brickstore.qrc

LANGUAGES = en de fr nl sl

for(l, LANGUAGES) {
  TRANSLATIONS += translations/brickstore_$${l}.ts
  qt_qm = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${l}.qm
  exists($$qt_qm):qt_translations.files += $$qt_qm
}
qt_translations.base = $$[QT_INSTALL_TRANSLATIONS]
qt_translations.prefix = i18n
RESOURCES += qt_translations


include(src/src.pri)
include(src/utility/utility.pri)
include(src/bricklink/bricklink.pri)
include(src/ldraw/ldraw.pri)
include(src/lzma/lzma.pri)
modeltest:debug:include(modeltest/modeltest.pri)

qtPrepareTool(LUPDATE, lupdate)

lupdate.commands = $$QMAKE_CD $$system_quote($$system_path($$PWD)) && $$LUPDATE $$_PRO_FILE_
QMAKE_EXTRA_TARGETS += lupdate-all


#
# Windows specific
#

win32 {
  RC_ICONS = \
    assets/generated-icons/brickstore.ico \
    assets/generated-icons/brickstore_doc.ico \

  # qmake uses these to generate a FILEVERSION record
  QMAKE_TARGET_COPYRIGHT   = "Copyright (c) $$COPYRIGHT"
  QMAKE_TARGET_COMPANY     = "https://$$GITHUB_URL"
  QMAKE_TARGET_DESCRIPTION = "$$DESCRIPTION"

  win32-msvc* {
#    QMAKE_CXXFLAGS_DEBUG   += /Od /GL-
#    QMAKE_CXXFLAGS_RELEASE += /O2 /GL
#    release:QMAKE_LFLAGS_WINDOWS += "/LTCG"
#    DEFINES += _CRT_SECURE_NO_DEPRECATE

    LIBS += user32.lib advapi32.lib wininet.lib
  }

  build_pass:CONFIG(release, debug|release) {
    ISCC="iscc.exe"
    !system(where $$ISCC >NUL) {
      INNO_PATH=$$(INNO_SETUP_PATH)
      !exists("$$INNO_PATH\\$$ISCC") {
        INNO_PATH="$$getenv(ProgramFiles(x86))\\Inno Setup 5"
        !exists("$$INNO_PATH\\$$ISCC") {
          INNO_PATH="$$getenv(ProgramFiles(x86))\\Inno Setup 6"
          !exists("$$INNO_PATH\\$$ISCC"):error("Please set %INNO_SETUP_PATH% to point to the directory containing the '$$ISCC' binary.")
        }
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

    OPENSSL="openssl.exe"
    OPENSSL_PATH="$$[QT_HOST_PREFIX]/../../Tools/OpenSSL/Win_$$TARCH/bin"
    !exists("$$OPENSSL_PATH/$$OPENSSL") {
      equals(TARCH, x64):OPENSSL_PATH="$$getenv(ProgramFiles)/OpenSSL-Win64/bin"
      equals(TARCH, x86):OPENSSL_PATH="$$getenv(ProgramFiles(x86))/OpenSSL-Win32/bin"
    }
    !exists("$$OPENSSL_PATH/$$OPENSSL"):error("Please install the matching OpenSSL version from https://slproweb.com/products/Win32OpenSSL.html.")

    OPENSSL_PATH=$$clean_path($$OPENSSL_PATH)
    log("Deploying OpenSSL libraries at: $$shell_path($$OPENSSL_PATH)")

    deploy.depends += $(DESTDIR_TARGET)
    deploy.commands += $$shell_path($$[QT_HOST_BINS]/windeployqt.exe) $(DESTDIR_TARGET)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/libcrypto-1_1$${OPENSSL_ARCH}.dll)) $(DESTDIR)
    deploy.commands += & $$QMAKE_COPY $$shell_quote($$shell_path($$OPENSSL_PATH/libssl-1_1$${OPENSSL_ARCH}.dll)) $(DESTDIR)

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

unix {
  debug:OBJECTS_DIR   = $$OUT_PWD/.obj/debug
  release:OBJECTS_DIR = $$OUT_PWD/.obj/release
  debug:MOC_DIR       = $$OUT_PWD/.moc/debug
  release:MOC_DIR     = $$OUT_PWD/.moc/release
  UI_DIR              = $$OUT_PWD/.uic
  RCC_DIR             = $$OUT_PWD/.rcc
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
    share_appicon.files  = assets/generated-icons/brickstore.png
    share_mimeicon.path  = $$PREFIX/share/icons/hicolor/128x128/mimetypes
    share_mimeicon.files = assets/generated-icons/brickstore_doc.png

    INSTALLS += share_desktop share_mime share_appicon share_mimeicon

    package.depends = $(DESTDIR_TARGET)
    package.commands = scripts/create-debian-changelog.sh > debian/changelog
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
  bundle_icons.files = $$files("assets/generated-icons/*.icns")
  bundle_printtemplates.path = Contents/Resources/print-templates
  bundle_printtemplates.files = $$files("print-templates/*.qs") \
                                $$files("print-templates/*.ui")

  bundle_locversions.path = Contents/Resources
  for(l, LANGUAGES) {
    outpath = $$OUT_PWD/.locversions/$${l}.lproj
    mkpath($$outpath)
    system(sed -e "s,@LANG@,$$l," < "$$PWD/macos/locversion.plist.in" > "$$outpath/locversion.plist")
    bundle_locversions.files += $$outpath
  }

  QMAKE_BUNDLE_DATA += bundle_icons bundle_locversions bundle_printtemplates

  CONFIG(release, debug|release) {
    deploy.depends += $(DESTDIR_TARGET)
    deploy.commands += $$shell_path($$[QT_HOST_BINS]/macdeployqt) $$OUT_PWD/$$DESTDIR/$${TARGET}.app

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
