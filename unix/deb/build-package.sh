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

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstore.pro `
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

if [ ! -x "`which dpkg-buildpackage`" ]; then
	echo "Error: the dpkg-buildpackage utility can not be found!"
	exit 3
fi

dist="unknown"

## Ubuntu (lsb package needed!)
if grep -sq "^DISTRIB_ID=Ubuntu\$" /etc/lsb-release; then
	dist=`grep "^DISTRIB_CODENAME" /etc/lsb-release | sed -e 's,^DISTRIB_CODENAME=\(.*\)$,\1,'`
	
## Maemo
elif [ -e /etc/maemo_version ]; then
	dist=maemo-`head -n1 /etc/maemo_version | sed -e 's, .*$,,'`

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
make tarball RELEASE=$pkg_ver

echo " > Creating DEB build directories..."
cd unix
rm -rf BUILD
mkdir BUILD
tar -xjf "../brickstore-$pkg_ver.tar.bz2" -C BUILD
builddir="BUILD/brickstore-$pkg_ver"
cd "$builddir"

## -----------------------------------------------------

mkdir debian
cp unix/deb/rules debian

cat >debian/control <<EOF
Source: brickstore
Section: x11
Priority: optional
Maintainer: Robert Griebl <rg@softforge.de>
Build-Depends: debhelper (>= 4.0.0), qt45-dev-tools (>= 4.5), libqt45-dev (>= 4.5)
Standards-Version: 3.6.1

Package: brickstore
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
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

if [ -e /etc/maemo_version ]; then
  extra_postinst_commands="maemo-select-menu-location brickstore.desktop"
fi

cat >debian/postinst <<EOF
#!/bin/sh
set -e
#DEBHELPER#
# update icon cache
touch /usr/share/icons/hicolor
$extra_postinst_commands
EOF

cat >debian/postrm <<EOF
#!/bin/sh
set -e
#DEBHELPER#
# update icon cache
touch /usr/share/icons/hicolor
EOF

echo >debian/compat '4'

## -----------------------------------------------------

echo " > Building package..."

chmod +x debian/rules
BRICKSTORE_VERSION=$pkg_ver dpkg-buildpackage -b -D -rfakeroot -us -uc

cd ../..
#rm -rf "$pkg_ver"
mkdir -p "$pkg_ver"

for i in `ls -1 tmp/*.deb`; do
	j=`basename "$i" .deb`
	cp "$i" "$pkg_ver/${j}_$dist.deb"
done

echo " > Cleaning DEB build directories..."
rm -rf BUILD
cd ..

echo
echo " > Finished"
exit 0
