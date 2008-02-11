
## Copyright (C) 2004-2006 Robert Griebl.  All rights reserved.
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
QT          *= core gui xml network

TARGET       = brickstore

#TRANSLATIONS = translations/brickstore_de.ts \
#               translations/brickstore_fr.ts \
#               translations/brickstore_nl.ts

RESOURCES     = brickstore.qrc

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

MOC_DIR   = .moc
UI_DIR    = .uic

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

  OBJECTS_DIR = .obj

  DEFINES += __USER__=\"$$(USER)\" __HOST__=\"$$system( hostname )\"
}

unix:!macx {
  CONFIG += x11

  isEmpty( PREFIX ):PREFIX = /usr/local

  target.path = $$PREFIX/bin
  resources_i1.path  = $$PREFIX/share/brickstore/images
  resources_i1.files = $$res_images
  resources_i2.path  = $$PREFIX/share/brickstore/images/16x16
  resources_i2.files = $$res_images_16
  resources_i3.path  = $$PREFIX/share/brickstore/images/22x22
  resources_i3.files = $$res_images_22
  resources_i4.path  = $$PREFIX/share/brickstore/images/status
  resources_i4.files = $$res_images_status
  resources_i5.path  = $$PREFIX/share/brickstore/images/sidebar
  resources_i5.files = $$res_images_sidebar

  res_qm = $$res_translations
  res_qm ~= s/.ts/.qm/g

  resources_t1.path  = $$PREFIX/share/brickstore/translations
  resources_t1.files = $$res_qm

  resources_p1.path  = $$PREFIX/share/brickstore/print-templates
  resources_p1.files = $$res_print_templates

  # this does not work, since qmake loads the qt prl after processing this file...
  #!contains( CONFIG, shared ):resources5.extra = cp $(QTDIR)/translations/qt_de.qm translations

  INSTALLS += target resources_i1 resources_i2 resources_i3 resources_i4 resources_i5 resources_t1 resources_p1
}

macx {
  CONFIG += x86
}


XFORMS  += registration \
           information \
           importorder \
           additem


SOURCES += main.cpp \
           bricklink.cpp \
           bricklink_data.cpp \
           bricklink_textimport.cpp \
           bricklink_priceguide.cpp \
           bricklink_picture.cpp \
           lzmadec.c

HEADERS += bricklink.h \
           lzmadec.h \
           ccheckforupdates.h \
           cimport.h \
           cupdatedatabase.h \
           ctooltiphelper.h \
           cdisableupdates.h \
	   

HEADERS += capplication.h \
           cmessagebox.h \
           csplash.h \
           cmoney.h \
           cref.h \
           ctransfer.h \
           cconfig.h \
           crebuilddatabase.h \
           cprogressdialog.h \
           cdocument.h \
           cdocument_p.h \
           cselectcolor.h \
           cframework.h \
           cpicturewidget.h \
           cappearsinwidget.h \
           cpriceguidewidget.h \
           ctaskwidgets.h \
           ctaskpanemanager.h \
           cselectitem.h \
           cspinner.h \
           cutility.h \
           clocalemeasurement.h \
           cwindow.h \


SOURCES += capplication.cpp \
           cmessagebox.cpp \
           csplash.cpp \
           cmoney.cpp \
           cref.cpp \
           ctransfer.cpp \
           cconfig.cpp \
           crebuilddatabase.cpp \
           cprogressdialog.cpp \
           cdocument.cpp \
           cselectcolor.cpp \
           cframework.cpp \
           cpicturewidget.cpp \
           cappearsinwidget.cpp \
           cpriceguidewidget.cpp \
           ctaskwidgets.cpp \
           ctaskpanemanager.cpp \
           cspinner.cpp \
           cutility.cpp \
           cselectitem.cpp \
           clocalemeasurement.cpp \
           cwindow.cpp \


for( form, XFORMS ) {
  FORMS += dialogs/$${form}.ui
  HEADERS += d$${form}.h
  SOURCES += d$${form}.cpp
}
