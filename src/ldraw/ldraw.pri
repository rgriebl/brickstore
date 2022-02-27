RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

bs_desktop|bs_mobile {

QT *= opengl

HEADERS += \
    $$PWD/glrenderer.h \
    $$PWD/library.h \
    $$PWD/part.h \
    $$PWD/renderwindow.h \

SOURCES += \
    $$PWD/glrenderer.cpp \
    $$PWD/library.cpp \
    $$PWD/part.cpp \
    $$PWD/renderwindow.cpp \

RESOURCES += \
    $$PWD/shaders.qrc \

OTHER_FILES += \
    $$PWD/shaders/phong.frag \
    $$PWD/shaders/phong_core.frag \
    $$PWD/shaders/conditional.vert \
    $$PWD/shaders/conditional_core.vert \
    $$PWD/shaders/surface.vert \
    $$PWD/shaders/surface_core.vert

}

bs_desktop {

versionAtLeast(QT_VERSION, 6.0.0):QT *= openglwidgets

HEADERS += \
    $$PWD/renderwidget.h \

SOURCES += \
    $$PWD/renderwidget.cpp \

}
