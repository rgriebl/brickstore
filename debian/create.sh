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

set -e

if [ ! -d rpm ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

[ -r package-version ] && pkg_ver=`cat package-version | head -n1`
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
tar -xjf "../brickstore-$pkg_ver.tar.bz2" -C tmp
tmpdir="tmp/brickstore-$pkg_ver"
cd "$tmpdir"


## -----------------------------------------------------

cat >debian/control <<EOF
Source: brickstore
Section: x11
Priority: optional
Maintainer: Robert Griebl <rg@softforge.de>
Build-Depends: debhelper (>= 4.0.0), qt3-dev-tools (>= 3.3), libqt3-mt-dev (>= 3.3), libcurl3-dev (>= 7.13)
Standards-Version: 3.6.1

Package: brickstore
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}
Description: Offline tool for BrickLink
 BrickStore is an offline tool to manage your online store on
 http://www.bricklink.com
 
EOF

cat >debian/dirs <<EOF
usr/bin
usr/share/brickstore
EOF
	
cat >debian/menu <<EOF
?package(brickstore):needs="X11" section="Apps/Net" title="BrickStore" command="/usr/bin/brickstore" icon="/usr/share/brickstore/images/icon.png"
EOF

cat >debian/changelog <<EOF
brickstore ($pkg_ver) $dist; urgency=low

  * Current Release
  
 -- Robert Griebl <rg@softforge.de>  `date -R`
   
EOF

cat >debian/copyright <<EOF
BrickStore is Copyright (C) 2005 Robert Griebl <rg@softforge.de>

BrickStore is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as 
published by the Free Software Foundation.

On Debian GNU/Linux systems, the complete text of the GNU General
Public License can be found in '/usr/share/common-licenses/GPL'.
EOF

echo >debian/compat '4'

## -----------------------------------------------------

echo " > Building package..."

# create a dummy subwcrev script which does not try
# to get the patchlevel from svn - we already know it
cat >scripts/subwcrev.sh <<-EOF
	#!/bin/sh
	
	patchlevel=`echo "$pkg_ver" | cut -d. -f3`
	cd \$1 && sed <\$2 >\$3 -e "s,\\\\\\\$WCREV\\\\\\\$,\$patchlevel,g"
EOF
chmod +x scripts/subwcrev.sh

chmod +x debian/rules
fakeroot debian/rules -s binary

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
