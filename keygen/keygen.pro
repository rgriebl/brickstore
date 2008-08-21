TEMPLATE    = app
TARGET      = keygen
QT         *= core gui
SOURCES    += keygen.cpp
RESOURCES   = keygen.qrc
win32:RC_FILE = keygen.rc

exists( ../.private-key ) {
  win32:cat_cmd = "type ..\\"
  unix:cat_cmd = "cat ../"

  DEFINES += BS_REGKEY=\"$$system( $${cat_cmd}.private-key )\"
}
else {
  error( Can not find ../.private-key )
}
