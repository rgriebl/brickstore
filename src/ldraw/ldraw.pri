QT *= opengl
versionAtLeast(QT_VERSION, 6.0.0):QT *= openglwidgets

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
