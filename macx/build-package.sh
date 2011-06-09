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
# this needs to be done before the macdeployqt step, since $qtdir
# will not be a real filesystem path afterwards
qtdir=$(otool -L "$builddir/src/$bundle/Contents/MacOS/BrickStore" | awk -F '/lib/QtCore.framework' '/QtCore.framework/ {  sub(/\t/, "", $1); print $1 }')
languages=$(awk '/^LANGUAGES/ { ORS=" " ; for (i = 3; i <= NF; i++) print $i }' ../src/src.pro)

for i in $languages; do
    if [ -e "$qtdir/translations/qt_$i.qm" ]; then
        cp "$qtdir/translations/qt_$i.qm" "$builddir/src/$bundle/Contents/Resources/translations"
    fi
done

macdeployqt "$builddir/src/$bundle"

( cd "$builddir/src/$bundle/Contents/" &&
  find -E . -regex "./PlugIns/qmltooling" \
        -or -regex "./PlugIns/imageformats/libq(tiff|mng|ico)\.dylib" \
        -or -regex "./PlugIns/graphicssystem" \
        -or -regex "./Frameworks/Qt(XmlPatterns|Sql|Svg|Declarative).framework" |
  xargs rm -rf )


echo " > Creating disk image $archive..."

mkdir BUILD/dmg
mkdir BUILD/dmg/.background
cp dmg-background.png BUILD/dmg/.background/background.png
ln -s /Applications "BUILD/dmg/ "
cp dmg-ds_store BUILD/dmg/.DS_Store
mv "$builddir/src/$bundle" BUILD/dmg
mkdir -p "../packages/$pkg_ver/dmg"

hdiutil create "../packages/$pkg_ver/dmg/$archive.dmg" -volname "BrickStore $pkg_ver" -fs "HFS+" -srcdir BUILD/dmg -quiet -format UDBZ -ov
rm -rf BUILD

echo
echo " > Finished"
exit 0
