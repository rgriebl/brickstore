RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= core gui xml network # networkauth
!backend-only:QT *= widgets printsupport qml quick

win32:QT *= winextras widgets

OTHER_FILES += \
  $$PWD/version.h.in \

SOURCES += \
  $$PWD/main.cpp \
  $$PWD/rebuilddatabase.cpp \
  $$PWD/ref.cpp \

HEADERS += \
  $$PWD/rebuilddatabase.h \
  $$PWD/ref.h \

!backend-only {

SOURCES += \
  $$PWD/appearsinwidget.cpp \
  $$PWD/application.cpp \
  $$PWD/config.cpp \
  $$PWD/document.cpp \
  $$PWD/documentio.cpp \
  $$PWD/documentdelegate.cpp \
  $$PWD/framework.cpp \
  $$PWD/incdecpricesdialog.cpp \
  $$PWD/managecolumnlayoutsdialog.cpp \
  $$PWD/picturewidget.cpp \
  $$PWD/priceguidewidget.cpp \
  $$PWD/selectcolor.cpp \
  $$PWD/selectitem.cpp \
  $$PWD/taskwidgets.cpp \
  $$PWD/updatedatabase.cpp \
  $$PWD/welcomewidget.cpp \
  $$PWD/window.cpp

HEADERS += \
  $$PWD/appearsinwidget.h \
  $$PWD/application.h \
  $$PWD/config.h \
  $$PWD/document.h \
  $$PWD/document_p.h \
  $$PWD/documentio.h \
  $$PWD/documentdelegate.h \
  $$PWD/framework.h \
  $$PWD/incdecpricesdialog.h \
  $$PWD/managecolumnlayoutsdialog.h \
  $$PWD/picturewidget.h \
  $$PWD/priceguidewidget.h \
  $$PWD/selectcolor.h \
  $$PWD/selectitem.h \
  $$PWD/taskwidgets.h \
  $$PWD/updatedatabase.h \
  $$PWD/welcomewidget.h \
  $$PWD/window.h \
  $$PWD/window_p.h

FORMS = \
  $$PWD/aboutdialog.ui \
  $$PWD/additemdialog.ui \
  $$PWD/changecurrencydialog.ui \
  $$PWD/consolidateitemsdialog.ui \
  $$PWD/importinventorydialog.ui \
  $$PWD/importcartdialog.ui \
  $$PWD/importorderdialog.ui \
  $$PWD/selectcolordialog.ui \
  $$PWD/selectdocumentdialog.ui \
  $$PWD/selectitemdialog.ui \
  $$PWD/selectprintingtemplatedialog.ui \
  $$PWD/settingsdialog.ui \
  $$PWD/settopriceguidedialog.ui

HEADERS += $$replace(FORMS, '\\.ui', '.h')
SOURCES += $$replace(FORMS, '\\.ui', '.cpp')

}
