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

if [ $# != 1 ]; then
  echo "Usage: $0 <release>"
  exit 1
fi

OIFS="$IFS"
IFS="."
set -- `head -n1 $1` 
IFS="$OIFS"

cat version.h.in | sed -e "s,\(^#define BRICKSTORE_MAJOR  *\)[^ ]*$,\1$1,g" \
                       -e "s,\(^#define BRICKSTORE_MINOR  *\)[^ ]*$,\1$2,g" \
                       -e "s,\(^#define BRICKSTORE_PATCH  *\)[^ ]*$,\1$3,g" \
>version.h
