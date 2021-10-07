RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

!win32:LIBS *= -lz

INCLUDEPATH += $$RELPWD/minizip
DEPENDPATH  += $$RELPWD/minizip

HEADERS += \
  $$PWD/minizip/crypt.h \
  $$PWD/minizip/ioapi.h \
  $$PWD/minizip/minizip.h \
  $$PWD/minizip/unzip.h \
  $$PWD/minizip/zlib_p.h \

SOURCES += \
  $$PWD/minizip/ioapi.c \
  $$PWD/minizip/minizip.cpp \
  $$PWD/minizip/unzip.c \

win32:HEADERS += $$PWD/minizip/iowin32.h
win32:SOURCES += $$PWD/minizip/iowin32.c
