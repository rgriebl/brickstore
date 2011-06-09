rem @echo off

rem Copyright (C) 2004-2011 Robert Griebl.  All rights reserved.
rem
rem This file is part of BrickStore.
rem
rem This file may be distributed and/or modified under the terms of the GNU
rem General Public License version 2 as published by the Free Software Foundation
rem and appearing in the file LICENSE.GPL included in the packaging of this file.
rem
rem This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
rem WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
rem
rem See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.

if not exist win32 (
  echo Error: this script needs to be called from the base directory
  exit /b 1
)

set /p pkg_ver=<RELEASE
if not "x%1" == "x" set pkg_ver=%1

if "x%pkg_ver%" == "x" (
  echo Error: no package version supplied
  exit /b 2
)

calL git archive --format zip -9 --prefix brickstore-%pkg_ver%/ HEAD >brickstore-%pkg_ver%.zip
goto :EOF
