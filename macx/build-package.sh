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

if [ ! -d macx ]; then
    echo "Error: this script needs to be called from the base directory!"
    exit 1
fi

pkg_ver=`cat RELEASE`
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
    echo "Error: no package version supplied!"
    exit 2
fi

bundle="BrickStore.app"
arch=`uname -p`
[ "$arch" = "i386" ] && arch="intel" 
archive="BrickStore-$arch-$pkg_ver"

echo
echo "Creating Mac Os X bundle ($pkg_ver)"

echo " > Creating tarball..."
./scripts/export-from-git.sh $pkg_ver

echo " > Creating Mac Os X build directories..."
cd macx
rm -rf BUILD
mkdir BUILD
tar -xjf "../brickstore-$pkg_ver.tar.bz2" -C BUILD
builddir="BUILD/brickstore-$pkg_ver"
cd "$builddir"

echo " > Compiling..."
build_output=$(./configure --release && make -j`sysctl -n hw.ncpu`)
build_result="$?"

if [ "$build_result" != "0" ]; then
    /bin/echo -e "$build_output"
    exit $build_result
fi

cd ../..

echo " > Deploying Qt to bundle..."
macdeployqt "$builddir/src/$bundle"

( cd "$builddir/src/$bundle/Contents/" &&
  find -E . -regex "./PlugIns/qmltooling" \
        -or -regex "./PlugIns/imageformats/libq(tiff|mng|ico)\.dylib" \
        -or -regex "./PlugIns/graphicssystem" \
        -or -regex "./Frameworks/Qt(XmlPatterns|Sql|Svg|Declarative).framework" |
  xargs rm -rf )


if [ "$arch" = "intel" ]; then
    compression="-format UDBZ"
    comptype="BZ"
else
    compression="-format UDZO -imagekey zlib-level=9"
    comptype="GZ"
fi

echo " > Creating disk image $archive ($comptype)..."

mkdir BUILD/dmg
mv "$builddir/src/$bundle" BUILD/dmg
mkdir -p $pkg_ver

hdiutil create "$pkg_ver/$archive.dmg" -volname "BrickStore $pkg_ver" -fs "HFS+" -srcdir BUILD/dmg -quiet $compression
rm -rf BUILD

echo
echo " > Finished"
exit 0
