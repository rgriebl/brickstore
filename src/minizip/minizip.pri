RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

!win32:LIBS *= -lz

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

!backend-only {

HEADERS += \
  $$PWD/crypt.h \
  $$PWD/ioapi.h \
  $$PWD/minizip.h \
  $$PWD/unzip.h \
  $$PWD/zlib_p.h \

SOURCES += \
  $$PWD/ioapi.c \
  $$PWD/minizip.cpp \
  $$PWD/unzip.c \

win32:HEADERS += $$PWD/iowin32.h
win32:SOURCES += $$PWD/iowin32.c

}
