RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= core gui widgets widgets-private network printsupport

HEADERS += \
    $$PWD/announcementsdialog.h \
    $$PWD/appearsinwidget.h \
    $$PWD/betteritemdelegate.h \
    $$PWD/brickstoreproxystyle.h \
    $$PWD/checkforupdates.h \
    $$PWD/desktopapplication.h \
    $$PWD/desktopuihelpers.h \
    $$PWD/developerconsole.h \
    $$PWD/documentdelegate.h \
    $$PWD/filtertermwidget.h \
    $$PWD/flowlayout.h \
    $$PWD/headerview.h \
    $$PWD/historylineedit.h \
    $$PWD/incdecpricesdialog.h \
    $$PWD/mainwindow.h \
    $$PWD/mainwindow_p.h \
    $$PWD/managecolumnlayoutsdialog.h \
    $$PWD/menucombobox.h \
    $$PWD/picturewidget.h \
    $$PWD/priceguidewidget.h \
    $$PWD/printjob.h \
    $$PWD/progresscircle.h \
    $$PWD/progressdialog.h \
    $$PWD/script.h \
    $$PWD/scriptmanager.h \
    $$PWD/selectcolor.h \
    $$PWD/selectdocumentdialog.h \
    $$PWD/selectitem.h \
    $$PWD/smartvalidator.h \
    $$PWD/taskwidgets.h \
    $$PWD/view.h \
    $$PWD/view_p.h \
    $$PWD/viewpane.h \
    $$PWD/welcomewidget.h \

SOURCES += \
    $$PWD/announcementsdialog.cpp \
    $$PWD/appearsinwidget.cpp \
    $$PWD/betteritemdelegate.cpp \
    $$PWD/brickstoreproxystyle.cpp \
    $$PWD/checkforupdates.cpp \
    $$PWD/desktopapplication.cpp \
    $$PWD/desktopuihelpers.cpp \
    $$PWD/developerconsole.cpp \
    $$PWD/documentdelegate.cpp \
    $$PWD/filtertermwidget.cpp \
    $$PWD/flowlayout.cpp \
    $$PWD/headerview.cpp \
    $$PWD/historylineedit.cpp \
    $$PWD/incdecpricesdialog.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/managecolumnlayoutsdialog.cpp \
    $$PWD/menucombobox.cpp \
    $$PWD/picturewidget.cpp \
    $$PWD/priceguidewidget.cpp \
    $$PWD/printjob.cpp \
    $$PWD/progresscircle.cpp \
    $$PWD/progressdialog.cpp \
    $$PWD/script.cpp \
    $$PWD/scriptmanager.cpp \
    $$PWD/selectcolor.cpp \
    $$PWD/selectdocumentdialog.cpp \
    $$PWD/selectitem.cpp \
    $$PWD/smartvalidator.cpp \
    $$PWD/taskwidgets.cpp \
    $$PWD/view.cpp \
    $$PWD/viewpane.cpp \
    $$PWD/welcomewidget.cpp \

macos:OBJECTIVE_SOURCES += $$PWD/desktopapplication_mac.mm

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
    $$PWD/settopriceguidedialog.ui \
    $$PWD/systeminfodialog.ui \

HEADERS += $$replace(FORMS, '\\.ui', '.h')
SOURCES += $$replace(FORMS, '\\.ui', '.cpp')
