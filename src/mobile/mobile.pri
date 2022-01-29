RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= qml quick quick-private quicktemplates2-private quickcontrols2impl-private \
    quickdialogs2-private

HEADERS += \
    $$PWD/mobileapplication.h \
    $$PWD/mobileuihelpers.h \

SOURCES += \
    $$PWD/mobileapplication.cpp \
    $$PWD/mobileuihelpers.cpp \

QML_FILES += \
    $$PWD/qtquickcontrols2.conf \
    $$PWD/AboutDialog.qml \
    $$PWD/ActionDelegate.qml \
    $$PWD/AutoSizingMenu.qml \
    $$PWD/DocumentProxyModelJS.qml \
    $$PWD/GridCell.qml \
    $$PWD/GridHeader.qml \
    $$PWD/Main.qml \
    $$PWD/SettingsDialog.qml \
    $$PWD/View.qml \
    $$PWD/ViewEditMenu.qml \
    $$PWD/ViewHeaderMenu.qml \
    $$PWD/uihelpers/FileDialog.qml \
    $$PWD/uihelpers/InputDialog.qml \
    $$PWD/uihelpers/MessageBox.qml \
    $$PWD/uihelpers/ProgressDialog.qml \

qml.base = $$PWD
qml.prefix = "mobile"
qml.files = $$QML_FILES
RESOURCES *= qml

OTHER_FILES *= $$QMLFILES
