RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD/qtwinextras
DEPENDPATH  += $$RELPWD/qtwinextras

QT *= gui

HEADERS += \
  $$PWD/qtwinextras/qwinevent.h \
  $$PWD/qtwinextras/qwineventfilter_p.h \
  $$PWD/qtwinextras/qwinfunctions.h \
  $$PWD/qtwinextras/qwintaskbarbutton.h \
  $$PWD/qtwinextras/qwintaskbarbutton_p.h \
  $$PWD/qtwinextras/qwintaskbarprogress.h \
  $$PWD/qtwinextras/windowsguidsdefs_p.h \
  $$PWD/qtwinextras/winshobjidl_p.h \

SOURCES += \
  $$PWD/qtwinextras/qwinevent.cpp \
  $$PWD/qtwinextras/qwineventfilter.cpp \
  $$PWD/qtwinextras/qwinfunctions.cpp \
  $$PWD/qtwinextras/qwinfunctions_p.h \
  $$PWD/qtwinextras/qwintaskbarbutton.cpp \
  $$PWD/qtwinextras/qwintaskbarprogress.cpp \
  $$PWD/qtwinextras/windowsguidsdefs.cpp \
