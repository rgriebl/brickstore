RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

HEADERS += $$RELPWD/bricklink.h \


SOURCES += $$RELPWD/bricklink.cpp \
           $$RELPWD/bricklink_data.cpp \
           $$RELPWD/bricklink_textimport.cpp \
           $$RELPWD/bricklink_priceguide.cpp \
           $$RELPWD/bricklink_picture.cpp \
           $$RELPWD/bricklink_model.cpp \
