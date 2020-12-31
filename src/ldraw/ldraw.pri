QT *= opengl

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  $$PWD/ldraw.h \

SOURCES += \
  $$PWD/ldraw.cpp \

!backend-only {

HEADERS += \
  $$PWD/renderwidget.h \
  $$PWD/shaders.h \


SOURCES += \
  $$PWD/renderwidget.cpp \

}
