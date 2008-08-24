QT *= opengl

INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

XSOURCES = ldraw \
           renderwidget \
           matrix_t \
           vector_t \

           
for( src, XSOURCES ) {
  HEADERS += $$PWD/$${src}.h
  SOURCES += $$PWD/$${src}.cpp
}
