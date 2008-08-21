RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += $$RELPWD/lzmadec.h \


SOURCES += $$RELPWD/lzmadec.c \
