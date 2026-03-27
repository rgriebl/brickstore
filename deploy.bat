@echo off
REM Deploy Qt dependencies to the bin directory so the executable can run

setlocal

set QT_PATH=X:\QT\6.10.1\msvc2022_64

echo.
echo ========================================
echo Deploying Qt Dependencies
echo ========================================
echo.

if not exist "bin\BrickStore.exe" (
    echo ERROR: BrickStore.exe not found in bin directory
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

echo Running windeployqt to copy Qt dependencies...
echo.

"%QT_PATH%\bin\windeployqt.exe" ^
    --release ^
    --no-compiler-runtime ^
    --no-translations ^
    --no-opengl-sw ^
    "bin\BrickStore.exe"

if errorlevel 1 (
    echo.
    echo ERROR: Deployment failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Deployment completed successfully!
echo ========================================
echo.
echo You can now run: bin\BrickStore.exe
echo.

pause
