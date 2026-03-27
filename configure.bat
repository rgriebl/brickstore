@echo off
REM Windows version of the configure script
REM Based on the original bash configure script

setlocal EnableDelayedExpansion

set "QMAKE_BIN="
set "QT_CMAKE_BIN="
set "BUILD_TYPE="
set "BACKEND_ONLY="

REM Parse command line arguments
:parse_args
if "%~1"=="" goto check_deps
if /i "%~1"=="--qmake" (
    set "QMAKE_BIN=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--debug" (
    set "BUILD_TYPE=-DCMAKE_BUILD_TYPE=Debug"
    shift
    goto parse_args
)
if /i "%~1"=="--release" (
    set "BUILD_TYPE=-DCMAKE_BUILD_TYPE=Release"
    shift
    goto parse_args
)
if /i "%~1"=="--backend-only" (
    set "BACKEND_ONLY=-DBACKEND_ONLY=TRUE"
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help

echo Unknown option: %~1
goto show_help

:show_help
echo Usage: configure.bat [options]
echo   --qmake ^<path^>     Path to qmake.exe (e.g., X:\QT\6.10.1\msvc2022_64\bin\qmake.exe)
echo   --release           Build in Release mode
echo   --debug             Build in Debug mode
echo   --backend-only      Build backend only
echo   --help              Show this help
exit /b 1

:check_deps
REM Check for cmake
echo Checking for cmake...
where cmake >nul 2>&1
if errorlevel 1 (
    echo FAIL: Could not find cmake in PATH
    echo Please install CMake and add it to your PATH
    pause
    exit /b 2
)
echo   cmake found

REM Check for ninja
echo Checking for ninja...
where ninja >nul 2>&1
if errorlevel 1 (
    echo FAIL: Could not find ninja in PATH
    echo Ninja is usually included with Qt or Visual Studio
    echo You can also download it from: https://github.com/ninja-build/ninja/releases
    pause
    exit /b 2
)
echo   ninja found

REM If qmake not specified, try to find it
if "%QMAKE_BIN%"=="" (
    where qmake >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%i in ('where qmake') do set "QMAKE_BIN=%%i"
    )
)

if "%QMAKE_BIN%"=="" (
    echo FAIL: Could not find qmake
    echo Please specify qmake location with --qmake option
    echo Example: configure.bat --qmake X:\QT\6.10.1\msvc2022_64\bin\qmake.exe
    exit /b 2
)

REM Check if qmake exists
echo Checking qmake at: %QMAKE_BIN%
if not exist "%QMAKE_BIN%" (
    echo FAIL: qmake not found at: %QMAKE_BIN%
    pause
    exit /b 2
)
echo   qmake found

REM Check Qt version
echo Checking Qt version...
"%QMAKE_BIN%" -query QT_VERSION > qt_version.tmp 2>&1
if errorlevel 1 (
    echo FAIL: Could not query Qt version from qmake
    echo Command: "%QMAKE_BIN%" -query QT_VERSION
    type qt_version.tmp
    del qt_version.tmp
    pause
    exit /b 3
)

set /p QT_VERSION=<qt_version.tmp
del qt_version.tmp

REM Trim whitespace
for /f "tokens=* delims= " %%a in ("%QT_VERSION%") do set "QT_VERSION=%%a"

if "%QT_VERSION%"=="" (
    echo FAIL: Could not determine Qt version
    pause
    exit /b 3
)

echo   Qt version: %QT_VERSION% - OK

REM Find qt-cmake next to qmake
echo Locating qt-cmake...
for %%i in ("%QMAKE_BIN%") do set "QT_BIN_DIR=%%~dpi"
set "QT_CMAKE_BIN=%QT_BIN_DIR%qt-cmake.bat"

if not exist "%QT_CMAKE_BIN%" (
    echo FAIL: Could not find qt-cmake.bat next to qmake
    echo Expected at: %QT_CMAKE_BIN%
    pause
    exit /b 2
)
echo   qt-cmake found

REM Run qt-cmake
echo.
echo Running qt-cmake with the following options:
echo   Qt version: %QT_VERSION%
echo   qmake: %QMAKE_BIN%
echo   qt-cmake: %QT_CMAKE_BIN%
echo   Build type: %BUILD_TYPE%
echo   Backend only: %BACKEND_ONLY%
echo.

REM Get Qt install prefix from qmake
for /f "delims=" %%i in ('"%QMAKE_BIN%" -query QT_INSTALL_PREFIX') do set "QT_INSTALL_PREFIX=%%i"
for /f "tokens=* delims= " %%a in ("%QT_INSTALL_PREFIX%") do set "QT_INSTALL_PREFIX=%%a"

echo   Qt install prefix: %QT_INSTALL_PREFIX%

REM Use regular cmake with Qt6_DIR instead of qt-cmake wrapper
cmake -S . -B . ^
    -G Ninja ^
    -DCMAKE_PREFIX_PATH=%QT_INSTALL_PREFIX% ^
    -DQt6_DIR=%QT_INSTALL_PREFIX%/lib/cmake/Qt6 ^
    %BUILD_TYPE% %BACKEND_ONLY%

if errorlevel 1 (
    echo.
    echo ========================================
    echo FAIL: Configuration failed
    echo ========================================
    echo Please check the error messages above.
    echo.
    pause
    exit /b 1
)

echo.
echo Configuration successful!
echo Now run: cmake --build .
echo.

endlocal
