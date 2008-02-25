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

if [ ! -r "brickstore.pro" ]; then
	echo "Error: this script needs to be called from the base directory!"
	exit 1
fi

pkg_ver=`awk '/^ *RELEASE *=/ { print $3; }' <brickstore.pro `
[ $# = 1 ] && pkg_ver="$1"

if [ -z $pkg_ver ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

if [ ! -r Makefile ]; then
	qmake CONFIG=release PREFIX=/usr
else
	make qmake
fi

files=`make -pn | egrep -e '^(SOURCES|HEADERS|FORMS|DIST)' | sed -e 's,[A-Z]\+ = ,,g'`

if [ -z "$files" ]; then
	echo "Error: couldn't read the DIST variable from the Makefile!"
	exit 3
fi

rm -rf .mkdist-tmp
tmpdir=".mkdist-tmp/brickstore-$pkg_ver"

if ! mkdir -p "$tmpdir"; then
	echo "Error: couldn't create directory $tmpdir!"
	exit 4
fi

for i in `ls -1 $files | sort`; do
	cp -f --parents "$i" "$tmpdir"
done

tar -cjsf brickstore-$pkg_ver.tar.bz2 -C "$tmpdir/.." "brickstore-$pkg_ver"

rm -rf .mkdist-tmp
