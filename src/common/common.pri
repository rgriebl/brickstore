RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
    $$PWD/onlinestate.h \

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/onlinestate.cpp \

bs_desktop|bs_mobile {

HEADERS += \
    $$PWD/actionmanager.h \
    $$PWD/announcements.h \
    $$PWD/application.h \
    $$PWD/config.h \
    $$PWD/document.h \
    $$PWD/documentio.h \
    $$PWD/documentlist.h \
    $$PWD/documentmodel.h \
    $$PWD/documentmodel_p.h \
    $$PWD/recentfiles.h \
    $$PWD/uihelpers.h \

SOURCES += \
    $$PWD/actionmanager.cpp \
    $$PWD/announcements.cpp \
    $$PWD/application.cpp \
    $$PWD/config.cpp \
    $$PWD/document.cpp \
    $$PWD/documentio.cpp \
    $$PWD/documentlist.cpp \
    $$PWD/documentmodel.cpp \
    $$PWD/recentfiles.cpp \
    $$PWD/uihelpers.cpp \

}
