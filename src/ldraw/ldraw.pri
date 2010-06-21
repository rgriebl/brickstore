QT *= opengl

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  ldraw.h \
  renderwidget.h \

SOURCES += \
  ldraw.cpp \
  renderwidget.cpp \

