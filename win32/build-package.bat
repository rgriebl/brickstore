@ECHO off

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

SET PKG_VER=<RELEASE

IF NOT "x%1" == "x" SET PKG_VER=%1
IF "x%PKG_VER%" == "x" (
  ECHO Error: no package version supplied
  EXIT /B 2
)

IF "x%VCINSTALLDIR%" == "x" (
  ECHO Error: please run this command from a Visual Studio Command Prompt
  EXIT /B 3
)

QMAKE.EXE --version | FIND "Using Qt version 4.6." >NUL 2>NUL
IF ERRORLEVEL 1 (
  ECHO Error: No Qt 4.6.x qmake found in PATH.
  EXIT /B 4
)

ECHO.
ECHO Creating Windows Installer (%PKG_VER%)

ECHO ^> Creating Tarball...
QMAKE.EXE brickstore.pro >NUL
NMAKE.EXE tarball RELEASE=%PKG_VER% >NUL 2>NUL


ECHO ^> Setting up build directory...

CD win32
RMDIR /S /Q BUILD 2>NUL
MKDIR BUILD
CD BUILD
..\tools\7za x ..\..\brickstore-%PKG_VER%.zip >NUL 2>NUL


ECHO ^> Compiling...
QMAKE.EXE -tp vc brickstore.pro >NUL
"%VCINSTALLDIR%\VCPackages\vcbuild.exe" /r /nologo /nohtmllog /M2 brickstore.vcproj "Release|Win32"


ECHO  ^> Compiling brickstore.wxs...
FOR /F "tokens=*" %%Q IN ('QMAKE.EXE -query QT_INSTALL_PREFIX') DO SET QTDIR=%%Q
..\tools\candle.exe -nologo -dTARGET=. -dBINARY=..\Binary -dVERSION=%PKG_VER% ..\installer\brickstore.wxs

ECHO  ^> Linking brickstore.msi...
..\tools\light.exe -nologo brickstore.wixobj -out brickstore.msi

CD ..


ECHO  ^> Moving to %PKG_VER%...
RMDIR /S /Q %PKG_VER% 2>NUL
MKDIR %PKG_VER%
MOVE /Y BUILD\brickstore.msi %PKG_VER%

ECHO  ^> Copying vssetup to %PKG_VER%...
COPY Binary\vssetup.exe %PKG_VER%\Setup.exe >NUL
COPY vssetup.ini %PKG_VER%\Setup.ini >NUL

ECHO  ^> Building sfx archive BrickStore-%PKG_VER%.exe...
CD %PKG_VER%
..\tools\7za a -y -bd -ms -mx9 brickstore.7z Setup.exe Setup.ini brickstore.msi >NUL
COPY /B ..\Binary\7zS.sfx + ..\7zS.ini + brickstore.7z BrickStore-%PKG_VER%.exe >NUL
DEL Setup.exe Setup.ini brickstore.7z
CD ..


ECHO ^> Cleaning build directory...
RMDIR /S /Q BUILD >NUL
CD ..

ECHO.
ECHO  ^> Finished
