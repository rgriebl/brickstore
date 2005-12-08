@ECHO off

REM Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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

IF NOT EXIST win32-installer (
  ECHO Error: this script needs to be called from the base directory
  EXIT /B 1
)

SET PKG_VER=

IF EXIST package-version (FOR /F %%V IN (package-version) DO SET PKG_VER=%%V)
IF NOT "x%1" == "x" SET PKG_VER=%1

IF "x%PKG_VER%" == "x" (
  ECHO Error: no package version supplied
  EXIT /B 2
)

CD win32-installer

ECHO.
ECHO Creating Windows Installer (%PKG_VER%)

ECHO  ^> Compiling brickstore.wxs...
Tools\candle.exe -nologo -dTARGET=.. -dVERSION=%PKG_VER% brickstore.wxs

ECHO  ^> Linking brickstore.msi...
Tools\light.exe -nologo brickstore.wixobj -out brickstore.msi
DEL brickstore.wixobj

ECHO  ^> Moving to %PKG_VER%...
IF NOT EXIST %PKG_VER% ( 
  MKDIR %PKG_VER% 
) ELSE ( 
  DEL /F /Q %PKG_VER%\*
)
MOVE /Y brickstore.msi %PKG_VER%

ECHO  ^> Copying vssetup to %PKG_VER%...
COPY Binary\vssetup.exe %PKG_VER%\Setup.exe >NUL
COPY vssetup.ini %PKG_VER%\Setup.ini >NUL

ECHO  ^> Building sfx archive BrickStore-%PKG_VER%.exe...
CD %PKG_VER%
..\Tools\7za a -y -bd -ms -mx9 brickstore.7z Setup.exe Setup.ini brickstore.msi >NUL
COPY /B ..\Binary\7zS.sfx + ..\7zS.ini + brickstore.7z BrickStore-%PKG_VER%.exe >NUL
DEL Setup.exe Setup.ini brickstore.7z
CD ..
CD ..

ECHO.
ECHO  ^> Finished
