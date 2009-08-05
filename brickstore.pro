## Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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

isEmpty( RELEASE ) {
  RELEASE    = 2.0.0
}

TEMPLATE     = app
CONFIG      *= warn_on thread qt
# CONFIG      *= modeltest
QT          *= core gui xml network script

static:QTPLUGIN  *= qjpeg qgif

TARGET            = BrickStore
unix:!macx:TARGET = brickstore

LANGUAGES    = de fr nl sl
RESOURCES   += brickstore.qrc
SUBPROJECTS  = utility bricklink ldraw lzma

modeltest:debug:SUBPROJECTS += modeltest


SOURCES += \
  appearsinwidget.cpp \
  application.cpp \
  config.cpp \
  document.cpp \
  documentdelegate.cpp \
  framework.cpp \
  itemdetailpopup.cpp \
  main.cpp \
  picturewidget.cpp \
  priceguidewidget.cpp \
  rebuilddatabase.cpp \
  report.cpp \
  reportobjects.cpp \
  selectcolor.cpp \
  selectitem.cpp \
  splash.cpp \
  taskwidgets.cpp \
  window.cpp \

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
  splash.h \
  taskwidgets.h \
  updatedatabase.h \
  window.h \


FORMS = \
  additemdialog.ui \
  consolidateitemsdialog.ui \
  importinventorydialog.ui \
  importorderdialog.ui \
  incdecpricesdialog.ui \
  incompleteitemdialog.ui \
  informationdialog.ui \
  registrationdialog.ui \
  selectcolordialog.ui \
  selectdocumentdialog.ui \
  selectitemdialog.ui \
  selectreportdialog.ui \
  settingsdialog.ui \
  settopriceguidedialog.ui \

HEADERS += $$replace(FORMS, '.ui$', '.h')
SOURCES += $$replace(FORMS, '.ui$', '.cpp')

# for(subp, SUBPROJECTS) : include($${subp}/$${subp}.pri)
# workaround until creator is able to cope with for()
include('utility/utility.pri')
include('bricklink/bricklink.pri')
include('ldraw/ldraw.pri')
include('lzma/lzma.pri')
include('modeltest/modeltest.pri')

TRANSLATIONS = $$replace(LANGUAGES, '$', '.ts')
TRANSLATIONS = $$replace(TRANSLATIONS, '^', 'translations/brickstore_')


#
# (n)make tarball
#

DISTFILES += $$SOURCES $$HEADERS $$FORMS $$RESOURCES

DISTFILES += brickstore.rc brickstore.ico brickstore_doc.ico
DISTFILES += version.h.in icon.png LICENSE.GPL brickstore.pro
for(subp, SUBPROJECTS) : DISTFILES += $${subp}/$${subp}.pri

DISTFILES += images/*.png images/*.jpg images/*.pic images/16x16/*.png images/22x22/*.png images/status/*.png images/sidebar/*.png
DISTFILES += utility/images/*.png
DISTFILES += translations/translations.xml $$TRANSLATIONS $$replace(TRANSLATIONS, .ts, .qm)
DISTFILES += print-templates/*.qs

DISTFILES += scripts/*.sh scripts/*.pl scripts/*.js
DISTFILES += unix-package/create.sh unix-package/???-create.sh unix-package/brickstore.spec unix-package/rules \ 
             unix-package/share/mimelnk/application/x-brickstore-xml.desktop \ 
             unix-package/share/icons/hicolor/48x48/mimetypes/application-x-brickstore-xml.png \ 
             unix-package/share/icons/hicolor/64x64/apps/brickstore.png \ 
             unix-package/share/mime/packages/brickstore-mime.xml \ 
             unix-package/share/applications/brickstore.desktop 
DISTFILES += unix-package/brickstore.spec
DISTFILES += unix-package/create.sh unix-package/rules
DISTFILES += win32-installer/*.wx? win32-installer/create.bat win32-installer/7zS.ini win32-installer/VsSetup.ini win32-installer/Tools/* win32-installer/Binary/*
DISTFILES += macx-bundle/create.sh macx-bundle/install-table.txt macx-bundle/*.plist macx-bundle/Resources/*.icns
for(lang, LANGUAGES) : DISTFILES += macx-bundle/Resources/$${lang}.lproj/*.plist

unix {
  #tarball.target = $$lower($$TARGET)-$$RELEASE.tar.bz2
  tarball.commands = ( rel=$(RELEASE) ; dst=$$lower($$TARGET)-\$${rel:-$$RELEASE}; \
                       rm -rf \$$dst ; \
                       mkdir \$$dst ; \
                       for i in $$DISTFILES; do \
                           j=\$$dst/`dirname \$$i`; \
                           [ -d \$$j ] || mkdir -p \$$j; \
                           cp \$$i \$$j; \
                       done ; \
                       tar -cjf \$$dst.tar.bz2 \$$dst ; \
                       rm -rf \$$dst )

  macx {
    package.commands = macx-bundle/create.sh
  } else {
    package.commands = unix-package/create.sh
  }
}

win32 {
  DISTFILES=$$replace(DISTFILES, /, \\)
  DISTFILES=$$replace(DISTFILES, ^\.\\\, ) # 7-zip doesn't like file names starting with dot-backslash

  tarball.commands = ( DEL $$lower($$TARGET)-$${RELEASE}.zip 2>NUL & win32-installer\Tools\7za a -tzip $$lower($$TARGET)-$${RELEASE}.zip $$DISTFILES )

  package.commands = win32-installer\create.bat
}

QMAKE_EXTRA_TARGETS += tarball package


#
# check key
#

exists( .private-key ) {
  win32:cat_cmd = type
  unix:cat_cmd = cat

  DEFINES += BS_REGKEY=\"$$system( $$cat_cmd .private-key )\"
} 
else {
  message( Building an OpenSource version )
}


#
# Windows specific
#

win32 {
  system( cscript.exe //B scripts\update_version.js $$RELEASE)
  
  CONFIG  += windows
  #CONFIG -= shared

  RC_FILE  = brickstore.rc

  DEFINES += __USER__=\"$$(USERNAME)\" __HOST__=\"$$(COMPUTERNAME)\"

  QMAKE_CXXFLAGS_DEBUG   += /Od /GL-
  QMAKE_CXXFLAGS_RELEASE += /O2 /GL

  win32-msvc2005 {
     DEFINES += _CRT_SECURE_NO_DEPRECATE

#    QMAKE_LFLAGS_WINDOWS += "/MANIFEST:NO"
#    QMAKE_LFLAGS_WINDOWS += "/LTCG"

     QMAKE_CXXFLAGS_DEBUG   += /EHc- /EHs- /GR-
     QMAKE_CXXFLAGS_RELEASE += /EHc- /EHs- /GR-
  }
}


#
# Unix specific
#

unix {
  system( scripts/update_version.sh $$RELEASE)

  MOC_DIR     = .moc
  UI_DIR      = .uic
  RCC_DIR     = .rcc
  OBJECTS_DIR = .obj

  DEFINES += __USER__=\"$$(USER)\" __HOST__=\"$$system( hostname )\"
}


#
# Unix/X11 specific
#

unix:!macx {
  CONFIG += x11

  isEmpty( PREFIX ):PREFIX = /usr/local
  target.path = $$PREFIX/bin
  INSTALLS += target
}


#
# Mac OS X specific
#

macx {
  CONFIG += x86
}
