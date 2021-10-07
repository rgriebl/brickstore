RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

clang:QMAKE_CXXFLAGS *= -fcoroutines-ts -stdlib=libc++
else:gcc:QMAKE_CXXFLAGS *= -fcoroutines

INCLUDEPATH += $$RELPWD/qcoro $$RELPWD/qcoro/core $$RELPWD/qcoro/network $$RELPWD/qcoro/dbus
DEPENDPATH  += $$RELPWD/qcoro $$RELPWD/qcoro/core $$RELPWD/qcoro/network $$RELPWD/qcoro/dbus

HEADERS += \
  $$PWD/qcoro/concepts_p.h  \
  $$PWD/qcoro/core/qcorocore.h \
  $$PWD/qcoro/core/qcorofuture.h \
  $$PWD/qcoro/core/qcoroiodevice.h \
  $$PWD/qcoro/core/qcorosignal.h \
  $$PWD/qcoro/core/qcorotimer.h \
  $$PWD/qcoro/coroutine.h \
  $$PWD/qcoro/macros_p.h \
  $$PWD/qcoro/network/qcoroabstractsocket.h \
  $$PWD/qcoro/network/qcorolocalsocket.h \
  $$PWD/qcoro/network/qcoronetwork.h \
  $$PWD/qcoro/network/qcoronetworkreply.h \
  $$PWD/qcoro/network/qcorotcpserver.h \
  $$PWD/qcoro/qcoro.h \
  $$PWD/qcoro/task.h \
  $$PWD/qcoro/waitoperationbase_p.h \

SOURCES += \
  $$PWD/qcoro/core/qcoroiodevice.cpp \
  $$PWD/qcoro/core/qcorotimer.cpp \
  $$PWD/qcoro/network/qcoroabstractsocket.cpp \
  $$PWD/qcoro/network/qcorolocalsocket.cpp \
  $$PWD/qcoro/network/qcoronetworkreply.cpp \
  $$PWD/qcoro/network/qcorotcpserver.cpp \

!ios {

HEADERS += \
  $$PWD/qcoro/core/qcoroprocess.h \

SOURCES += \
  $$PWD/qcoro/core/qcoroprocess.cpp \

}

contains(QT, dbus) {

INCLUDEPATH += $$RELPWD/qcoro/dbus
DEPENDPATH  += $$RELPWD/qcoro/dbus

HEADERS += \
  $$PWD/qcoro/dbus/qcorodbus.h \
  $$PWD/qcoro/dbus/qcorodbuspendingcall.h \
  $$PWD/qcoro/dbus/qcorodbuspendingreply.h \

SOURCES += \
  $$PWD/qcoro/dbus/qcorodbuspendingcall.cpp \

}
