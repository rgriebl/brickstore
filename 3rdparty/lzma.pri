RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD/lzma
DEPENDPATH  += $$RELPWD/lzma

HEADERS += \
  $$PWD/lzma/lzmadec.h \

SOURCES += \
  $$PWD/lzma/lzmadec.c \

