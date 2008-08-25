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

if [ ! -d brickstore.app ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstore.pro `
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

if ! qmake --version 2>/dev/null | grep -sq 'Using Qt version 4.4.'; then
    echo "Error: No Qt 4.4.x qmake found in PATH."
    exit 3
fi

bundle="BrickStore.app"
arch=`uname -p`
[ "$arch" = "i386" ] && arch="intel" 
archive="BrickStore-$arch-$pkg_ver"

echo
echo "Creating Mac Os X bundle ($pkg_ver)"

echo " > Creating tarball..."
[ ! -e Makefile ] && qmake
make tarball RELEASE=$pkg_ver

echo " > Creating Mac Os X build directories..."
cd macx-bundle
rm -rf tmp
mkdir tmp
tar -xjf "../brickstore-$pkg_ver.tar.bz2" -C tmp
tmpdir="tmp/brickstore-$pkg_ver"
cd "$tmpdir"

[ -f ../../../.private-key ] && cp -H ../../../.private-key .

echo -n " > Compiling..."
qmake CONFIG=release
make

echo -n " > Populating bundle.."
if [ ! -d "$bundle" ]; then
    echo "Bundle \"$bundle\" was not created by make"
    exit 3
fi
cd ../..
cat macx-bundle/install-table.txt | while read xsrc xdst xname; do
	[ -z "${xsrc}" ] && continue
	[ "${xsrc:0:1}" = "#" ] && continue

	src=`eval echo $xsrc`
	dst=`eval echo $xdst`
	name=`eval echo $xname`
	
	mkdir -p "$tmpdir/$bundle/$dst"
	cp $src "$tmpdir/$bundle/$dst/$name"
	echo -n .
done
echo

echo " > Fixing version information..."
perl -pi -e "s/%\{version\}/$pkg_ver/g" "$tmpdir/$bundle/Contents/Info.plist"

echo " > Stripping binaries in bundle..."
strip -Sx $tmpdir/$bundle/Contents/MacOS/*

if [ "$arch" = "intel" ]; then
  compression="-format UDBZ"
  comptype="BZ"
else
  compression="-format UDZO -imagekey zlib-level=9"
  comptype="GZ"
fi

echo " > Creating disk image $archive ($comptype)..."
hdiutil create "macx-bundle/$pkg_ver/$archive.dmg" -volname "BrickStore $pkg_ver" -fs "HFS+" -srcdir "$tmpdir" -quiet $compression
rm -rf "$tmpdir"

echo
echo " > Finished"
exit 0
