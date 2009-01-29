win|macx|!isEmpty(QMAKE_LIBS_QT_OPENGL):QT *= opengl

RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

XSOURCES = ldraw \
           renderwidget \
           matrix_t \
           vector_t \

           
for( src, XSOURCES ) {
  HEADERS += $$RELPWD/$${src}.h
  SOURCES += $$RELPWD/$${src}.cpp
}
