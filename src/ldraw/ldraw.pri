QT *= opengl

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  $$PWD/ldraw.h \
  $$PWD/renderwidget.h \

SOURCES += \
  $$PWD/ldraw.cpp \
  $$PWD/renderwidget.cpp \

