RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

bs_desktop|bs_mobile:QT *= opengl

HEADERS += \
    $$PWD/glrenderer.h \
    $$PWD/ldraw.h \
    $$PWD/renderwindow.h

SOURCES += \
    $$PWD/glrenderer.cpp \
    $$PWD/ldraw.cpp \
    $$PWD/renderwindow.cpp

bs_desktop {

versionAtLeast(QT_VERSION, 6.0.0):QT *= openglwidgets

HEADERS += \
    $$PWD/renderwidget.h \


SOURCES += \
    $$PWD/renderwidget.cpp \

RESOURCES += \
    $$PWD/shaders.qrc

}

OTHER_FILES += \
    $$PWD/shaders/phong.frag \
    $$PWD/shaders/phong_core.frag \
    $$PWD/shaders/phong.vert \
    $$PWD/shaders/phong_core.vert
    $$PWD/shaders/conditional.vert
    $$PWD/shaders/conditional_core.vert
