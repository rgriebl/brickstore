RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

# is this really still needed?
HEADERS += $$RELPWD/cdisableupdates.h \


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
  HEADERS += $$RELPWD/$${src}.h
  SOURCES += $$RELPWD/$${src}.cpp
  
  exists($$RELPWD/$${src}_p.h) {
    HEADERS += $$RELPWD/$${src}_p.h
  }
}


