
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
CONFIG      *= warn_on thread qt modeltest
QT          *= core gui xml network

TARGET            = BrickStore
unix:!macx:TARGET = brickstore

#TRANSLATIONS = translations/brickstore_de.ts \
#               translations/brickstore_fr.ts \
#               translations/brickstore_nl.ts

RESOURCES     = brickstore.qrc


exists( .private-key ) {
  win32:cat_cmd = type
  unix:cat_cmd = cat

  DEFINES += BS_REGKEY=\"$$system( $$cat_cmd .private-key )\"
} 
else {
  message( Building an OpenSource version )
}

win32 {
  system( cscript.exe //B scripts\update_version.js $$RELEASE)
  
  CONFIG += windows
  #CONFIG -= shared

  RC_FILE = brickstore.rc

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

unix {
  system( scripts/update_version.sh $$RELEASE)

  MOC_DIR     = .moc
  UI_DIR      = .uic
  OBJECTS_DIR = .obj

  DEFINES += __USER__=\"$$(USER)\" __HOST__=\"$$system( hostname )\"
}

unix:!macx {
  CONFIG += x11

  isEmpty( PREFIX ):PREFIX = /usr/local
  target.path = $$PREFIX/bin
  INSTALLS += target
}

macx {
  CONFIG += x86
}

win {
  SOURCES += dotnetstyle.cpp
  HEADERS += dotnetstyle.h
}

modeltest:debug {
  include(modeltest/modeltest.pri)
}

include(utility/utility.pri)
include(bricklink/bricklink.pri)
include(ldraw/ldraw.pri)
include(lzma/lzma.pri)


SOURCES += main.cpp

HEADERS += ccheckforupdates.h \
           cimport.h \
           cref.h \
           cupdatedatabase.h \
	   

XSOURCES = capplication \
           csplash \
           cconfig \
           crebuilddatabase \
           cdocument \
           cselectcolor \
           cframework \
           cpicturewidget \
           cappearsinwidget \
           cpriceguidewidget \
           ctaskwidgets \
           cselectitem \
           cwindow \


XFORMS  = additem \
          importinventory \
          importorder \
          information \
          registration \
          selectcolor \
          selectitem \
          settings \

for( src, XSOURCES ) {
  HEADERS += $${src}.h
  SOURCES += $${src}.cpp

  exists($${src}_p.h) {
    HEADERS += $${src}_p.h
  }
}

for( form, XFORMS ) {
  HEADERS += d$${form}.h
  SOURCES += d$${form}.cpp
  FORMS   += dialogs/$${form}.ui
}

DISTFILES += $$SOURCES $$HEADERS $$FORMS
DISTFILES += brickstore.pro */*.pri

res_images          = images/*.png images/*.jpg
res_images_16       = images/16x16/*.png
res_images_22       = images/22x22/*.png
res_images_status   = images/status/*.png
res_images_sidebar  = images/sidebar/*.png
res_translations    = translations/translations.xml $$TRANSLATIONS
res_print_templates = print-templates/*.qs

dist_extra          = version.h.in icon.png
dist_scripts        = scripts/*.sh scripts/*.pl scripts/*.js
dist_unix_rpm       = rpm/create.sh rpm/brickstore.spec
dist_unix_deb       = debian/create.sh debian/rules
dist_macx           = macx-bundle/create.sh macx-bundle/install-table.txt macx-bundle/*.plist macx-bundle/Resources/*.icns macx-bundle/Resources/??.lproj/*.plist
dist_win32          = win32-installer/*.wx?

DISTFILES += $$res_images $$res_images_16 $$res_images_22 $$res_images_status $$res_images_sidebar $$res_translations $$res_print_templates $$dist_extra $$dist_scripts $$dist_unix_rpm $$dist_unix_deb $$dist_macx $$dist_win32

#tarball.target = $$lower($$TARGET)-$$RELEASE.tar.bz2
tarball.commands = ( dst=$$lower($$TARGET)-$$RELEASE; \
                     rm -rf \$$dst ; \
                     mkdir \$$dst ; \
                     for i in $$DISTFILES; do \
                         j=\$$dst/`dirname \$$i`; \
                         [ -d \$$j ] || mkdir -p \$$j; \
                         cp \$$i \$$j; \
                     done ; \
                     tar -cjf \$$dst.tar.bz2 \$$dst ; \
                     rm -rf \$$dst )

QMAKE_EXTRA_TARGETS += tarball
