RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  $$PWD/bricklink.h \
  $$PWD/bricklinkfwd.h \
  $$PWD/bricklink_setmatch.h

SOURCES += \
  $$PWD/bricklink.cpp \
  $$PWD/bricklink_data.cpp \
  $$PWD/bricklink_textimport.cpp \
  $$PWD/bricklink_priceguide.cpp \
  $$PWD/bricklink_picture.cpp \
  $$PWD/bricklink_setmatch.cpp \

!backend-only {

HEADERS += \
  $$PWD/bricklink_model.h \

SOURCES += \
  $$PWD/bricklink_model.cpp \

}
