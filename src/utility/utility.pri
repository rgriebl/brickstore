RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

QT *= core-private gui-private concurrent

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
    $$PWD/chunkreader.h \
    $$PWD/chunkwriter.h \
    $$PWD/exception.h \
    $$PWD/q3cache.h \
    $$PWD/q5hashfunctions.h \
    $$PWD/stopwatch.h \
    $$PWD/systeminfo.h \
    $$PWD/transfer.h \
    $$PWD/utility.h \
    $$PWD/xmlhelpers.h

SOURCES += \
    $$PWD/chunkreader.cpp \
    $$PWD/exception.cpp \
    $$PWD/q5hashfunctions.cpp \
    $$PWD/systeminfo.cpp \
    $$PWD/transfer.cpp \
    $$PWD/utility.cpp \
    $$PWD/xmlhelpers.cpp

!backend-only {

HEADERS += \
    $$PWD/betteritemdelegate.h \
    $$PWD/currency.h \
    $$PWD/filter.h \
    $$PWD/flowlayout.h \
    $$PWD/headerview.h \
    $$PWD/historylineedit.h \
    $$PWD/humanreadabletimedelta.h \
    $$PWD/messagebox.h \
    $$PWD/progresscircle.h \
    $$PWD/progressdialog.h \
    $$PWD/qparallelsort.h \
    $$PWD/smartvalidator.h \
    $$PWD/staticpointermodel.h \
    $$PWD/undo.h \
    $$PWD/workspace.h \


SOURCES += \
    $$PWD/betteritemdelegate.cpp \
    $$PWD/currency.cpp \
    $$PWD/filter.cpp \
    $$PWD/flowlayout.cpp \
    $$PWD/headerview.cpp \
    $$PWD/historylineedit.cpp \
    $$PWD/humanreadabletimedelta.cpp \
    $$PWD/messagebox.cpp \
    $$PWD/progresscircle.cpp \
    $$PWD/progressdialog.cpp \
    $$PWD/smartvalidator.cpp \
    $$PWD/staticpointermodel.cpp \
    $$PWD/undo.cpp \
    $$PWD/workspace.cpp \

}
