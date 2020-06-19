## Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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

requires(linux|macos|win32:!winrt:!android)
!versionAtLeast(QT_VERSION, 5.11.0) {
    log("$$escape_expand(\\n\\n) *** Brickstore needs to be built against Qt 5.11.0+ ***$$escape_expand(\\n\\n)")
    CONFIG += Qt_version_needs_to_be_at_least_5_11_0
}
requires(!Qt_version_needs_to_be_at_least_5_11_0)

TEMPLATE = subdirs
CONFIG  += ordered
SUBDIRS  = src

OTHER_FILES += .github/workflows/*.yml

macos|win32 {
  deploy.CONFIG += recursive
  installer.CONFIG += recursive
  QMAKE_EXTRA_TARGETS += deploy installer
}
