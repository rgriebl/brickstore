DEFINES += MODELTEST

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

SOURCES += \
  $$PWD/modeltest.cpp

HEADERS += \
  $$PWD/modeltest.h

