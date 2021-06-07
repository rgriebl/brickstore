RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD/lzma
DEPENDPATH  += $$RELPWD/lzma

HEADERS += \
  $$PWD/lzma/bs_lzma.h \
  $$PWD/lzma/lzmadec.h \

SOURCES += \
  $$PWD/lzma/bs_lzma.cpp \
  $$PWD/lzma/lzmadec.c \

