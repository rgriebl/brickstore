RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= core gui xml network

HEADERS += \
  $$PWD/category.h \
  $$PWD/changelogentry.h \
  $$PWD/color.h \
  $$PWD/core.h \
  $$PWD/database.h \
  $$PWD/global.h \
  $$PWD/item.h \
  $$PWD/itemtype.h \
  $$PWD/lot.h \
  $$PWD/partcolorcode.h \
  $$PWD/picture.h \
  $$PWD/priceguide.h \
  $$PWD/textimport.h \

SOURCES += \
  $$PWD/category.cpp \
  $$PWD/changelogentry.cpp \
  $$PWD/color.cpp \
  $$PWD/core.cpp \
  $$PWD/database.cpp \
  $$PWD/item.cpp \
  $$PWD/itemtype.cpp \
  $$PWD/lot.cpp \
  $$PWD/partcolorcode.cpp \
  $$PWD/picture.cpp \
  $$PWD/priceguide.cpp \
  $$PWD/textimport.cpp \

bs_mobile|bs_desktop {

HEADERS += \
    $$PWD/cart.h \
    $$PWD/io.h \
    $$PWD/model.h \
    $$PWD/order.h \
    $$PWD/store.h \

SOURCES += \
    $$PWD/cart.cpp \
    $$PWD/io.cpp \
    $$PWD/model.cpp \
    $$PWD/order.cpp \
    $$PWD/store.cpp \

}

bs_desktop {

QT *= widgets

HEADERS += \
    $$PWD/delegate.h

SOURCES += \
    $$PWD/delegate.cpp

}
