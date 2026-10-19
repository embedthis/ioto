@echo off
REM
REM   make.bat -- Build Ioto on Windows from the pre-generated VS2022 projects.
REM
REM   Usage: make.bat [release|debug] [msbuild args ...]
REM
REM   Builds release unless "debug" is given. The test suite needs the debug configuration.
REM
REM   Builds the library solution, then prepares and builds each application, the same sequence
REM   the Makefile follows on Unix. Every msbuild runs through projects\windows.bat, which locates
REM   Visual Studio, selects the x64 toolchain the solutions target, and sources
REM   projects\openssl-prep.bat - so this is the one documented way to build, and the CI workflow
REM   invokes it rather than msbuild directly.
REM
REM   The solutions' configurations are "debug|windows" and "release|windows". msbuild resolves
REM   neither from a Configuration alone: it pairs the name with the host architecture and stops
REM   with "MSB4126: The specified solution configuration is invalid". Both properties are passed.
REM
setlocal
set "CONFIG=release"
if /I "%~1"=="release" goto :shift
if /I "%~1"=="debug" goto :debug
goto :build
:debug
set "CONFIG=debug"
:shift
shift
:build
call projects\windows.bat msbuild projects\vs2022\ioto.sln /p:Configuration=%CONFIG% /p:Platform=windows %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 exit /b %errorlevel%

for %%a in (blank http unit) do (
    powershell -ExecutionPolicy Bypass -File bin\prepare.ps1 -APP %%a
    if errorlevel 1 exit /b 1
    call projects\windows.bat msbuild apps\%%a\projects\vs2022\ioto-%%a.sln /p:Configuration=%CONFIG% /p:Platform=windows %1 %2 %3 %4 %5 %6 %7 %8 %9
    if errorlevel 1 exit /b 1
)
