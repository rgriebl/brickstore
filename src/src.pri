RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= core gui xml network # networkauth
!backend-only:QT *= widgets printsupport qml quick

linux {
  # needed for C++17 parallel mode (std::execution::par_unseq)
  CONFIG *= link_pkgconfig
  PKGCONFIG *= tbb
}

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
  $$PWD/checkforupdates.cpp \
  $$PWD/config.cpp \
  $$PWD/document.cpp \
  $$PWD/documentio.cpp \
  $$PWD/documentdelegate.cpp \
  $$PWD/framework.cpp \
  $$PWD/incdecpricesdialog.cpp \
  $$PWD/lot.cpp \
  $$PWD/managecolumnlayoutsdialog.cpp \
  $$PWD/picturewidget.cpp \
  $$PWD/priceguidewidget.cpp \
  $$PWD/selectcolor.cpp \
  $$PWD/selectdocumentdialog.cpp \
  $$PWD/selectitem.cpp \
  $$PWD/taskwidgets.cpp \
  $$PWD/updatedatabase.cpp \
  $$PWD/welcomewidget.cpp \
  $$PWD/window.cpp

HEADERS += \
  $$PWD/appearsinwidget.h \
  $$PWD/application.h \
  $$PWD/checkforupdates.h \
  $$PWD/config.h \
  $$PWD/document.h \
  $$PWD/document_p.h \
  $$PWD/documentio.h \
  $$PWD/documentdelegate.h \
  $$PWD/framework.h \
  $$PWD/incdecpricesdialog.h \
  $$PWD/lot.h \
  $$PWD/managecolumnlayoutsdialog.h \
  $$PWD/picturewidget.h \
  $$PWD/priceguidewidget.h \
  $$PWD/selectcolor.h \
  $$PWD/selectdocumentdialog.h \
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
  $$PWD/printdialog.ui \
  $$PWD/selectcolordialog.ui \
  $$PWD/selectitemdialog.ui \
  $$PWD/settingsdialog.ui \
  $$PWD/settopriceguidedialog.ui

HEADERS += $$replace(FORMS, '\\.ui', '.h')
SOURCES += $$replace(FORMS, '\\.ui', '.cpp')

}
