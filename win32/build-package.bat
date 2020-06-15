@echo off

rem Copyright (C) 2004-2020 Robert Griebl.  All rights reserved.
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

setlocal EnableDelayedExpansion

if not exist win32 (
  echo Error: this script needs to be called from the base directory
  exit /b 1
)

set /p pkg_ver= < RELEASE
rem set VSINSTALLDIR=
rem set QTDIR=

:loop
if "%~1" == "" goto continue
set arg=%1
if !arg! == /qt (
  set "QTDIR=%~2"
  shift
) else (
if !arg! == /vs (
  set "VSINSTALLDIR=%~2"
  shift
) else (
  set "pkg_ver=%~1"
))
shift
goto loop
:continue

echo QTDIR       : %QTDIR%
echo VSINSTALLDIR: %VSINSTALLDIR%

if "%pkg_ver%x" == "x" (
  echo Error: no package version supplied
  exit /b 2
)

if "%QTDIR%x" == "x" (
  echo Error: please run this command with the correct /qt ^<path/to/qt^> parameter
  exit /b 4
)

if "%VSINSTALLDIR%x" == "x" (
  echo Error: please run this command with the correct /vs ^<path/to/visual studio^> parameter
  exit /b 3
)

"%QTDIR%\bin\qmake" --version | find "Using Qt version 4." >NUL 2>NUL
if errorlevel 1 (
  echo Error: %QTDIR%\bin\qmake is not a Qt 4.x.y qmake.
  exit /b 5
)

echo.
echo Creating Windows Installer (%pkg_ver%)

echo ^> Creating Tarball...
call scripts\export-from-git.bat %pkg_ver%

echo ^> Setting up build directory...

cd win32
rmdir /s /q BUILD 2>NUL
mkdir BUILD
cd BUILD
call ..\tools\7za x ..\..\brickstore-%pkg_ver%.zip >NUL 2>NUL
cd brickstore-%pkg_ver%


echo ^> Compiling...
set "PATH=%VSINSTALLDIR%\Common7\IDE;%VSINSTALLDIR%\VC\bin;%PATH%"
set devenv=devenv.com

if not exist "%VSINSTALLDIR%\Common7\IDE\%devenv%" (
  set devenv=vcexpress.exe
)

call "%QTDIR%\bin\qmake" -tp vc -r brickstore.pro
if not "%ERRORLEVEL%" == "0" exit /b %ERRORLEVEL%
set vsconsoleoutput=1
call %devenv% brickstore.sln /Build release
if not "%ERRORLEVEL%" == "0" exit /b %ERRORLEVEL%
cd ..

echo  ^> Compiling brickstore.wxs...

call ..\tools\candle.exe -nologo ^
     -dTARGET=brickstore-%pkg_ver% ^
     -dBINARY=..\installer\Binary ^
     -dVERSION=%pkg_ver% ^
     -dQTDIR="%QTDIR%" ^
     -dVSINSTALLDIR="%VSINSTALLDIR%" ^
     ..\installer\brickstore.wxs
if not "%ERRORLEVEL%" == "0" exit /b %ERRORLEVEL%

echo  ^> Linking brickstore.msi...
call ..\tools\light.exe -ext WixUIExtension -nologo ^
     -sice:ICE08 -sice:ICE09 -sice:ICE32 -sice:ICE60 -sice:ICE61 ^
     brickstore.wixobj -out brickstore.msi
if not "%ERRORLEVEL%" == "0" exit /b %ERRORLEVEL%

echo  ^> Building sfx archive BrickStore-%pkg_ver%.exe...
call ..\tools\7za a -y -bd -ms -mx9 brickstore.7z brickstore.msi >NUL
if not "%ERRORLEVEL%" == "0" exit /b %ERRORLEVEL%
copy /B ..\installer\7zS.sfx + ..\installer\7zS.ini + brickstore.7z BrickStore-%pkg_ver%.exe >NUL
del brickstore.7z brickstore.wixobj
cd ..

echo  ^> Moving to %pkg_ver%...
if not exist ..\packages (
  mkdir ..\packages
)
if not exist ..\packages\%pkg_ver% (
  mkdir ..\packages\%pkg_ver%
)

rem move /y BUILD\brickstore.msi ..\packages\%pkg_ver%
move /y BUILD\BrickStore-%PKG_VER%.exe ..\packages\%pkg_ver%

echo ^> Cleaning build directory...
rmdir /s /q BUILD >NUL
cd ..

echo.
echo  ^> Finished
