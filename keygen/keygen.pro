TEMPLATE    = app
TARGET      = ../keygen 
QT         *= core

SOURCES    += keygen.cpp

gui {
    DEFINES += GUI
    QT      *= gui
}
