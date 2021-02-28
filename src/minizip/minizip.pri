RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  $$PWD/unzip.h \
  $$PWD/ioapi.h \
  $$PWD/crypt.h \

SOURCES += \
  $$PWD/unzip.c \
  $$PWD/ioapi.c \

win32:HEADERS += $$PWD/iowin32.h
win32:SOURCES += $$PWD/iowin32.c

