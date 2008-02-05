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

if [ ! -d rpm ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstore.pro `
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

dist="(unknown distribution)"
          
## SuSE
if [ -r /etc/SuSE-release ]; then
	export QTDIR="/usr/lib/qt3"
	export BUILDREQ="curl-devel, qt3-devel, qt3-devel-tools, gcc-c++"
	dist="SuSE"

## Fedora
elif [ -r /etc/fedora-release ]; then
	export QTDIR="/usr/lib/qt-3.3"
	export BUILDREQ="curl-devel, qt-devel, gcc-c++"
	dist="Fedora"

## RedHat
elif [ -r /etc/redhat-release ]; then
	export QTDIR="/usr/lib/qt-3.3"
	export BUILDREQ="curl-devel, qt-devel, gcc-c++"
	dist="RedHat"
fi

echo
echo "Creating $dist RPM package ($pkg_ver)"

echo " > Creating tarball..."
scripts/mkdist.sh "$pkg_ver"

echo " > Creating RPM build directories..."
cd rpm
rm -rf SPECS RPMS BUILD SRPMS SOURCES
mkdir SPECS RPMS BUILD SRPMS SOURCES
cp ../brickstore-$pkg_ver.tar.bz2 SOURCES
cp brickstore.spec SPECS

echo " > Building package..."
( rpmbuild -bb --quiet \
           --define="_topdir `pwd`" \
           --define="_brickstore_version $pkg_ver" \
           --define="_brickstore_qtdir $QTDIR" \
           --define="_brickstore_buildreq $BUILDREQ" \
           SPECS/brickstore.spec ) >/dev/null 2>/dev/null

rm -rf "$pkg_ver"
mkdir "$pkg_ver"
for i in `find RPMS -name "*.rpm"`; do cp "$i" "$pkg_ver"; done

echo " > Cleaning RPM build directories..."
rm -rf SPECS RPMS BUILD SRPMS SOURCES
cd ..

echo
echo " > Finished"
exit 0
