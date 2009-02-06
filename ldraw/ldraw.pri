win|macx|!isEmpty(QMAKE_LIBS_QT_OPENGL):QT *= opengl

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  ldraw.h \
  renderwidget.h \
  matrix_t.h \
  vector_t.h \

SOURCES += \
  ldraw.cpp \
  renderwidget.cpp \
  matrix_t.cpp \
  vector_t.cpp \

