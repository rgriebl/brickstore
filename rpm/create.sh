#!/bin/sh

## Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
##
## This file is part of BrickStock.
## BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
## by Robert Griebl, Copyright (C) 2004-2008.
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

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstock.pro `
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
cp ../brickstock-$pkg_ver.tar.bz2 SOURCES
cp brickstock.spec SPECS

echo " > Building package..."
( rpmbuild -bb --quiet \
           --define="_topdir `pwd`" \
           --define="_brickstock_version $pkg_ver" \
           --define="_brickstock_qtdir $QTDIR" \
           --define="_brickstock_buildreq $BUILDREQ" \
           SPECS/brickstock.spec ) >/dev/null 2>/dev/null

rm -rf "$pkg_ver"
mkdir "$pkg_ver"
for i in `find RPMS -name "*.rpm"`; do cp "$i" "$pkg_ver"; done

echo " > Cleaning RPM build directories..."
rm -rf SPECS RPMS BUILD SRPMS SOURCES
cd ..

echo
echo " > Finished"
exit 0
