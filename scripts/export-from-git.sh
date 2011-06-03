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

release=`cat RELEASE`

if [ -z $release ]; then
	echo "Error: no package version supplied!"
	exit 2
fi

git archive --format tar --prefix brickstore-$release/ HEAD | xz >brickstore-$release.tar.xz
