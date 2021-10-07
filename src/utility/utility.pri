RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

QT *= core-private gui-private concurrent

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
    $$PWD/chunkreader.h \
    $$PWD/chunkwriter.h \
    $$PWD/exception.h \
    $$PWD/q3cache.h \
    $$PWD/ref.h \
    $$PWD/stopwatch.h \
    $$PWD/systeminfo.h \
    $$PWD/transfer.h \
    $$PWD/utility.h \
    $$PWD/xmlhelpers.h

SOURCES += \
    $$PWD/chunkreader.cpp \
    $$PWD/exception.cpp \
    $$PWD/ref.cpp \
    $$PWD/systeminfo.cpp \
    $$PWD/transfer.cpp \
    $$PWD/utility.cpp \
    $$PWD/xmlhelpers.cpp

versionAtLeast(QT_VERSION, 6.0.0) {

HEADERS += \
    $$PWD/q5hashfunctions.h \

SOURCES += \
    $$PWD/q5hashfunctions.cpp \

}

bs_desktop|bs_mobile {

HEADERS += \
    $$PWD/currency.h \
    $$PWD/filter.h \
    $$PWD/humanreadabletimedelta.h \
    $$PWD/qparallelsort.h \
    $$PWD/staticpointermodel.h \
    $$PWD/undo.h \


SOURCES += \
    $$PWD/currency.cpp \
    $$PWD/filter.cpp \
    $$PWD/humanreadabletimedelta.cpp \
    $$PWD/staticpointermodel.cpp \
    $$PWD/undo.cpp \

}

