RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

bs_desktop|bs_mobile {

QT *= qml quick

HEADERS += \
    $$PWD/bricklink_wrapper.h \
    $$PWD/brickstore_wrapper.h \
    $$PWD/common.h

SOURCES += \
    $$PWD/bricklink_wrapper.cpp \
    $$PWD/brickstore_wrapper.cpp \
    $$PWD/common.cpp

}
