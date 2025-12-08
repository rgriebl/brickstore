@echo off
REM BrickStore Build Script for Windows
REM Automatically sets up Visual Studio environment and builds the project

setlocal

set QT_PATH=X:\QT\6.10.1\msvc2022_64
set VS_PATH=A:\Visual Studio

echo.
echo ========================================
echo Building BrickStore
echo ========================================
echo.

REM Setup Visual Studio environment (64-bit)
echo Setting up Visual Studio 2022 environment...
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul

if errorlevel 1 (
    echo ERROR: Failed to setup Visual Studio environment
    echo Make sure Visual Studio 2022 is installed at: %VS_PATH%
    pause
    exit /b 1
)

echo Visual Studio environment configured.
echo.

REM Check if already configured
if exist "CMakeCache.txt" (
    echo CMake already configured. Building...
    echo.
    goto build
)

REM Configure the project
echo Configuring project...
call configure.bat --qmake "%QT_PATH%\bin\qmake.exe" --release

if errorlevel 1 (
    echo.
    echo ERROR: Configuration failed
    pause
    exit /b 1
)

:build
REM Build the project
echo.
echo Building...
echo.
cmake --build .

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo Executable: bin\BrickStore.exe
echo ========================================
echo.

REM Check if Qt dependencies are already deployed
if exist "bin\Qt6Core.dll" (
    echo Qt dependencies already deployed. Executable is ready to run.
    echo Run: bin\BrickStore.exe
    echo.
    echo If you need to redeploy Qt dependencies, run: deploy.bat
    echo.
    pause
) else (
    echo Qt dependencies not found. You need to deploy them first.
    echo.
    echo Would you like to deploy Qt dependencies now? (Y/N)
    set /p DEPLOY_CHOICE=
    if /i "%DEPLOY_CHOICE%"=="Y" (
        echo.
        call deploy.bat
    ) else (
        echo.
        echo Remember to run deploy.bat before running BrickStore.exe
        echo.
        pause
    )
)
