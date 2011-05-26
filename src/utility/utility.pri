RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

RESOURCES += $$RELPWD/utility.qrc

HEADERS += \
    chunkreader.h \
    chunkwriter.h \
    currency.h \
    filter.h \
    filteredit.h \
    headerview.h \
    messagebox.h \
    progresscircle.h \
    progressdialog.h \
    qparallelsort.h \
    qtemporaryresource.h \
    qtemporaryresource_p.h \
    stopwatch.h \
    staticpointermodel.h \
    threadpool.h \
    transfer.h \
    undo.h \
    utility.h \
    workspace.h \
    utility/infobar.h


SOURCES += \
    chunkreader.cpp \
    currency.cpp \
    filter.cpp \
    filteredit.cpp \
    headerview.cpp \
    messagebox.cpp \
    progresscircle.cpp \
    progressdialog.cpp \
    qtemporaryresource.cpp \
    staticpointermodel.cpp \
    threadpool.cpp \
    transfer.cpp \
    undo.cpp \
    utility.cpp \
    workspace.cpp \
    utility/infobar.cpp


macx {
  HEADERS += macx.h
  OBJECTIVE_SOURCES += macx.mm
  LIBS += -framework Cocoa -framework Carbon
}
