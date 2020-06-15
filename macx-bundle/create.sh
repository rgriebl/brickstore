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

if [ ! -d brickstock.app ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstock.pro `
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

bundle="BrickStock.app"
arch=`uname -p`
[ "$arch" = "i386" ] && arch="intel" 
archive="BrickStock-$arch-$pkg_ver"

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

	if [ -d "${src}" ]; then
		cp -R $src "$tmpdir/$bundle/$dst/$name"
	else
    		cp $src "$tmpdir/$bundle/$dst/$name"
	fi

	echo -n .
done
echo

cp LICENSE.GPL $tmpdir/

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
hdiutil create "macx-bundle/$pkg_ver/$archive.dmg" -volname "BrickStock $pkg_ver" -fs "HFS+" -srcdir "$tmpdir" -quiet $compression
rm -rf "$tmpdir"

echo
echo " > Finished"
exit 0
