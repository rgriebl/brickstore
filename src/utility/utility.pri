RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

QT *= core_private concurrent
macos:QT *= gui_private

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

RESOURCES += $$RELPWD/utility.qrc

HEADERS += \
    $$PWD/chunkreader.h \
    $$PWD/chunkwriter.h \
    $$PWD/currency.h \
    $$PWD/filter.h \
    $$PWD/filteredit.h \
    $$PWD/headerview.h \
    $$PWD/messagebox.h \
    $$PWD/progresscircle.h \
    $$PWD/progressdialog.h \
    $$PWD/q3cache.h \
    $$PWD/qparallelsort.h \
    $$PWD/qtemporaryresource.h \
    $$PWD/qtemporaryresource_p.h \
    $$PWD/stopwatch.h \
    $$PWD/staticpointermodel.h \
    $$PWD/threadpool.h \
    $$PWD/transfer.h \
    $$PWD/undo.h \
    $$PWD/utility.h \
    $$PWD/workspace.h \


SOURCES += \
    $$PWD/chunkreader.cpp \
    $$PWD/currency.cpp \
    $$PWD/filter.cpp \
    $$PWD/filteredit.cpp \
    $$PWD/headerview.cpp \
    $$PWD/messagebox.cpp \
    $$PWD/progresscircle.cpp \
    $$PWD/progressdialog.cpp \
    $$PWD/qtemporaryresource.cpp \
    $$PWD/staticpointermodel.cpp \
    $$PWD/threadpool.cpp \
    $$PWD/transfer.cpp \
    $$PWD/undo.cpp \
    $$PWD/utility.cpp \
    $$PWD/workspace.cpp \


macx {
#  HEADERS +=
#  OBJECTIVE_SOURCES +=
  LIBS += -framework Cocoa -framework Carbon
}
