RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

RESOURCES += utility.qrc

# is this really still needed?
HEADERS += $$RELPWD/disableupdates.h \
           $$RELPWD/stopwatch.h \


XSOURCES = filteredit \
           filter \
           headerview \
           messagebox \
           money \
           multiprogressbar \
           progressdialog \
           spinner \
           taskpanemanager \
           threadpool \
           transfer \
           undo \
           workspace \
           qtemporaryresource \
           utility \


for( src, XSOURCES ) {
  HEADERS += $$RELPWD/$${src}.h
  SOURCES += $$RELPWD/$${src}.cpp
  
  exists($${src}_p.h) : HEADERS += $$RELPWD/$${src}_p.h
}

#for( form, XFORMS ) {
#  exists($${form}.h) : HEADERS += $${form}.h
#  exists($${form}.cpp) : SOURCES += $${form}.cpp
#  FORMS   += $${form}.ui
#}

