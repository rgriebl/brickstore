INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

# is this really still needed?
HEADERS += $$PWD/cdisableupdates.h \


XSOURCES = cfilteredit \
           cmessagebox \
           cmoney \
           cmultiprogressbar \
           cprogressdialog \
           cspinner \
           ctaskpanemanager \
           cthreadpool \
           ctransfer \
           cundo \
           cworkspace \
           qtemporaryresource \
           utility \

for( src, XSOURCES ) {
  HEADERS += $$PWD/$${src}.h
  SOURCES += $$PWD/$${src}.cpp
  
  exists($$PWD/$${src}_p.h) {
    HEADERS += $$PWD/$${src}_p.h
  }
}


