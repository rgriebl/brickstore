RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

!backend-only {

HEADERS += \
    $$PWD/scriptmanager.h \
    $$PWD/script.h \
    $$PWD/bricklink_wrapper.h \
    $$PWD/printjob.h

SOURCES += \
    $$PWD/scriptmanager.cpp \
    $$PWD/script.cpp \
    $$PWD/bricklink_wrapper.cpp \
    $$PWD/printjob.cpp \

}
