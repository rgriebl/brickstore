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

if ! which svn >/dev/null || [ "$#" != "3" ] || [ ! -d $1 ]; then
  echo >&2 "Usage: $0 <path to WC> <source> <destination>"
  exit 1
fi

sed <"$2" >"$3" -e s,\\\$WCREV\\\$,`svn info -R "$1" | grep Revision: | awk '{ print $2; }' | sort -un | tail -n 1`,g
