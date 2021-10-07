RELPWD = $$replace(PWD,$$_PRO_FILE_PWD_,.)

INCLUDEPATH += $$RELPWD
DEPENDPATH  += $$RELPWD

QT *= core gui xml network # networkauth

linux:!android {
  # needed for C++17 parallel mode (std::execution::par_unseq)
  CONFIG *= link_pkgconfig
  packagesExist(tbb) {
    PKGCONFIG *= tbb
    DEFINES *= BS_HAS_PARALLEL_STL
  } exists(/usr/include/tbb/tbb.h) {
    # https://bugs.archlinux.org/task/40628
    LIBS *= -ltbb
    DEFINES *= BS_HAS_PARALLEL_STL
  } else {
    message("No libtbb found: parallel STL algorithms will not be used.")
  }
}

win32 {
  #QT *= widgets
  !versionAtLeast(QT_VERSION, 6.0.0):QT *= winextras
  else:include(../3rdparty/qtwinextras.pri)

  DEFINES *= BS_HAS_PARALLEL_STL
}

OTHER_FILES += \
  $$PWD/version.h.in \

include(utility/utility.pri)
include(bricklink/bricklink.pri)
include(ldraw/ldraw.pri)
include(qmlapi/qmlapi.pri)
include(common/common.pri)
bs_mobile:include(mobile/mobile.pri)
bs_desktop:include(desktop/desktop.pri)
bs_backend:include(backend/backend.pri)
