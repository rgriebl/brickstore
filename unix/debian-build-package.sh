#!/bin/sh

## Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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

if [ ! -x "`which dpkg-buildpackage`" ]; then
    echo "Error: the dpkg-buildpackage utility can not be found!"
    exit 3
fi

dist_id=$(lsb_release -i -s 2>/dev/null)
dist_code=$(lsb_release -c -s 2>/dev/null)

if [ -n "$dist_code" ]; then
    dist=$(printf "%s" "$dist_code")
elif [ -n "$dist_id" ]; then
    dist=$(printf "%s" "$dist_id")
else
    dist="unknown"
fi


echo "Creating $dist DEB package ($pkg_ver)"

echo " > Creating tarball..."
./scripts/export-from-git.sh $pkg_ver

echo " > Creating DEB build directories..."
cd unix
rm -rf BUILD
mkdir BUILD
tar -xjf "../brickstore-$pkg_ver.tar.bz2" -C BUILD
builddir="BUILD/brickstore-$pkg_ver"
cd "$builddir"

## -----------------------------------------------------

mkdir debian
for i in compat control copyright menu postinst postrm rules; do
    cp unix/deb/$i debian
done

maintainer=$(grep ^Maintainer: debian/control | sed -e 's,^.*:[ ]*,,g')
package=$(grep ^Package: debian/control | head -n1 | sed -e 's,^.*:[ ]*,,g')

cat >debian/changelog <<EOF
$package ($pkg_ver) $dist; urgency=low

  * Current Release
  
 -- $maintainer  `date -R`
   
EOF

## -----------------------------------------------------

echo " > Building package..."

chmod +x debian/rules
NUMJOBS="$(grep -s -E "^processor[[:space:]]+:" /proc/cpuinfo | wc -l)"

set +e

build_output=$(BRICKSTORE_VERSION=$pkg_ver DEB_BUILD_OPTIONS="parallel=$NUMJOBS" dpkg-buildpackage -b -D -rfakeroot -us -uc -Zxz 2>&1)
build_result=$?

set -e

if [ "$build_result" != "0" ]; then
    echo -e "$build_output"
    exit $build_result
fi

cd ../..
mkdir -p "../packages/$pkg_ver"

for i in `find BUILD -name "*.deb"`; do
    cp "$i" "../packages/$pkg_ver";
done

echo " > Cleaning DEB build directories..."
rm -rf BUILD
cd ..

echo
echo " > Finished"
exit 0
