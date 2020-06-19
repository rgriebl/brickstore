/* Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU 
** General Public License version 2 as published by the Free Software Foundation 
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#if !defined(BRICKSTORE_MAJOR) || !defined(BRICKSTORE_MINOR) || !defined(BRICKSTORE_PATCH)
#  error "You forgot to define BRICKSTORE_(MAJOR,MINOR,PATCH) before including version.h"
#endif

// stringification sucks :)
#define BS_STR(s)   BS_STR2(s)
#define BS_STR2(s)  #s

#define BRICKSTORE_VERSION   BS_STR(BRICKSTORE_MAJOR) "." BS_STR(BRICKSTORE_MINOR) "." BS_STR(BRICKSTORE_PATCH)
#define BRICKSTORE_COPYRIGHT "2004-2020 Robert Griebl"
#define BRICKSTORE_URL       "www.brickforge.de/software/brickstore"
#define BRICKSTORE_MAIL      "brickstore@brickforge.de"
#define BRICKSTORE_NAME      "Brickstore"


// Win32 FILEVERSION resource
#ifdef RC_INVOKED
#  define BRICKSTORE_VERSIONINFO  BS_STR(BRICKSTORE_MAJOR) ", " BS_STR(BRICKSTORE_MINOR) ", " BS_STR(BRICKSTORE_PATCH) ", 0"
#endif
