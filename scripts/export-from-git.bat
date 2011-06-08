Rem @ECHO off

REM Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
REM
REM This file is part of BrickStore.
REM
REM This file may be distributed and/or modified under the terms of the GNU 
REM General Public License version 2 as published by the Free Software Foundation 
REM and appearing in the file LICENSE.GPL included in the packaging of this file.
REM
REM This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
REM WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
REM
REM See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.

IF NOT EXIST win32 (
  ECHO Error: this script needs to be called from the base directory
  EXIT /B 1
)

SET /P PKG_VER=<RELEASE
IF NOT "x%1" == "x" SET PKG_VER=%1

IF "x%PKG_VER%" == "x" (
  ECHO Error: no package version supplied
  EXIT /B 2
)

CALL git archive --format zip -9 --prefix brickstore-%PKG_VER%/ HEAD >brickstore-%PKG_VER%.zip
GOTO :EOF
