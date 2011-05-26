## Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
##
## This file is part of BrickStore.
##
## This file may be distributed and/or modified under the terms of the GNU
## General Public License version 2 as published by the Free Software Foundation
## and appearing in the file LICENSE.GPL included in the packaging of this file.
##
## This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
## WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
## See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.

QT_TOO_OLD=$$find(QT_VERSION, "^4\\.[0-6]")
count(QT_TOO_OLD, 1) {
  error("BrickStore needs at least Qt version 4.7. You are trying to compile against Qt version " $$QT_VERSION)
}

include(release.pri)

TEMPLATE = subdirs
CONFIG  += ordered
SUBDIRS  = src

unix:tarball.commands = cd $$PWD && git archive --format tar --prefix brickstore-$${RELEASE}/ HEAD | bzip2 >$$OUT_PWD/brickstore-$${RELEASE}.tar.bz2
win32:tarball.commands = cd $$PWD && git archive --format zip -9 --prefix brickstore-$${RELEASE}/ HEAD >$$OUT_PWD/brickstore-$${RELEASE}.zip

macx:package.commands = macx/build-package.sh $$RELEASE
win32:package.commands = win32\\build-package.bat $$RELEASE
else:package.commands = unix/build-package.sh $$RELEASE

QMAKE_EXTRA_TARGETS += tarball package
