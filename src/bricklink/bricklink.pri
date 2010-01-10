RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += \
  bricklink.h \
  bricklinkfwd.h \
  bricklink_setmatch.h \


SOURCES += \
  bricklink.cpp \
  bricklink_data.cpp \
  bricklink_textimport.cpp \
  bricklink_priceguide.cpp \
  bricklink_picture.cpp \
  bricklink_model.cpp \
  bricklink_setmatch.cpp \

