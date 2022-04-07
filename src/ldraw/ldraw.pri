RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

bs_desktop|bs_mobile {

QT *= quick3d

HEADERS += \
    $$PWD/library.h \
    $$PWD/part.h \
    $$PWD/rendercontroller.h \

SOURCES += \
    $$PWD/library.cpp \
    $$PWD/part.cpp \
    $$PWD/rendercontroller.cpp \

RESOURCES += \
    $$PWD/shaders.qrc \

OTHER_FILES += \
    $$PWD/shaders/phong.frag \
    $$PWD/shaders/phong_core.frag \
    $$PWD/shaders/conditional.vert \
    $$PWD/shaders/conditional_core.vert \
    $$PWD/shaders/surface.vert \
    $$PWD/shaders/surface_core.vert \
    $$PWD/PartRenderer.qml \

}

bs_desktop {

HEADERS += \
    $$PWD/renderwidget.h \

SOURCES += \
    $$PWD/renderwidget.cpp \

}
