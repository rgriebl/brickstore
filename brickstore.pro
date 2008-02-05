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
  RELEASE    = 1.1.13
}

TEMPLATE     = app
CONFIG      *= warn_on thread qt link_prl

TARGET       = brickstore

TRANSLATIONS = translations/brickstore_de.ts \
               translations/brickstore_fr.ts \
               translations/brickstore_nl.ts \
               translations/brickstore_sl.ts

res_images          = images/*.png images/*.jpg 
res_images_16       = images/16x16/*.png
res_images_22       = images/22x22/*.png
res_images_status   = images/status/*.png
res_images_sidebar  = images/sidebar/*.png
res_translations    = translations/translations.xml $$TRANSLATIONS
res_print_templates = print-templates/standard.qs

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

  DEFINES += BS_REGKEY="\"$$system( $$cat_cmd .private-key )\""
} 
else {
  message( Building an OpenSource version )
}

win32 {
  system( cscript.exe //B scripts\update_version.js $$RELEASE)

  INCLUDEPATH += $$(CURLDIR)\include
  LIBS += $$(CURLDIR)\lib\libcurl.lib
  DEFINES += CURL_STATICLIB
  RC_FILE = brickstore.rc

  DEFINES += __USER__="\"$$(USERNAME)\"" __HOST__="\"$$(COMPUTERNAME)\""

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
  
  DEFINES += __USER__="\"$$(USER)\"" __HOST__="\"$$system( hostname )\""
}

unix:!macx {
  LIBS += -lcurl

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
  osx_minor = $$system( sw_vers -productVersion | awk -F. '{ print $2; }' )

  system( test $$osx_minor -ge 4 ) {
    LIBS += -lcurl
  }
  else {
    # HACK, but we need the abs. path, since MacOS X <10.4 already has 
    # an old 2.x version in /usr/lib ...
    LIBS += /usr/local/lib/libcurl.a
  }
}

HEADERS += bricklink.h \
           cappearsinwidget.h \
           capplication.h \
           ccheckforupdates.h \
           cconfig.h \
           cdocument.h \
           cdocument_p.h \
           cfilteredit.h \
           cframework.h \
           ciconfactory.h \
           cimport.h \
           citemtypecombo.h \
           citemview.h \
           clistaction.h \
           clistview.h \
           clistview_p.h \
           cmessagebox.h \
           cmoney.h \
           cmultiprogressbar.h \
           cpicturewidget.h \
           cpriceguidewidget.h \
           cprogressdialog.h \
           crebuilddatabase.h \
           cref.h \
           creport.h \
           creportobjects.h \
           creport_p.h \
           cresource.h \
           cselectcolor.h \
           cselectitem.h \
           cspinner.h \
           csplash.h \
           ctaskpanemanager.h \
           ctaskwidgets.h \
           ctransfer.h \
           cundo.h \
           cundo_p.h \
           cupdatedatabase.h \
           curllabel.h \
           cutility.h \
           cwindow.h \
           cworkspace.h \
           lzmadec.h \
           sha1.h

SOURCES += bricklink.cpp \
           bricklink_data.cpp \
           bricklink_picture.cpp \
           bricklink_priceguide.cpp \
           bricklink_textimport.cpp \
           capplication.cpp \
           cappearsinwidget.cpp \
           cconfig.cpp \
           cdocument.cpp \
           cfilteredit.cpp \
           cframework.cpp \
           ciconfactory.cpp \
           citemview.cpp \
           clistaction.cpp \
           clistview.cpp \
           cmessagebox.cpp \
           cmoney.cpp \
           cmultiprogressbar.cpp \
           cpicturewidget.cpp \
           cpriceguidewidget.cpp \
           cprogressdialog.cpp \
           crebuilddatabase.cpp \
           cref.cpp \
           creport.cpp \
           creportobjects.cpp \
           cresource.cpp \
           cselectcolor.cpp \
           cselectitem.cpp \
           cspinner.cpp \
           csplash.cpp \
           ctaskpanemanager.cpp \
           ctaskwidgets.cpp \
           ctransfer.cpp \
           cundo.cpp \
           curllabel.cpp \
           cutility.cpp \
           cwindow.cpp \
           cworkspace.cpp \
           lzmadec.c \
           main.cpp \
           sha1.cpp

FORMS   += dlgadditem.ui \
           dlgincdecprice.ui \
           dlgincompleteitem.ui \
           dlgloadinventory.ui \
           dlgloadorder.ui \
           dlgmerge.ui \
           dlgmessage.ui \
           dlgregistration.ui \
           dlgselectreport.ui \
           dlgsettings.ui \
           dlgsettopg.ui \
           dlgsubtractitem.ui

HEADERS += dlgadditemimpl.h \
           dlgincdecpriceimpl.h \
           dlgincompleteitemimpl.h \
           dlgloadinventoryimpl.h \
           dlgloadorderimpl.h \
           dlgmergeimpl.h \
           dlgmessageimpl.h \
           dlgregistrationimpl.h \
           dlgselectreportimpl.h \
           dlgsettingsimpl.h \
           dlgsettopgimpl.h \
           dlgsubtractitemimpl.h

SOURCES += dlgadditemimpl.cpp \
           dlgincdecpriceimpl.cpp \
           dlgincompleteitemimpl.cpp \
           dlgloadinventoryimpl.cpp \
           dlgloadorderimpl.cpp \
           dlgmergeimpl.cpp \
           dlgmessageimpl.cpp \
           dlgregistrationimpl.cpp \
           dlgselectreportimpl.cpp \
           dlgsettingsimpl.cpp \
           dlgsettopgimpl.cpp \
           dlgsubtractitemimpl.cpp

unix:!macx {
  INCLUDEPATH += qsa/src/qsa
  LIBS += qsa/src/qsa/libqsa.a
}
else {
  load( qsa )
}
