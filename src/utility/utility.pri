RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

QT *= core_private concurrent
macos:!backend-only:QT *= gui_private

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
    $$PWD/chunkreader.h \
    $$PWD/chunkwriter.h \
    $$PWD/exception.h \
    $$PWD/q3cache.h \
    $$PWD/staticpointermodel.h \
    $$PWD/stopwatch.h \
    $$PWD/transfer.h \
    $$PWD/utility.h \

SOURCES += \
    $$PWD/chunkreader.cpp \
    $$PWD/staticpointermodel.cpp \
    $$PWD/transfer.cpp \
    $$PWD/utility.cpp \

!backend-only {

HEADERS += \
    $$PWD/currency.h \
    $$PWD/filter.h \
    $$PWD/headerview.h \
    $$PWD/humanreadabletimedelta.h \
    $$PWD/messagebox.h \
    $$PWD/progresscircle.h \
    $$PWD/progressdialog.h \
    $$PWD/qparallelsort.h \
    $$PWD/undo.h \
    $$PWD/workspace.h \


SOURCES += \
    $$PWD/currency.cpp \
    $$PWD/filter.cpp \
    $$PWD/headerview.cpp \
    $$PWD/humanreadabletimedelta.cpp \
    $$PWD/messagebox.cpp \
    $$PWD/progresscircle.cpp \
    $$PWD/progressdialog.cpp \
    $$PWD/undo.cpp \
    $$PWD/workspace.cpp \

}
