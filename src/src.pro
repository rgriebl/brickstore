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

include(../release.pri)

TEMPLATE     = app
CONFIG      *= warn_on thread qt # uitools
# CONFIG      *= modeltest
QT          *= core gui xml network script scripttools printsupport uitools networkauth

static:error("ERROR: Static builds are not supported")

TARGET            = BrickStore
unix:!macx:TARGET = brickstore

LANGUAGES    = de fr nl sl
RESOURCES   += brickstore.qrc

DEFINES += BRICKSTORE_MAJOR=$$RELEASE_MAJOR BRICKSTORE_MINOR=$$RELEASE_MINOR BRICKSTORE_PATCH=$$RELEASE_PATCH
DEFINES += __USER__=\"$$RELEASE_USER\" __HOST__=\"$$RELEASE_HOST\"

SOURCES += \
  appearsinwidget.cpp \
  application.cpp \
  config.cpp \
  document.cpp \
  documentdelegate.cpp \
  framework.cpp \
  import.cpp \
  itemdetailpopup.cpp \
  main.cpp \
  picturewidget.cpp \
  priceguidewidget.cpp \
  rebuilddatabase.cpp \
  report.cpp \
  reportobjects.cpp \
  selectcolor.cpp \
  selectitem.cpp \
  taskwidgets.cpp \
  updatedatabase.cpp \
  window.cpp

HEADERS += \
  appearsinwidget.h \
  application.h \
  checkforupdates.h \
  config.h \
  document.h \
  document_p.h \
  documentdelegate.h \
  framework.h \
  import.h \
  itemdetailpopup.h \
  picturewidget.h \
  priceguidewidget.h \
  rebuilddatabase.h \
  ref.h \
  report.h \
  report_p.h \
  reportobjects.h \
  selectcolor.h \
  selectitem.h \
  taskwidgets.h \
  updatedatabase.h \
  window.h \
  version.h

FORMS = \
  additemdialog.ui \
  changecurrencydialog.ui \
  consolidateitemsdialog.ui \
  importinventorydialog.ui \
  importorderdialog.ui \
  incdecpricesdialog.ui \
  informationdialog.ui \
  selectcolordialog.ui \
  selectdocumentdialog.ui \
  selectitemdialog.ui \
  selectreportdialog.ui \
  settingsdialog.ui \
  settopriceguidedialog.ui

HEADERS += $$replace(FORMS, '\\.ui', '.h')
SOURCES += $$replace(FORMS, '\\.ui', '.cpp')

include('utility/utility.pri')
include('bricklink/bricklink.pri')
include('ldraw/ldraw.pri')
include('lzma/lzma.pri')
modeltest:debug:include('modeltest/modeltest.pri')

for(l, LANGUAGES):TRANSLATIONS += translations/brickstore_$${l}.ts

#
# Windows specific
#

win32 {
  CONFIG  += windows
  RC_FILE  = $$PWD/../win32/brickstore.rc

  win32-msvc* {
    QMAKE_CXXFLAGS_DEBUG   += /Od /GL-
    QMAKE_CXXFLAGS_RELEASE += /O2 /GL
    release:QMAKE_LFLAGS_WINDOWS += "/LTCG"
    DEFINES += _CRT_SECURE_NO_DEPRECATE

    LIBS += user32.lib advapi32.lib wininet.lib
  }
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

unix:!macx {
  CONFIG += x11

  isEmpty(PREFIX):PREFIX = /usr/local
  DEFINES += INSTALL_PREFIX=\"$$PREFIX\"
  target.path = $$PREFIX/bin
  INSTALLS += target

  linux* {
    sharedir = "$$PWD/../unix/share"
    QMS = $$replace(TRANSLATIONS, '.ts$', '.qm')

    share_desktop.path   = $$PREFIX/share/applications
    share_desktop.files  = $$sharedir/applications/brickstore.desktop
    share_mimelnk.path   = $$PREFIX/share/mimelnk/application
    share_mimelnk.files  = $$sharedir/mimelnk/application/x-brickstore-xml.desktop
    share_mimeicon.path  = $$PREFIX/share/icons/hicolor/48x48/mimetypes
    share_mimeicon.files = $$sharedir/icons/hicolor/48x48/mimetypes/application-x-brickstore-xml.png
    share_appicon.path   = $$PREFIX/share/icons/hicolor/64x64/apps
    share_appicon.files  = $$sharedir/icons/hicolor/64x64/apps/brickstore.png
    share_mime.path      = $$PREFIX/share/mime/packages
    share_mime.files     = $$sharedir/mime/packages/brickstore-mime.xml
    share_trans.path     = $$PREFIX/share/brickstore2/translations
    share_trans.files    = $$PWD/translations/translations.xml \
                           $$PWD/translations/qt_nl.qm \
                           $$replace(QMS, '^', '$$PWD/')

    INSTALLS += share_desktop share_mimelnk share_mimeicon share_appicon share_mime share_trans

    # avoid useless dependencies (and warnings from dpkg-buildpackage)
    QMAKE_LIBS_X11 -= -lXext -lX11
    QMAKE_LIBS_OPENGL -= -lGLU
  }
}


#
# Mac OS X specific
#

macx {
  LIBS += -framework SystemConfiguration
  QMS = $$replace(TRANSLATIONS, '.ts$', '.qm')

  QMAKE_INFO_PLIST = $$PWD/../macx/Info.plist
  bundle_icons.path = Contents/Resources
  bundle_icons.files = $$system(find $$PWD/../macx/Resources/ -name \'*.icns\')
  bundle_locversions.path = Contents/Resources
  bundle_locversions.files = $$system(find $$PWD/../macx/Resources/ -name \'*.lproj\')
  bundle_translations.path = Contents/Resources/translations
  bundle_translations.files = $$PWD/translations/translations.xml \
                              $$PWD/translations/qt_nl.qm \
                              $$[QT_INSTALL_TRANSLATIONS]/qt_de.qm \
                              $$[QT_INSTALL_TRANSLATIONS]/qt_fr.qm \
                              $$[QT_INSTALL_TRANSLATIONS]/qt_sl.qm \
                              $$replace(QMS, '^', '$$PWD/')
  bundle_printtemplates.path = Contents/Resources/print-templates
  bundle_printtemplates.files = $$system(find $$PWD/../print-templates -name \'*.qs\' -or -name \'*.ui\')

  QMAKE_BUNDLE_DATA += bundle_icons bundle_locversions bundle_translations bundle_printtemplates

  QMAKE_POST_LINK = "sed -i '' -e 's/@VERSION@/$$RELEASE/g' $$OUT_PWD/$${TARGET}.app/Contents/Info.plist"
}
