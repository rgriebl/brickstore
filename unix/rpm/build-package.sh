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

if [ ! -x "`which rpmbuild`" ]; then
    echo "Error: the rpmbuild utility can not be found!"
    exit 3
fi

dist_id=$(lsb_release -i -s 2>/dev/null)
dist_release=$(lsb_release -r -s 2>/dev/null)

if [ -n "$dist_id" -a -n "$dist_release" ]; then
    dist=$(printf "%s %s" "$dist_id" "$dist_release")
elif [ -n "$dist_id" ]; then
    dist=$(printf "%s" "$dist_id")
else
    dist="unknown"
fi

build_req="qt-devel, gcc-c++"
          
echo "Creating $dist RPM package ($pkg_ver)"

echo " > Creating tarball..."
./scripts/export-from-git.sh $pkg_Ver

echo " > Creating RPM build directories..."
cd unix
rm -rf SPECS RPMS BUILD SRPMS SOURCES
mkdir SPECS RPMS BUILD SRPMS SOURCES
cp ../brickstore-$pkg_ver.tar.xz SOURCES
cp -aH share SOURCES
cp rpm/brickstore.spec SPECS

echo " > Building package..."
build_output = $(rpmbuild -bb --quiet \
                          --define="_topdir `pwd`" \
                          --define="_brickstore_version $pkg_ver" \
                          --define="_brickstore_buildreq $build_req" \
                          SPECS/brickstore.spec 2>&1)
build_result=$?

if [ "$build_result" != "0" ]; then
    echo -e "$build_output"
    exit $build_result
fi

#rm -rf "$pkg_ver"
mkdir -p "$pkg_ver"
for i in `find RPMS -name "*.rpm"`; do
    cp "$i" "$pkg_ver";
done

echo " > Cleaning RPM build directories..."
rm -rf SPECS RPMS BUILD SRPMS SOURCES
cd ..

echo
echo " > Finished"
exit 0
