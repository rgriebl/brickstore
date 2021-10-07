RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
    $$PWD/backendapplication.h \
    $$PWD/rebuilddatabase.h \

SOURCES += \
    $$PWD/backendapplication.cpp \
    $$PWD/rebuilddatabase.cpp \
