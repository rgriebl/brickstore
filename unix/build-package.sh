#!/bin/sh

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

set -e

if [ ! -d unix ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

pkg_ver=`cat RELEASE`
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

if [ -x "`which rpmquery`" ]; then
	echo "** Detected a RPM based distro **"
	echo
	`dirname $0`/rpm/build-package.sh "$@"
elif [ -x "`which dpkg-query`" ]; then
	echo "** Detected a DEB based distro **"
	echo
	`dirname $0`/deb/build-package.sh "$@"
else
	echo "** You are running neither a RPM nor a DEB based distribution **"
	echo
	echo "No package will be built."
	exit 1
fi
