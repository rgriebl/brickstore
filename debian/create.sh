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

dist="unknown"

## Ubuntu (lsb package needed!)
if grep -sq "^DISTRIB_ID=Ubuntu\$" /etc/lsb-release; then
	dist=`grep "^DISTRIB_CODENAME" /etc/lsb-release | sed -e 's,^DISTRIB_CODENAME=\(.*\)$,\1,'`
	
## Debian
elif [ -e /etc/debian_version ]; then
	case `cat /etc/debian_version` in
		3.0) dist=woody
			;;
		3.1) dist=sarge
			;;
		4.0) dist=etch
			;;
		*/*) dist=`sed </etc/debian_version -e 's,/.*$,,'`
			;;
		sid*) dist=sid
			;;
	esac
fi

echo
echo "Creating $dist DEB package ($pkg_ver)"

echo " > Creating tarball..."
scripts/mkdist.sh "$pkg_ver"

echo " > Creating DEB build directories..."
cd debian
rm -rf tmp
mkdir tmp
tar -xjf "../brickstock-$pkg_ver.tar.bz2" -C tmp
tmpdir="tmp/brickstock-$pkg_ver"
cd "$tmpdir"

cp -aH ../../../qsa .
[ -f ../../../.private-key ] && cp -H ../../../.private-key .


## -----------------------------------------------------

cat >debian/control <<EOF
Source: brickstock
Section: x11
Priority: optional
Maintainer: Robert Griebl <rg@softforge.de>
Build-Depends: debhelper (>= 4.0.0), qt3-dev-tools (>= 3.3), libqt3-mt-dev (>= 3.3), libcurl3-dev
Standards-Version: 3.6.1

Package: brickstock
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Offline tool for BrickLink
 BrickStock is an offline tool to manage your online store on
 http://www.bricklink.com
 
EOF

cat >debian/dirs <<EOF
usr/bin
usr/share/brickstock
EOF
	
cat >debian/menu <<EOF
?package(brickstock):needs="X11" section="Apps/Net" title="BrickStock" command="/usr/bin/brickstock" icon="/usr/share/brickstock/images/icon.png"
EOF

cat >debian/changelog <<EOF
brickstock ($pkg_ver) $dist; urgency=low

  * Current Release
  
 -- Robert Griebl <rg@softforge.de>  `date -R`
   
EOF

cat >debian/copyright <<EOF
BrickStock is Copyright (C) 2005 Robert Griebl <rg@softforge.de>

BrickStock is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as 
published by the Free Software Foundation.

On Debian GNU/Linux systems, the complete text of the GNU General
Public License can be found in '/usr/share/common-licenses/GPL'.
EOF

echo >debian/compat '4'

## -----------------------------------------------------

echo " > Building package..."

chmod +x debian/rules
BRICKSTOCK_VERSION=$pkg_ver dpkg-buildpackage -b -D -rfakeroot -us -uc

cd ../..
rm -rf "$pkg_ver"
mkdir "$pkg_ver"

for i in `ls -1 tmp/*.deb`; do
	j=`basename "$i" .deb`
	cp "$i" "$pkg_ver/${j}_$dist.deb"
done

echo " > Cleaning DEB build directories..."
rm -rf tmp
cd ..

echo
echo " > Finished"
exit 0
