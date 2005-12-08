#!/bin/sh

## Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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

[ -r package-version ] && pkg_ver=`cat package-version | head -n1`
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

bundle="BrickStore.app"
archive="brickstore-$pkg_ver"

tmpdir="macx-bundle/$pkg_ver/tmp"

echo
echo "Creating Mac Os X bundle ($pkg_ver)"

echo -n " > Populating bundle.."
rm -rf "macx-bundle/$pkg_ver"
mkdir -p "$tmpdir/$bundle"
#cp ../README.txt "$tmpdir"

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
strip $tmpdir/$bundle/Contents/MacOS/*

echo " > Creating disk image $archive..."
hdiutil create "macx-bundle/$pkg_ver/$archive.dmg" -volname "archive" -fs "HFS+" -srcdir "$tmpdir" -format UDZO -imagekey zlib-devel=9 -quiet
rm -rf "$tmpdir"

echo
echo " > Finished"
exit 0
