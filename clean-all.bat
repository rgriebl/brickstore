@echo off
REM Complete clean of all build artifacts

echo Cleaning all build artifacts...
echo.

if exist CMakeCache.txt del /F /Q CMakeCache.txt
if exist cmake_install.cmake del /F /Q cmake_install.cmake
if exist compile_commands.json del /F /Q compile_commands.json
if exist Makefile del /F /Q Makefile
if exist .ninja_deps del /F /Q .ninja_deps
if exist .ninja_log del /F /Q .ninja_log
if exist build.ninja del /F /Q build.ninja
if exist rules.ninja del /F /Q rules.ninja
if exist CTestTestfile.cmake del /F /Q CTestTestfile.cmake
if exist BundleConfig.cmake del /F /Q BundleConfig.cmake

if exist CMakeFiles rd /S /Q CMakeFiles
if exist bin rd /S /Q bin
if exist generated rd /S /Q generated
if exist imports rd /S /Q imports
if exist qm rd /S /Q qm
if exist .cmake rd /S /Q .cmake
if exist _deps rd /S /Q _deps
REM DO NOT DELETE src and 3rdparty - these are source directories!

echo.
echo All build artifacts cleaned!
echo.
pause
